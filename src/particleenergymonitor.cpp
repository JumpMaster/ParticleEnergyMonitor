#include "papertrail.h"
#include "mqtt.h"
#include "application.h"
#include "particleenergymonitor.h"
#include "Adafruit_BME280.h"
#include "Adafruit_Sensor.h"
#include "secrets.h"
#include "DiagnosticsHelperRK.h"

//  Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);

double temperature = UNSET;
int humidity = UNSET;
int pressure = UNSET;
volatile int powerWatts = UNSET;
bool bmePresent = false;

volatile unsigned long lastWattUsedTime = 0;

unsigned long nextEnviornmentReading = 0;
unsigned long nextUpdateTime = 0;
unsigned long resetTime = 0;

MQTT mqttClient(mqttServer, 1883, mqttCallback);
unsigned long lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout = 5000;
retained uint32_t lastHardResetTime;
retained int resetCount;

Adafruit_BME280 bme; // I2C

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort,
  "Energy-Monitor", System.deviceID(),
  LOG_LEVEL_NONE, {
  { "app", LOG_LEVEL_ALL }
  // TOO MUCH!!! { “system”, LOG_LEVEL_ALL },
  // TOO MUCH!!! { “comm”, LOG_LEVEL_ALL }
});

ApplicationWatchdog wd(60000, System.reset);

void lightSensorISR() {

  unsigned long currenttime = millis();
  if (lastWattUsedTime > 0) {
    powerWatts = 3600000 / (currenttime - lastWattUsedTime);

    // power_count++;
    // avg_power += (power - avg_power) / power_count;
  }
  lastWattUsedTime = currenttime;
}

void sendData() {

    if (mqttClient.isConnected()) {
        char charPower[5];
        char charTemperature[6];
        char charHumidity[6];
        char charPressure[7];
        snprintf(charPower, sizeof(charPower), "%d", powerWatts);
        snprintf(charTemperature, sizeof(charTemperature), "%.1f", temperature);
        snprintf(charHumidity, sizeof(charHumidity), "%d", humidity);
        snprintf(charPressure, sizeof(charPressure), "%d", pressure);
        
        mqttClient.publish("emon/particle/power", charPower, true);
        mqttClient.publish("emon/particle/temperature", charTemperature, true);
        mqttClient.publish("emon/particle/humidity", charHumidity, true);
        mqttClient.publish("emon/particle/pressure", charPressure, true);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
}

void random_seed_from_cloud(unsigned seed) {
   srand(seed);
}

void connectToMQTT() {
    lastMqttConnectAttempt = millis();
    bool mqttConnected = mqttClient.connect(System.deviceID(), mqttUsername, mqttPassword);
    if (mqttConnected) {
        Log.info("MQTT Connected");
    } else
        Log.info("MQTT failed to connect");
}

uint32_t nextMetricsUpdate = 0;
void sendTelegrafMetrics() {
    if (millis() > nextMetricsUpdate) {
        nextMetricsUpdate = millis() + 30000;

        char buffer[150];
        snprintf(buffer, sizeof(buffer),
            "status,device=Energy\\ Monitor uptime=%d,resetReason=%d,firmware=\"%s\",memTotal=%ld,memUsed=%ld,ipv4=\"%s\"",
            System.uptime(),
            System.resetReason(),
            System.version().c_str(),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_TOTAL_RAM),
            DiagnosticsHelper::getValue(DIAG_ID_SYSTEM_USED_RAM),
            WiFi.localIP().toString().c_str()
            );
        mqttClient.publish("telegraf/particle", buffer);
    }
}

int cloudReset(const char* data) {
    uint32_t rTime = millis() + 10000;
    Log.info("Cloud reset received");
    while (millis() < rTime)
        Particle.process();
    System.reset();
    return 0;
}

SYSTEM_THREAD(ENABLED)

void startupMacro() {
    WiFi.selectAntenna(ANT_EXTERNAL);
    System.enableFeature(FEATURE_RESET_INFO);
    System.enableFeature(FEATURE_RETAINED_MEMORY);
}
STARTUP(startupMacro());

void setup() {

    pinMode(LIGHTSENSORPIN, INPUT);
  
    waitFor(Particle.connected, 30000);

    do {
        resetTime = Time.now();
        Particle.process();
    } while (resetTime < 1500000000 || millis() < 10000);

    if (System.resetReason() == RESET_REASON_PANIC) {
        if ((Time.now() - lastHardResetTime) < 120) {
            resetCount++;
        } else {
            resetCount = 1;
        }

        lastHardResetTime = Time.now();

        if (resetCount > 3) {
            System.enterSafeMode();
        }
    } else if (System.resetReason() == RESET_REASON_WATCHDOG) {
        Log.info("RESET BY WATCHDOG");
    } else {
        resetCount = 0;
    }

    Particle.function("cloudReset", cloudReset);
    Particle.variable("resetTime", &resetTime, INT);

    Particle.publishVitals(900);

    bmePresent = bme.begin();
    if (!bmePresent) {
        Log.info("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
        Log.info("SensorID was: 0x%X", bme.sensorID());
    } else {
        Log.info("Valid BME280 sensor found");
        bme.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X1, // temperature
                        Adafruit_BME280::SAMPLING_X1, // pressure
                        Adafruit_BME280::SAMPLING_X1, // humidity
                        Adafruit_BME280::FILTER_OFF);
    }
  
    attachInterrupt(LIGHTSENSORPIN, lightSensorISR, FALLING);
    uint32_t resetReasonData = System.resetReasonData();
    connectToMQTT();
    Particle.publish("pushover", String::format("EnergyMonitor: Boot complete: %d-%d", System.resetReason(), resetReasonData).c_str(), PRIVATE);
}

void loop() {
    unsigned long currenttime = millis();

    if (currenttime > nextUpdateTime) {
        if (powerWatts != UNSET && temperature != UNSET && humidity != UNSET && pressure != UNSET) {
          sendData();
          nextUpdateTime = currenttime + PUSH_RESULTS_INTERVAL;
        }
    }

    if (bmePresent && currenttime > nextEnviornmentReading) {
        nextEnviornmentReading = currenttime + ENVIORNMENT_INTERVAL;

        bme.takeForcedMeasurement();

        temperature = round(bme.readTemperature()*10.0) / 10.0;
        humidity = (int) round(bme.readHumidity());
        pressure = (int) round(bme.readPressure() / 100.0F);
    }

    if (mqttClient.isConnected()) {
        mqttClient.loop();
        sendTelegrafMetrics();
    } else if (millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout)) {
        Log.info("MQTT Disconnected");
        connectToMQTT();
    }

    wd.checkin(); // resets the AWDT count
}