#include "papertrail.h"
#include "mqtt.h"
#include "publishqueue.h"
#include "application.h"
#include "HttpClient.h"
#include "main.h"
#include "ArduinoJson.h"
#include "SparkFunBME280.h"
#include "secrets.h"

float temperature = UNSET;
float humidity = UNSET;
float pressure = UNSET;
int intHumidity;
int intTemperature;
volatile int power = UNSET;
// volatile float avg_power = UNSET;
// volatile int power_count = 0;

volatile unsigned long lastWattUsedTime = 0;

unsigned long nextEnviornmentReading = 0;
unsigned long nextUpdateTime = 0;
unsigned long resetTime = 0;

MQTT mqttClient(mqttServer, 1883, mqttCallback);
unsigned long lastMqttConnectAttempt;
const int mqttConnectAtemptTimeout = 5000;

HttpClient http;

http_response_t response;
http_request_t request;

BME280 bme;
PublishQueue pq;
// PresenceManager pm;

PapertrailLogHandler papertrailHandler(papertrailAddress, papertrailPort, "Energy-Monitor");

ApplicationWatchdog wd(60000, System.reset);

void lightSensorISR() {

  LOGGING_PRINTLN("LIGHT FLASHED");
  unsigned long currenttime = millis();
  if (lastWattUsedTime > 0) {
    power = 3600000 / (currenttime - lastWattUsedTime);

    // power_count++;
    // avg_power += (power - avg_power) / power_count;
  }
  lastWattUsedTime = currenttime;
}

void doGetRequest() {
  int err = 0;
  response.status = 0;
  response.body[0] = '\0';
//  unsigned long httpStartTime = millis();

  // Using IP
  err = http.get(request.ip, request.hostname, request.path);

  if (err == 0)
  {
    err = http.responseStatusCode();
    if (err >= 0)
    {
      response.status = err;

      if (err >= 200 || err < 300)
      {
        err = http.skipResponseHeaders();
        if (err >= 0)
        {
          int bodyLen = http.contentLength();

          unsigned long timeoutStart = millis();
          if (bodyLen <= 0)
            bodyLen = kFallbackContentLength;

          char *body = (char *) malloc( sizeof(char) * ( bodyLen + 1 ) );

          char c;
          int i = 0;

          while ( (http.connected() || http.available()) &&
                 ((millis() - timeoutStart) < http.kHttpResponseTimeout))
          {
            // Let's make sure this character will fit into our char array
            if (i == bodyLen)
            {
              // No it won't fit. Let's try and resize our body char array
              char *temp = (char *) realloc(body, sizeof(char) * (bodyLen+kFallbackContentLength));

              if ( temp != NULL ) //resize was successful
              {
                bodyLen += kFallbackContentLength;
                body = temp;
              }
              else //there was an error
              {
                free(temp);
                break;
              }
            }

            if (http.available())
            {
              c = http.read();

              body[i] = c;
              i++;
              // We read something, reset the timeout counter
              timeoutStart = millis();
            }
            else
            {
              // We haven't got any data, so let's pause to allow some to
              // arrive
              delay(http.kHttpWaitForDataDelay);
            }
          }
          body[i] = '\0';
          //return body;
          strncpy(response.body, body, bodyLen > sizeof(body) ? sizeof(body) : bodyLen);
          //response.body = body;
          
          free(body);
        }
      }
    }
  }
  http.stop();

//  Serial.println();
//  Serial.print("http request took : ");
//  Serial.print(millis()-httpStartTime);
//  Serial.println("ms");
}

void sendData(float power, float temperature, float humidity, float pressure) {
  //request.path = String::format("/input/post.json?apikey=%s&node=1&json={power:%.0f,temperature:%.1f,humidity:%.1f,pressure:%.1f}", API_KEY, power, temperature, humidity, pressure);
  
  //char data[256];
//   snprintf(request.path, sizeof(request.path), "/input/post.json?apikey=%s&node=1&json={power:%.0f,temperature:%.1f,humidity:%.1f,pressure:%.1f}", API_KEY, power, temperature, humidity, pressure);
  
    if (mqttClient.isConnected()) {
        char charPower[5];
        char charTemperature[6];
        char charHumidity[6];
        char charPressure[7];
        snprintf(charPower, sizeof(charPower), "%.0f", power);
        snprintf(charTemperature, sizeof(charTemperature), "%.1f", temperature);
        snprintf(charHumidity, sizeof(charHumidity), "%.0f", humidity);
        snprintf(charPressure, sizeof(charPressure), "%.0f", pressure);
        
        mqttClient.publish("emon/particle/power", charPower, true);
        mqttClient.publish("emon/particle/temperature", charTemperature, true);
        mqttClient.publish("emon/particle/humidity", charHumidity, true);
        mqttClient.publish("emon/particle/pressure", charPressure, true);
    }
  
  LOGGING_PRINTLN(request.path);
//   doGetRequest();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
}

void random_seed_from_cloud(unsigned seed) {
   srand(seed);
}

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
STARTUP(System.enableFeature(FEATURE_RESET_INFO));

int roundFloat(float x) {
    return ((x)>=0?(int)((x)+0.5):(int)((x)-0.5));
}

void connectToMQTT() {
    lastMqttConnectAttempt = millis();
    bool mqttConnected = mqttClient.connect(System.deviceID(), mqttUsername, mqttPassword);
    if (mqttConnected) {
        Log.info("MQTT Connected");
    } else
        Log.info("MQTT failed to connect");
}

void setup() {
  WiFi.useDynamicIP();

  #ifdef LOGGING
    Serial.begin(9600);
    while (!Serial.available()) {
      LOGGING_PRINTLN("Press any key to start.");
      delay (1000);
    }
  #endif

  pinMode(LIGHTSENSORPIN, INPUT);

  request.ip = EMONCMS_IP;
  request.hostname = EMONCMS_URL;
  
  waitUntil(Particle.connected);

  do
  {
    resetTime = Time.now();
    delay(10);
  } while (resetTime < 1000000 && millis() < 20000);

  Particle.variable("resetTime", &resetTime, INT);
  Particle.variable("humidity", intHumidity);
  Particle.variable("temperature", intTemperature);
  
    // bme.reset();
    // delay(10);
    bme.settings.commInterface = SPI_MODE;
	bme.settings.chipSelectPin = BME_CS;
	//runMode can be:
	//  0, Sleep mode
	//  1 or 2, Forced mode
	//  3, Normal mode
	bme.settings.runMode = 3; //Normal mode

	//tStandby can be:
	//  0, 0.5ms
	//  1, 62.5ms
	//  2, 125ms
	//  3, 250ms
	//  4, 500ms
	//  5, 1000ms
	//  6, 10ms
	//  7, 20ms
	bme.settings.tStandby = 5;

	//filter can be off or number of FIR coefficients to use:
	//  0, filter off
	//  1, coefficients = 2
	//  2, coefficients = 4
	//  3, coefficients = 8
	//  4, coefficients = 16
	bme.settings.filter = 4;

	//tempOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme.settings.tempOverSample = 5;

	//pressOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    bme.settings.pressOverSample = 5;

	//humidOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme.settings.humidOverSample = 5;

  
    if (!bme.begin())
        pq.publish("pushover", "Could not find a valid BME280 sensor, check wiring!");
  
  attachInterrupt(LIGHTSENSORPIN, lightSensorISR, FALLING);
  uint32_t resetReasonData = System.resetReasonData();
  connectToMQTT();
  pq.publish("pushover", String::format("EnergyMonitor: Boot complete: %d-%d", System.resetReason(), resetReasonData).c_str());
}

void loop() {
    unsigned long currenttime = millis();

    if (currenttime > nextUpdateTime) {
        if (power != UNSET && temperature != UNSET && humidity != UNSET && pressure != UNSET) {
          sendData(power, temperature, humidity, pressure);
          nextUpdateTime = currenttime + PUSH_RESULTS_INTERVAL;
        }
    }

    if (currenttime > nextEnviornmentReading) {
        temperature = bme.readTempC();
        humidity = bme.readFloatHumidity();
        pressure = bme.readFloatPressure() / 100.0F;
        if (intHumidity != roundFloat(humidity) || intTemperature != roundFloat(temperature)) {
            intHumidity = roundFloat(humidity);
            intTemperature = roundFloat(temperature);
        }
        nextEnviornmentReading = currenttime + ENVIORNMENT_INTERVAL;
    }

    if (mqttClient.isConnected()) {
        mqttClient.loop();
    } else if (millis() > (lastMqttConnectAttempt + mqttConnectAtemptTimeout)) {
        Log.info("MQTT Disconnected");
        connectToMQTT();
    }

    pq.process();
    wd.checkin(); // resets the AWDT count
}
