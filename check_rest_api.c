#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json/json_tokener.h>
#include <json/json_object.h>

#include "check_rest_api.h"
#include "read_input.h"

enum STATUS{OK, WARNING, CRITICAL, UNKNOWN};

// Holds our response from cURL
typedef struct cURLHTTPBody {
  char* payload;
  size_t size;
} cURLHTTPBody;

// Define our cURL handle and struct, and JSON object
CURL* curl;
struct cURLHTTPBody* body;
json_object* json;

// Cleanup
void end(int exitCode) {

  free(body->payload);
  free(body);

  exit(exitCode);
}


// Callback after cURL.. does its thing
size_t write_data(void *buf, size_t size, size_t nmemb, void *userp) {

  //struct cURLHTTPBody* body = (struct cURLHTTPBody*) userp;
  
  // Size of body plus null terminator
  body->size = nmemb + 1;
  
  // Expand our payload to account for the actual payload
  body->payload = (char*) malloc(body->size);

  if (body->payload == NULL) {

    // Malloc failed. Signal to handler to use HTTP Status code
    body->size = 0;
  } else {

    // Copy payload and set null terminiating byte
    memcpy(body->payload, buf, body->size - 1);
    body->payload[body->size] = '\0';
  }

  return nmemb;
}

void* callAPI(char* apiUrl) {

  CURLcode res;

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);    

    res = curl_easy_perform(curl);

    // This is the only place, besides main() that returns
    if (res != CURLE_OK) {
      printf("UNKNOWN - cURL call resulted in error: %s\n", curl_easy_strerror(res));
      end(UNKNOWN);
    }

    curl_easy_cleanup(curl);
    
    return curl;
  }

  return 0;
}

// Parses JSON
void parseJSON() {
  
  if (body->size == 0) return;

  json_tokener* tok = json_tokener_new();
  enum json_tokener_error jerr = json_tokener_continue;

  while (jerr == json_tokener_continue) {

    json = json_tokener_parse_ex(tok, body->payload, body->size);

    jerr = json_tokener_get_error(tok);
  }

  // Not successful :(
  if (jerr != json_tokener_success) {
    json_tokener_free(tok);
  }
}

// Program Entry Point
int main(int argc, char** argv) {

  // Initialize our body
  body = malloc(sizeof(struct cURLHTTPBody));

  if (!validateArguments(argc, argv)) return CRITICAL;

  // Returns a CURL thing that we can use to get info from the API call
  CURL *curl = (CURL*) callAPI(argv[argc - 1]);

  parseJSON();

  //json_object* status = json_object_object_get_ex(json, "status");

  //printf("%s\n", json_object_get_string(status));

  // Extract the HTTP Response Code
  long httpResponseCode;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpResponseCode);

  if (httpResponseCode == 200) {
    printf("OK - API Returned 200-OK\n");
    end(OK);
  }

  
  if (httpResponseCode < 500) {
    printf("WARNING - Unexpected HTTP response code: %ld\n", httpResponseCode);
    end(WARNING);
  }

  if (httpResponseCode > 500) {
    printf("CRITICAL - HTTP Reponse Code > 500: %ld\n", httpResponseCode);
    end(CRITICAL);
  }

  // Not sure how we would get here
  printf("UNKNOWN - Something weird happened. HTTP Reponse Code: %ld\n", httpResponseCode);
  end(UNKNOWN);     

  // We never get here; just satisfies -Werror=return-type
  return 0;
}
