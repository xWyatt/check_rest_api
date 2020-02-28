#ifndef CHECK_REST_API
#define CHECK_REST_API

enum STATUS{OK, WARNING, CRITICAL, UNKNOWN};

// Holds our response from cURL
typedef struct cURLHTTPBody {
  char* payload;
  size_t size;
} cURLHTTPBody;

#endif 
