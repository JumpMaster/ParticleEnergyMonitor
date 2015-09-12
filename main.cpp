#include "application.h"
#include "HttpClient.h"
#include "PietteTech_DHT.h"
#include "main.h"

//declarations

void dht_wrapper(); // must be declared before the lib initialization
bool bDHTstarted;   // flag to indicate we started acquisition
bool ignoreNextInterrupt = false;
float temperature = UNSET;
float humidity = UNSET;
volatile int power = UNSET;

volatile unsigned long lastWattUsedTime = 0;
unsigned long nextEnviornmentReading = 0;
unsigned long nextUpdateTime = 0;
unsigned long resetTime = 0;

HttpClient http;

http_response_t response;
http_request_t request;

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// This wrapper is in charge of calling
// must be defined like this for the lib work
void dht_wrapper() {
    DHT.isrCallback();
}

void lightSensorISR() {
  if (ignoreNextInterrupt) {
    ignoreNextInterrupt = false;
    return;
  }
  LOGGING_PRINTLN("LIGHT FLASHED");
  unsigned long currenttime = millis();
  if (lastWattUsedTime > 0)
    power = 3600000 / (currenttime - lastWattUsedTime);
  lastWattUsedTime = currenttime;
}

void doGetRequest() {
  int err = 0;
  response.status = 0;
  response.body = '\0';
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
          response.body = String(body);
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

void sendData(int power, float temperature, float humidity) {
  char buffer[200];
  sprintf(buffer, "/input/post.json?apikey=%s&node=1&json={power:%d,temperature:%.1f,humidity:%.1f}", apikey, power, temperature, humidity);
  request.path = String(buffer);
  LOGGING_PRINTLN(request.path);
  doGetRequest();
}

STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));

void setup() {
  #ifdef LOGGING
    Serial.begin(9600);
    while (!Serial.available()) {
      LOGGING_PRINTLN("Press any key to start.");
      delay (1000);
    }
  #endif
  pinMode(LIGHTSENSORPIN, INPUT);

  request.ip = { 80, 243, 190, 58 };
  request.hostname = "emoncms.org";

  do
  {
    resetTime = Time.now();
    delay(10);
  } while (resetTime < 1000000 && millis() < 20000);

  Spark.variable("resetTime", &resetTime, INT);
}

void loop() {
  unsigned long currenttime = millis();

  if (currenttime > nextUpdateTime) {
    if (power != UNSET && temperature != UNSET && humidity != UNSET) {
      sendData(power, temperature, humidity);
      nextUpdateTime = currenttime + PUSH_RESULTS_INTERVAL;
    }
  }

  if (currenttime > nextEnviornmentReading) {
    if (!bDHTstarted) {		// start the sample
      LOGGING_PRINTLN(": Retrieving information from sensor: ");
      detachInterrupt(LIGHTSENSORPIN);
	    DHT.acquire();
	    bDHTstarted = true;
    }

    if (!DHT.acquiring()) {		// has sample completed?
      // get DHT status
      int result = DHT.getStatus();
      LOGGING_PRINT("Read sensor: ");

      if (result == DHTLIB_OK) {
        LOGGING_PRINTLN("OK");
        humidity = DHT.getHumidity();
        temperature = DHT.getCelsius();
      }
      bDHTstarted = false;
      nextEnviornmentReading = currenttime + ENVIORNMENT_INTERVAL;
      lastWattUsedTime = 0;
      ignoreNextInterrupt = true;
      attachInterrupt(LIGHTSENSORPIN, lightSensorISR, FALLING);
    }
  }
  delay(10);
}
