#ifndef Main_h
#define Main_h

//#define LOGGING
// #define DHTTYPE DHT22     // Sensor type DHT11/21/22/AM2301/AM2302
// #define DHTPIN  D1        // Digital pin for communications
#define BME_CS A2
#define UNSET   -100      // A variable that wont be used by power, temperature (i hope), or humidity
#define ENVIORNMENT_INTERVAL  30000UL // 30 seconds
#define PUSH_RESULTS_INTERVAL 10000UL
#define LIGHTSENSORPIN  A0

#ifdef LOGGING
  #define LOGGING_PRINTLN(x)  Serial.println(x)
  #define LOGGING_PRINT(x)    Serial.print(x)
  #define LOGGING_WRITE(x, y)    Serial.write(x, y)
#else
  #define LOGGING_PRINTLN(x)
  #define LOGGING_PRINT(x)
  #define LOGGING_WRITE(x, y)
#endif

// If Content-Length isn't given this is used for the body length increments
const int kFallbackContentLength = 100;

typedef struct
{
  String hostname;
  IPAddress ip;
  char path[200];
  int port;
  char body[200];
} http_request_t;

/**
 * HTTP Response struct.
 * status  response status code.
 * body    response body
 */
typedef struct
{
  int status;
  char body[200];
} http_response_t;

#endif
