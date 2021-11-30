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
    printf("OK - API Returned 200-OK | status_code=200\n");
    return OK;
  } else if (httpResponseCode == 201) {
    printf("OK - API Returned 201-Created | status_code=201\n");
    return OK;
  }

  
  if (httpResponseCode < 500) {
    printf("WARNING - Unexpected HTTP response code: %ld | status_code=%ld\n", httpResponseCode, httpResponseCode);
    return WARNING;
  }

  if (httpResponseCode > 500) {
    printf("CRITICAL - HTTP Reponse Code > 500: %ld | status_code=%ld\n", httpResponseCode, httpResponseCode);
    return CRITICAL;
  }

  printf("UNKNOWN - HTTP Response Code: %ld | status_code=%ld\n", httpResponseCode, httpResponseCode);
  return UNKNOWN;
}

// Determines if a key exists in the JSON object
//   If it does exist, it will be passed back via the third parameter
//   This wraps json-c's json_object_object_get_ex() method to allow us to use
//   our argument syntax to extract the field
json_bool getJSONKeyValue(json_object* json, char* key, json_object** returnedObject) {

  char* token;
  token = strtok(key, ".");

  if (token == NULL) { // Not sure what to do. let JSON-C handle it
    return json_object_object_get_ex(json, key, returnedObject);
  }

  json_object* object;
  json_bool status;

  // Is the requested object actually array?
  char* arrKey;
  arrKey = strchr(key, '[');
  if (arrKey != NULL) { // Yup

    // Extract key name
    char* actualKey = malloc(strlen(key) - strlen(arrKey));
    if (actualKey == NULL) return 0;
    int i;
    for (i = 0; i < (strlen(key) - strlen(arrKey)); i++) {
      actualKey[i] = key[i];
    } 
    actualKey[i] = '\0'; // Terminate the string     
 
    // Extract index
    char* index = malloc(strlen(arrKey) - 2);
    if (index == NULL) return 0;
    for (i = 1; i < (strlen(arrKey) - 1); i++) {
      index[i-1] = arrKey[i];
    }
    index[i-1] = '\0'; // Termiate the string

    // Turns index into a number
    int idx;
    for (i = 0, idx = 0; i < strlen(index); i++) {
      idx = (idx * 10) + (index[i] - '0');
    }

    // Does the presumed 'array' JSON object exist?
    if (json_object_object_get_ex(json, actualKey, &object)) {
      // It does. Verify it is an array
      json_type type = json_object_get_type(object);
      if (type == json_type_array) {
        object = json_object_array_get_idx(object, idx); 
        free(actualKey);
        free(index);
        status = 1; 
      } else { // Not an array
        free(actualKey);
        free(index);
        status = 0;
      }
    } else {
      // Object does not exist
      free(actualKey);
      free(index);
      status = 0;
    }
  } else {
    // Not an array, normal JSOn object
    status = json_object_object_get_ex(json, token, &object);
  }

  // Try to drill down through the JSON object
  if (!status) return status;
  token = strtok(NULL, ".");
  while (token != NULL) {

    // Is the requested object actually array?
    char* arrKey;
    arrKey = strchr(token, '[');
    if (arrKey != NULL) { // Yup

      // Extract key name
      char* actualKey = malloc(strlen(token) - strlen(arrKey));
      if (actualKey == NULL) return 0;
      int i;
      for (i = 0; i < (strlen(token) - strlen(arrKey)); i++) {
        actualKey[i] = token[i];
      } 
      actualKey[i] = '\0'; // Terminate the string     
 
      // Extract index
      char* index = malloc(strlen(arrKey) - 2);
      if (index == NULL) return 0;
      for (i = 1; i < (strlen(arrKey) - 1); i++) {
        index[i-1] = arrKey[i];
      }
      index[i-1] = '\0'; // Termiate the string

      // Turns index into a number
      int idx;
      for (i = 0, idx = 0; i < strlen(index); i++) {
        idx = (idx * 10) + (index[i] - '0');
      }

      // Does the presumed 'array' JSON object exist?
      if (json_object_object_get_ex(object, actualKey, &object)) {
        // It does. Verify it is an array
        json_type type = json_object_get_type(object);
        if (type == json_type_array) {
          object = json_object_array_get_idx(object, idx); 
          free(actualKey);
          free(index);
          status = 1; 
        } else { // Not an array
          free(actualKey);
          free(index);
          status = 0;
        }
      } else {
        // Object does not exist
        free(actualKey);
        free(index);
        status = 0;
      }
    } else {
      // Not an array, normal JSOn object
      status = json_object_object_get_ex(object, token, &object);
    }

    // This child object does not exist. Return 
    if (!status) return status;

    token = strtok(NULL, ".");  
  }

  // Pass back our found object
  *returnedObject = object; 
  return status;
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
  char* OKmessages;
  char* WARNINGmessages;
  char* CRITICALmessages;
  char* UNKNOWNmessages;
  OKmessages = malloc(1);
  WARNINGmessages = malloc(1);
  CRITICALmessages = malloc(1);
  UNKNOWNmessages = malloc(1);
  OKmessages[0] = '\0';
  WARNINGmessages[0] = '\0';
  CRITICALmessages[0] = '\0';
  UNKNOWNmessages[0] = '\0';

  // Check each key for 'validity'
  int i;
  for (i = 0; i < numberOfKeys; i++) {
    // Copy the json key string to print later
    int j;
    char* jsonKey = malloc(strlen(keys[i]));
    if (jsonKey == NULL) return 0;
    for (j = 0; j < strlen(keys[i]); j++) { jsonKey[j] = keys[i][j]; }
    jsonKey[j] = '\0';

    // Get the value
    json_object* object;
    if (getJSONKeyValue(json, keys[i], &object)) {
      json_type type  = json_object_get_type(object);
   
      // Require type to be a number before checking
      if (type != json_type_int && type != json_type_double) {

        // Form the 'pretty' info string
        char tempMessage[] = "JSON type of '%s' is '%s'. Needs to be a double or int, ";
        char* message = malloc(snprintf(NULL, 0, tempMessage, jsonKey, json_type_to_name(type)) + 1);
        sprintf(message, tempMessage, jsonKey, json_type_to_name(type));

        UNKNOWNmessages = realloc(UNKNOWNmessages, strlen(UNKNOWNmessages) + strlen(message) + 1);
        strcat(UNKNOWNmessages, message);
 
        severityLevel = UNKNOWN;
      } else {

        double value = json_object_get_double(object);
        int thisKeyStatus = OK;        

        // Form the 'pretty' info string
        char tempMessage[] = "'%s' is %g, ";
        char* message = malloc(snprintf(NULL, 0, tempMessage, jsonKey, value) + 1);
        sprintf(message, tempMessage, jsonKey, value);

        // Check Critical, if applicable
        if (arguments->criticalMin != NULL) {
        
          int min, max, inclusive;
          min = arguments->criticalMin[i];
          max = arguments->criticalMax[i];
          inclusive = arguments->criticalInclusive[i];
        
          if (inclusive) {

            if (value <= min || value >= max) {
 
              CRITICALmessages = realloc(CRITICALmessages, strlen(CRITICALmessages) + strlen(message) + 1);
              strcat(CRITICALmessages, message);

              if (severityLevel < CRITICAL) severityLevel = CRITICAL;
              thisKeyStatus = CRITICAL;
            }
          } else {
            if (value < min || value > max) {
              
              CRITICALmessages = realloc(CRITICALmessages, strlen(CRITICALmessages) + strlen(message) + 1);
              strcat(CRITICALmessages, message);

              if (severityLevel < CRITICAL) severityLevel = CRITICAL;
              thisKeyStatus = CRITICAL;
            }
          }
        }

        // Check Warning, if applicable (We set WARNING, and CRITICAL isn't set)
        if (arguments->warningMin != NULL && thisKeyStatus != CRITICAL) {
        
          int min, max, inclusive;
          min = arguments->warningMin[i];
          max = arguments->warningMax[i];
          inclusive = arguments->warningInclusive[i];
        
          if (inclusive) {
            if (value <= min || value >= max) {

              WARNINGmessages = realloc(WARNINGmessages, strlen(WARNINGmessages) + strlen(message) + 1);
              strcat(WARNINGmessages, message);

              if (severityLevel < WARNING) severityLevel = WARNING;
              thisKeyStatus = WARNING;
            }
          } else {
            if (value < min || value > max) {
      
              WARNINGmessages = realloc(WARNINGmessages, strlen(WARNINGmessages) + strlen(message) + 1);
              strcat(WARNINGmessages, message);

              if (severityLevel < WARNING) severityLevel = WARNING;
              thisKeyStatus = WARNING;
            }
          }
        }

        // Set OK string if we never set WARNING nor CRITICAL
        if (thisKeyStatus == OK) {
          OKmessages = realloc(OKmessages, strlen(OKmessages) + strlen(message) + 1);
          strcat(OKmessages, message);
        }
      }
    } else { // Object not found

      // Form the 'pretty' info string
      char tempMessage[] = "JSON key '%s' not found, ";
      char* message;
      message = malloc(snprintf(NULL, 0, tempMessage, jsonKey) + 1);
      strcpy(message, tempMessage);
   
      UNKNOWNmessages = realloc(UNKNOWNmessages, strlen(UNKNOWNmessages) + strlen(message) + 1);
      strcat(UNKNOWNmessages, message);

      severityLevel = UNKNOWN;
    }
  }

  // Print the overriding status
  switch(severityLevel) {
    case OK:
      printf("OK - ");
      break;
    
    case WARNING:
      printf("WARNING - ");
      break;

    case CRITICAL:
      printf("CRITICAL - ");
      break;

    case UNKNOWN:
    default:
      printf("UNKNOWN - ");
  }

  // Print everything in order of UNKNOWN, then CRITICAL, WARNING, and finally OK
  
  // UNKNOWN
  if (strlen(UNKNOWNmessages) > 2 && strlen(CRITICALmessages) == 0 && strlen(WARNINGmessages) == 0 && strlen(OKmessages) == 0) {
    // Print the substring minus 2 (to get rid of ", ")
    printf("%.*s", (int) strlen(UNKNOWNmessages) - 2, UNKNOWNmessages);
  } else {
    printf("%s", UNKNOWNmessages);
  }

  // CRITICAL
  if (strlen(CRITICALmessages) > 2 && strlen(WARNINGmessages) == 0 && strlen(OKmessages) == 0) {
    printf("%.*s", (int) strlen(CRITICALmessages) - 2, CRITICALmessages);
  } else {
    printf("%s", CRITICALmessages);
  }

  // WARNING
  if (strlen(WARNINGmessages) > 2 && strlen(OKmessages) == 0) {
    printf("%.*s", (int) strlen(WARNINGmessages) - 2, WARNINGmessages);
  } else {
    printf("%s", WARNINGmessages);
  }
  
  // OK
  if (strlen(OKmessages) > 2) {
    printf("%.*s", (int) strlen(OKmessages) - 2, OKmessages);
  } else {
    printf("%s", OKmessages);
  }

  printf("\n");

  free(UNKNOWNmessages);
  free(CRITICALmessages);
  free(WARNINGmessages);
  free(OKmessages);

  return severityLevel;
}
