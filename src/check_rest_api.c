#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json/json_tokener.h>
#include <json/json_object.h>
#include <json/json_util.h>

#include "headers/check_rest_api.h"
#include "headers/read_input.h"

// Define our cURL handle and struct, and JSON object
CURL* curl;
struct cURLHTTPBody* body;
json_object* json;

// Cleanup
void end(int exitCode) {

  curl_easy_cleanup(curl);

  free(body->payload);
  free(body);

  free(argVals->hostname);
  
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

  if (nmemb == 1) {
    body->size = 0;
    body->payload = NULL;
    return nmemb;
  }  

  // Size of body plus null terminator
  body->size = nmemb + 1;
  
  // Expand our payload to account for the actual payload
  body->payload = (char*) malloc(body->size + 1);

  if (body->payload == NULL) {

    // Malloc failed. Signal to handler to use HTTP Status code
    body->size = 0;
  } else {

    // Copy payload and set null terminiating byte
    memcpy(body->payload, buf, body->size - 1);
    body->payload[nmemb] = '\0';
  }

  return nmemb;
}

void* callAPI(void) {

  CURLcode res;

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, argVals->hostname);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);    

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

// Checks HTTP status codes. Program can/will exit from here
void checkHTTPStatusCode(void) {

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
}

// Checks HTTP body against -K parameters, and -w/-c
// Program can/will exit from here
void checkHTTPBody(void) {

  // Is there more elegant methods of sorting out OK/WARN/CRIT/UNKNOWN messages so
  // they output in order? Yeah. There are.
  int severityLevel = OK;
  char** OKmessages;
  char** WARNINGmessages;
  char** CRITICALmessages;
  char** UNKNOWNmessages;
  OKmessages = malloc(argVals->numberOfKeys * sizeof(OKmessages));
  WARNINGmessages = malloc(argVals->numberOfKeys * sizeof(WARNINGmessages));
  CRITICALmessages = malloc(argVals->numberOfKeys * sizeof(CRITICALmessages));
  UNKNOWNmessages = malloc(argVals->numberOfKeys * sizeof(UNKNOWNmessages));

  int i;

  // Allocate blank values for our messages  
  for (i = 0; i < argVals->numberOfKeys; i++) { 
    OKmessages[i] = malloc(1);
    WARNINGmessages[i] = malloc(1);
    CRITICALmessages[i] = malloc(1);
    UNKNOWNmessages[i] = malloc(1);

    OKmessages[i][0] = '\0'; 
    WARNINGmessages[i] = '\0'; 
    CRITICALmessages[i][0] = '\0'; 
    UNKNOWNmessages[i][0] = '\0'; 
  }

  // Check each key for 'validity'
  for (i = 0; i < argVals -> numberOfKeys; i++) {
    char* jsonKey = argVals->keys[i];

    // Get the value
    json_object* object;
    if (json_object_object_get_ex(json, jsonKey, &object)) {
      json_type type  = json_object_get_type(object);
    
      // Require type to be a number before checking
      if (type != json_type_int && type != json_type_double) {

        char msg[] = "UNKNOWN - JSON type of '%s' is '%s'. Needs to be a double or int.\n";
        int len = snprintf(NULL, 0, msg, jsonKey, json_type_to_name(type));
        UNKNOWNmessages[i] = realloc(UNKNOWNmessages[i], len + 1);
        sprintf(UNKNOWNmessages[i], msg, jsonKey, json_type_to_name(type));

        severityLevel = UNKNOWN;
      } else {

        double value = json_object_get_double(object);
      
        // Check Critical, if applicable
        if (argVals->criticalMin != NULL) {
        
          int min, max, inclusive;
          min = argVals->criticalMin[i];
          max = argVals->criticalMax[i];
          inclusive = argVals->criticalInclusive[i];
        
          if (inclusive) {

            if (value <= min || value >= max) {

              char msg[] = "CRITICAL - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              CRITICALmessages[i] = realloc(CRITICALmessages[i], len + 1);
              sprintf(CRITICALmessages[i], msg, jsonKey, value);

              if (severityLevel < WARNING) severityLevel = WARNING;
            } else {

              char msg[] = "OK - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              OKmessages[i] = realloc(OKmessages[i], len + 1);      
              sprintf(OKmessages[i], msg, jsonKey, value);
            }

          } else {

            if (value < min || value > max) {
              
              char msg[] = "CRITICAL - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              CRITICALmessages[i] = realloc(CRITICALmessages[i], len + 1);
              sprintf(CRITICALmessages[i], msg, jsonKey, value);

              if (severityLevel < WARNING) severityLevel = WARNING;
            } else {           

              char msg[] = "OK - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              OKmessages[i] = realloc(OKmessages[i], len + 1);      
              sprintf(OKmessages[i], msg, jsonKey, value);
            }
          }
        }

        // Check Warning, if applicable (We set WARNING, and CRITICAL isn't set)
        if (argVals->warningMin != NULL && (CRITICALmessages[i]) == 0) {
        
          int min, max, inclusive;
          min = argVals->warningMin[i];
          max = argVals->warningMax[i];
          inclusive = argVals->warningInclusive[i];
        
          if (inclusive) {

            if (value <= min || value >= max) {

              char msg[] = "WARNING - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              WARNINGmessages[i] = realloc(WARNINGmessages[i], len + 1);
              sprintf(WARNINGmessages[i], msg, jsonKey, value);

              if (severityLevel < WARNING) severityLevel = WARNING;
            } else {

              // Only write if we haven't already written OK
              if (strlen(OKmessages[i]) == 0) {
                char msg[] = "OK - '%s' is %g\n";
                int len = snprintf(NULL, 0, msg, jsonKey, value);
                OKmessages[i] = realloc(OKmessages[i], len + 1);
                sprintf(OKmessages[i], msg, jsonKey, value);
              }
            }

          } else {

            if (value < min || value > max) {
      
              char msg[] = "WARNING - '%s' is %g\n";
              int len = snprintf(NULL, 0, msg, jsonKey, value);
              WARNINGmessages[i] = realloc(WARNINGmessages[i], len + 1);
              sprintf(WARNINGmessages[i], msg, jsonKey, value);

              if (severityLevel < WARNING) severityLevel = WARNING;
            } else {

              // Only write if we haven't already written OK
              if (strlen(OKmessages[i]) == 0) {
                char msg[] = "OK - '%s' is %g\n";
                int len = snprintf(NULL, 0, msg, jsonKey, value);
                OKmessages[i] = realloc(OKmessages[i], len + 1);
                sprintf(OKmessages[i], msg, jsonKey, value);
              }
            }
          }
        }
      }
    } else { // Object not found
      sprintf(UNKNOWNmessages[i], "UNKNOWN - JSON key '%s' not found!\n", jsonKey);
      severityLevel = UNKNOWN;
    }
  }

  // Print everything in order of UNKNOWN, then CRITICAL, WARNING, and finally OK
  for (i = 0; i < argVals->numberOfKeys; i++) {
    printf(UNKNOWNmessages[i]);
  }
  for (i = 0; i < argVals->numberOfKeys; i++) {
    printf(CRITICALmessages[i]);
  }
  for (i = 0; i < argVals->numberOfKeys; i++) {
    printf(WARNINGmessages[i]); 
  }
  for (i = 0; i < argVals->numberOfKeys; i++) {
    printf(OKmessages[i]);
    
    free(UNKNOWNmessages[i]);
    free(CRITICALmessages[i]);
    free(WARNINGmessages[i]);
    free(OKmessages[i]);
  }

  free(UNKNOWNmessages);
  free(CRITICALmessages);
  free(WARNINGmessages);
  free(OKmessages);

  exit(severityLevel);
}

// Program Entry Point
int main(int argc, char** argv) {

  // Initialize our body
  body = malloc(sizeof(struct cURLHTTPBody));
  body->payload = NULL;

  if (!validateArguments(argc, argv)) end(UNKNOWN);

  // Call the API and get data back
  callAPI();

  // If we didn't get back a body, fallback to HTTP status codes
  if (body->payload == NULL || argVals->numberOfKeys == 0) {
    checkHTTPStatusCode();
  } else {
    parseJSON();
    checkHTTPBody();
  }

  //json_object* status = json_object_object_get_ex(json, "status");

  //printf("%s\n", json_object_get_string(status));

  // Not sure how we would get here
  printf("UNKNOWN - Something weird happened.\n");
  end(UNKNOWN);     

  // We never get here; just satisfies -Werror=return-type
  return 0;
}
