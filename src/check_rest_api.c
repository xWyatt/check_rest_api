#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json/json_tokener.h>
#include <json/json_object.h>
#include <json/json_util.h>

#include "headers/check_rest_api.h"
#include "headers/read_input.h"
#include "headers/check_thresholds.h"

// Define our cURL handle and struct, and JSON object
CURL* curl;
struct cURLHTTPBody* body;

// Cleanup
void end(int exitCode) {

  curl_easy_cleanup(curl);

  free(body->payload);
  free(body);

  free(argVals->hostname);
  free(argVals->username);
  free(argVals->password);  

  int i;
  for (i = 0; i < argVals->numberOfKeys; i++) {
    free(argVals->keys[i]);
  }

  free(argVals->keys);
  free(argVals->warningMin);
  free(argVals->warningMax);
  free(argVals->warningInclusive);
  free(argVals->criticalMin);
  free(argVals->criticalMax);
  free(argVals->criticalInclusive);

  exit(exitCode);
}


// Callback after cURL.. does its thing
size_t write_data(void *buf, size_t size, size_t nmemb, void *userp) {

  // First run - initalize everything
  if (body->size == 0) {
    // Set size of body. Data + 1 for null terminator
    body->size = nmemb + 1;

    body->payload = (char *) malloc(body->size);

    // If Malloc failed, signal to handler to use HTTP Status code
    if (body->payload == NULL) {
      body->size = 0;
    } else {
      memcpy(body->payload, buf, body->size);
      body->payload[nmemb] = '\0';
    }
  } else { // All other runs - append data

    // Set new body size
    body->size += nmemb;
    
    // Overwrite payload with new data
    body->payload = (char *) realloc(body->payload, body->size);
   
    if (body->payload == NULL) {
      body->size = 0;
    } else {
      strncat(body->payload, buf, nmemb);
    }
  }
  
  return nmemb;
}

void* callAPI(void) {

  CURLcode res;

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, argVals->hostname);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);    

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, argVals->timeout);

    if (argVals->username != NULL) {
      curl_easy_setopt(curl, CURLOPT_USERNAME, argVals->username);
      curl_easy_setopt(curl, CURLOPT_PASSWORD, argVals->password);
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    }

    res = curl_easy_perform(curl);

    // This is the only place, besides main() that returns
    if (res != CURLE_OK) {
      printf("UNKNOWN - cURL call resulted in error: %s\n", curl_easy_strerror(res));
      end(UNKNOWN);
    }

    return curl;
  }

  return 0;
}

// Parses JSON
void parseJSON(json_object** json) {

  if (body->size == 0) return;

  json_tokener* tok = json_tokener_new();
  enum json_tokener_error jerr = json_tokener_continue;

  while (jerr == json_tokener_continue) {

    *json = json_tokener_parse_ex(tok, body->payload, body->size);

    jerr = json_tokener_get_error(tok);
  }

  json_tokener_free(tok);
}


// Program Entry Point
int main(int argc, char** argv) {

  json_object* json;

  // Initialize our body
  body = malloc(sizeof(struct cURLHTTPBody));
  body->payload = NULL;
  body->size = 0;

  if (!validateArguments(argc, argv)) end(UNKNOWN);

  // Call the API and get data back
  callAPI();

  int statusCode;

  // If we didn't get back a body, fallback to HTTP status codes
  if (body->payload == NULL || argVals->numberOfKeys == 0) {
    statusCode = checkHTTPStatusCode(curl);
  } else {
    parseJSON(&json);
    statusCode = checkHTTPBody(json, argVals);
    json_object_put(json);
  }

  return statusCode;
}
