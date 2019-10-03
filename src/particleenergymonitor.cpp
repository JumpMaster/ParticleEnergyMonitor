#include "papertrail.h"
#include "mqtt.h"
#include "application.h"
#include "particleenergymonitor.h"
#include "Adafruit_BME280.h"
#include "Adafruit_Sensor.h"
#include "secrets.h"

//  Stubs
void mqttCallback(char* topic, byte* payload, unsigned int length);

double temperature = UNSET;
int humidity = UNSET;
int pressure = UNSET;
volatile int power = UNSET;
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
    power = 3600000 / (currenttime - lastWattUsedTime);

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
        snprintf(charPower, sizeof(charPower), "%d", power);
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

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
STARTUP(System.enableFeature(FEATURE_RESET_INFO));
STARTUP(System.enableFeature(FEATURE_RETAINED_MEMORY));
SYSTEM_THREAD(ENABLED);

void connectToMQTT() {
    lastMqttConnectAttempt = millis();
    bool mqttConnected = mqttClient.connect(System.deviceID(), mqttUsername, mqttPassword);
    if (mqttConnected) {
        Log.info("MQTT Connected");
    } else
        Log.info("MQTT failed to connect");
}

void setup() {

  pinMode(LIGHTSENSORPIN, INPUT);
  
  waitFor(Particle.connected, 30000);
  
  if (!Particle.connected)
      System.reset();
  do {
      resetTime = Time.now();
      delay(10);
  } while (resetTime < 1000000UL && millis() < 20000);

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
  } else {
      resetCount = 0;
  }

  Particle.variable("resetTime", &resetTime, INT);

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
        if (power != UNSET && temperature != UNSET && humidity != UNSET && pressure != UNSET) {
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
    } else if (millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout)) {
        Log.info("MQTT Disconnected");
        connectToMQTT();
    }

    wd.checkin(); // resets the AWDT count
}