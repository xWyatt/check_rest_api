#ifndef CHECK_REST_API
#define CHECK_REST_API

enum STATUS{OK, WARNING, CRITICAL, UNKNOWN};

// Holds our response from cURL
typedef struct cURLHTTPBody {
  char* payload;
  size_t size;
} cURLHTTPBody;

void end(int exitCode);
size_t write_data(void* buffer, size_t size, size_t nmemb, void* userPointer);
void* callAPI(char* apiURL);
void parseJSON();

#endif 
