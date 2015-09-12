#ifndef Main_h
#define Main_h

//#define LOGGING
#define DHTTYPE DHT22     // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN  D1        // Digital pin for communications
#define UNSET   -100      // A variable that wont be used by power, temperature (i hope), or humidity
#define ENVIORNMENT_INTERVAL  300000UL // 5 minutes
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
static const char apikey[] = "MYEMONCMSAPIKEY";

typedef struct
{
  String hostname;
  IPAddress ip;
  String path;
  int port;
  String body;
} http_request_t;

/**
 * HTTP Response struct.
 * status  response status code.
 * body    response body
 */
typedef struct
{
  int status;
  String body;
} http_response_t;

#endif
