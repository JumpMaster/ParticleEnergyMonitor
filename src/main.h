#ifndef Main_h
#define Main_h

//#define LOGGING
#define UNSET   -100      // A variable that wont be used by power, temperature (i hope), or humidity
#define ENVIORNMENT_INTERVAL  60000UL // 60 seconds
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

#endif
