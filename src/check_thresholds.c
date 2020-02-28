#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <json/json_tokener.h>
#include <json/json_object.h>
#include <json/json_util.h>

#include "headers/check_rest_api.h"
#include "headers/read_input.h"

// Checks HTTP status codes. Program can/will exit from here
int checkHTTPStatusCode(CURL* curl) {

  long httpResponseCode;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpResponseCode);

  if (httpResponseCode == 200) {
    printf("OK - API Returned 200-OK\n");
    return OK;
  }

  
  if (httpResponseCode < 500) {
    printf("WARNING - Unexpected HTTP response code: %ld\n", httpResponseCode);
    return WARNING;
  }

  if (httpResponseCode > 500) {
    printf("CRITICAL - HTTP Reponse Code > 500: %ld\n", httpResponseCode);
    return CRITICAL;
  }

  printf("UNKNOWN - HTTP Response Code: %ld\n", httpResponseCode);
  return UNKNOWN;
}

// Checks HTTP body against -K parameters, and -w/-c
// Program can/will exit from here
int checkHTTPBody(json_object* json, argValues* arguments) {

  int numberOfKeys = arguments->numberOfKeys;
  char** keys = arguments->keys;

  // Is there more elegant methods of sorting out OK/WARN/CRIT/UNKNOWN messages so
  // they output in order? Yeah. There are.
  // TODO: More efficient way of storing and sorting output messages from checkHTTPBody
  int severityLevel = OK;
  char** OKmessages;
  char** WARNINGmessages;
  char** CRITICALmessages;
  char** UNKNOWNmessages;
  OKmessages = malloc(numberOfKeys * sizeof(OKmessages));
  WARNINGmessages = malloc(numberOfKeys * sizeof(WARNINGmessages));
  CRITICALmessages = malloc(numberOfKeys * sizeof(CRITICALmessages));
  UNKNOWNmessages = malloc(numberOfKeys * sizeof(UNKNOWNmessages));

  int i;

  // Allocate blank values for our messages  
  for (i = 0; i < numberOfKeys; i++) { 
    OKmessages[i] = malloc(1);
    WARNINGmessages[i] = malloc(1);
    CRITICALmessages[i] = malloc(1);
    UNKNOWNmessages[i] = malloc(1);

    OKmessages[i][0] = '\0'; 
    WARNINGmessages[i][0] = '\0'; 
    CRITICALmessages[i][0] = '\0'; 
    UNKNOWNmessages[i][0] = '\0'; 
  }

  // Check each key for 'validity'
  for (i = 0; i < numberOfKeys; i++) {
    char* jsonKey = keys[i];

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
        if (arguments->criticalMin != NULL) {
        
          int min, max, inclusive;
          min = arguments->criticalMin[i];
          max = arguments->criticalMax[i];
          inclusive = arguments->criticalInclusive[i];
        
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
        if (arguments->warningMin != NULL && (CRITICALmessages[i]) == 0) {
        
          int min, max, inclusive;
          min = arguments->warningMin[i];
          max = arguments->warningMax[i];
          inclusive = arguments->warningInclusive[i];
        
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
  for (i = 0; i < numberOfKeys; i++) {
    printf(UNKNOWNmessages[i]);
  }
  for (i = 0; i < numberOfKeys; i++) {
    printf(CRITICALmessages[i]);
  }
  for (i = 0; i < numberOfKeys; i++) {
    printf(WARNINGmessages[i]); 
  }
  for (i = 0; i < numberOfKeys; i++) {
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

  return severityLevel;
}
