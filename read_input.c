#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include "read_input.h"
#include "check_rest_api.h"

// XXX: Update Help message
char helpMessage[] = "Usage: check_rest_api [OPTIONS..]\n\nOptions:\n\
  -h, --help\n\
    Print detailed help screen\n\
  -V, --version\n\
    Print Version information\n\
  -H, --hostname=HOSTNAME\n\
    The hostname, or IP address of the API you want to check\n\
  -P, --port=PORT\n\
    Optional; the port for the API you want to check. Defaults to port 80\n\
  -p, --protocol=PROTOCOL\n\
    Optional; the protocol for the API you want to check. Possible values are 'HTTP' or 'HTTPS'\n\
  -K, --key=jsonKey\n\
    Optional; the name of the JSON key you want to check. The value of this key must be a number\n\
     If not provided, check_rest_api will check the HTTP status code. Anything < 400 will return OK,\n\
     Anthing >=400 and < 500 will return WARNING, and >= 500 will return CRITICAL.\n\
   -w, --warning=warningThreshold\n\
    Optional, returns WARNING if --key is outside the range of this. Disabled by default\n\
  -c, --critical=criticalThreshold\n\
    Optional, returns CRITICAL if --key is outsid ethe range of this. Disabled by default\n\
  -t, --timeout=timeoutValue\n\
    Optional, seconds before connection times out (default: 10 seconds)\n\
  \nReport Bugs to: teeterwyatt@gmail.com\n";

char version[] = "check_rest_api version: 0.0.1\n";

// I'm thinking this needs externalized
// This function is dedicated to parsing out [@]start:end from -w and -c
// If successful, it stores the value in the global argValues structure
int parseWarningOrCriticalValues(char* value, char type) {

  if (type != 'w' && type != 'c') {
    return 0;
  }

  char* switchType;

  if (type == 'w') {
    switchType = malloc(14);
    if (switchType == NULL) {
      printf("Error allocating heap space!\n");
      return 0; 
    }
    strcpy(switchType, "-w, --warning");
  } else if (type == 'c') {
    switchType = malloc(15);
    if (switchType == NULL) {
      printf("Error allocating heap space!\n");
      return 0;
    }
    strcpy(switchType, "-c, --critical");
  }

  int numberOfTokens = 0;
  char* token;
  char* innerToken;
  char* outerLoopSavePtr;
  char* innerLoopSavePtr;
  token = strtok_r(value, ",", &outerLoopSavePtr);
  if (token == NULL) {
    printf("Value required for %s\n\n%s", switchType, helpMessage);
    return 0;
  }
      
  while (token != NULL) {
    if (numberOfTokens > argVals->numberOfKeys) {
      break;
    } 

    // Parse out the user input
    // Grammar is [@][<min>:]<max>
    //  <min> => 0,1,2,3,4,5,6,7,8,9,~
    //  <max> => 0,1,2,3,4,5,6,7,8,9
    //  '<min>:' can be excluded- <min> is assumed 0 in that case
    //
    double min = 0, max = 0;
    int maxSet = 0, numberOfColonTokens = 0, inclusive = 0;
    int endsWithColon = 0;
    endsWithColon = token[strlen(token) - 1] == ':';
    innerToken = strtok_r(token, ":", &innerLoopSavePtr);
    while (innerToken != NULL) {

      // Only allow one ':'
      if (numberOfColonTokens > 1) {
        printf("Invalid token '%s' in value '%s' for %s\n\n%s", innerToken, token, switchType, helpMessage);
        return 0;
      }

      // Parse Min
      if (numberOfColonTokens == 0) {
        if (innerToken[0] == '@') {
          inclusive = 1;
        } 

        else if (innerToken[0] == '~') {
          min = -INFINITY;
          if (strlen(innerToken) != 1) {
            printf("Invalid token '%c' in value '%s' for %s\n\n%s", innerToken[1], token, switchType, helpMessage);
            return 0;
          }
        }

        // Build digit
        else {
          int j;
          for (j = inclusive ? 1 : 0; j < strlen(innerToken); j++) {
            if (!isdigit(innerToken[j])) {
              printf("Invalid token '%c' in value '%s' for %s\n\n%s", innerToken[j], token, switchType, helpMessage);
              return 0;
            }
              
            // Build the double from char
            min = (min * 10) + (innerToken[j] - '0');
          }
        }
      }

      // Parse Max
      if (numberOfColonTokens == 1) {
        int j;
        for (j = 0; j < strlen(innerToken); j++) {
          if (!isdigit(innerToken[j])) {
            printf("Invalid token '%c' in value '%s' for %s\n\n%s", innerToken[j], token, switchType, helpMessage);
            return 0;
          }

          // Build double from consecutive chars
          max = (max * 10) + (innerToken[j] - '0');
        }
            
        maxSet = 1;
      }

      // This will not get above 1 due to the guard clause at the beginning of the while loop
      ++numberOfColonTokens;
      innerToken = strtok_r(NULL, ":", &innerLoopSavePtr);
    }

    // Two cases. 'x:' or 'x'. Former '< x', latter '< 0 or > x'
    if (!maxSet) {
      if (endsWithColon) {
        max = INFINITY;
      } else {
        max = min;
        min = 0;
      }
    }

    if (max < min || min == max) {
      printf("Minimum threshold must be less than Maximum threshold\n");
      return 0;
    }


    // Allocate space for this in the array
    if (type == 'w') {
      if (argVals->warningMin == NULL) {
        argVals->warningMin = malloc(sizeof(double));
        argVals->warningMax = malloc(sizeof(double));
        argVals->warningInclusive = malloc(sizeof(int));
      } else {
        argVals->warningMin = realloc(argVals->warningMin, sizeof(double) * (numberOfTokens + 1));
        argVals->warningMax = realloc(argVals->warningMax, sizeof(double) * (numberOfTokens + 1));
        argVals->warningInclusive = realloc(argVals->warningInclusive, sizeof(int) * (numberOfTokens + 1));
      }

      if (argVals->warningMin == NULL || argVals->warningMax == NULL || argVals->warningInclusive == NULL) {
        printf("Error allocating space for warning array!\n");
        return 0;
      }

      argVals->warningMin[numberOfTokens] = min;
      argVals->warningMax[numberOfTokens] = max;
      argVals->warningInclusive[numberOfTokens] = inclusive;

    } else if (type == 'c') {
      if (argVals->criticalMin == NULL) {
        argVals->criticalMin = malloc(sizeof(double));
        argVals->criticalMax = malloc(sizeof(double));
        argVals->criticalInclusive = malloc(sizeof(int));
      } else {
        argVals->criticalMin = realloc(argVals->criticalMin, sizeof(double) * (numberOfTokens + 1));
        argVals->criticalMax = realloc(argVals->criticalMax, sizeof(double) * (numberOfTokens + 1));
        argVals->criticalInclusive = realloc(argVals->criticalInclusive, sizeof(int) * (numberOfTokens + 1));
      }

      if (argVals->criticalMin == NULL || argVals->criticalMax == NULL || argVals->criticalInclusive == NULL) {
        printf("Error allocating space for critical array!\n");
        return 0;
      }

      argVals->criticalMin[numberOfTokens] = min;
      argVals->criticalMax[numberOfTokens] = max;
      argVals->criticalInclusive[numberOfTokens] = inclusive;

    }

    ++numberOfTokens;
    token = strtok_r(NULL, ",", &outerLoopSavePtr);
  }

  // This assumes -K is always before -w/-c. I don't like that
  if (numberOfTokens != argVals->numberOfKeys) {
    printf("Number of %s values must match number of -K, --key values!\n\n%s", switchType, helpMessage);
    return 0;
  }

  free(switchType);

  return 1;
}

// Validates input arguments, and saves them in the struct... somewhere
int validateArguments(int argc, char** argv) {

  // argVals defined in header
  argVals = malloc(sizeof(struct argValues));
  argVals->hostname = NULL;
  argVals->port = NULL;
  argVals->protocol = NULL;
  argVals->keys = NULL;
  argVals->numberOfKeys = 0;
  argVals->warningMax = NULL;
  argVals->warningMin = NULL;
  argVals->warningInclusive = NULL;
  argVals->criticalMin = NULL;
  argVals->criticalMax = NULL;
  argVals->criticalInclusive = NULL;
  argVals->timeout = 10;

  // Require arguments
  if (argc == 1) {
    printf("Arguments Required!\n\n%s", helpMessage);
    return 0;
  }

  // Verify all arguments passed
  int i;
  for (i = 1; i < argc; i += 2) {

    char* arg = (char*) argv[i];
    char* nextArg = (char*) argv[i + 1];

    // Remove '=' from argument if it exists
    int j;
    for (j = 0; j < strlen(arg); j++) {
      if (arg[j] == '=') arg[j] = ' ';
    }

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      printf(version);
      printf(helpMessage);
      return 0;
    }

    if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0){
      printf(version);
      return 0;
    };
   
    // Help message
    if (strcmp(arg, "-H") == 0 || strcmp(arg, "--hostname") == 0) {
      if (nextArg[0] == '-') {
        printf("Invalid value for -H, --hostname. Must be an IP address or URL\n\n%s", helpMessage);
        return 0;
      }

      // Save hostname
      argVals->hostname = (char*) malloc(strlen(nextArg) * sizeof(char) + 1);
      if (argVals->hostname == NULL) return 0;

      strcpy(argVals->hostname, nextArg);    
      
      continue;
    }

    if (strcmp(arg, "-P") == 0 || strcmp(arg, "--port") == 0) {
      // TODO better error checking
      if (!isdigit(nextArg)) {
        printf("Invalid value for -P, --port. Must be a valid port number\n\n%s", helpMessage);
        return 0;
      }

      // Save port
      argVals->port = malloc(sizeof(nextArg));
      if (argVals->hostname == NULL) return 0;

      strcpy(argVals->port, nextArg);

      continue;
    }

    if (strcmp(arg, "-p") == 0 || strcmp(arg, "--protocol") == 0) {

      // Turn port uppercase for comparison
      int j;
      for (j = 0; j < strlen(nextArg); j++) nextArg[j] = toupper(nextArg[j]);

      if (strcmp(nextArg, "HTTP") != 0 && strcmp(nextArg, "HTTPS") != 0) {
        printf("%s not a valid protocol. Only HTTP and HTTPS are allowed!\n", nextArg);
        return 0;
      }

      // Save protocol
      argVals->protocol = malloc(sizeof(nextArg));
      if (argVals->protocol == NULL) return 0;

      strcpy(argVals->protocol, nextArg);

      continue;
    }

    if (strcmp(arg, "-K") == 0 || strcmp(arg, "--key") == 0) {
      
      if (nextArg[0] == '-') {
        printf("Invalid value for -K, --key. This must be a comma-delimited list of strings.\n\n%s", helpMessage);
        return 0;
      }

      char* token;
      token = strtok(nextArg, ",");
      if (token == NULL) {
        printf("Value required for -k, --key!\n\n%s", helpMessage);
        return 0;
      }

      while (token != NULL) {       

        // Reize array 
        if (argVals->keys == NULL) {
          argVals->keys = malloc(sizeof(char*));
          if (argVals->keys == NULL) {
            printf("Error allocating array for argVals->keys!\n");
            return 0;
          }
        } else {
          argVals->keys = realloc(argVals->keys, (argVals->numberOfKeys + 1) * sizeof(char*));
          if (argVals->keys == NULL) {
            printf("Error re-sizing array for argVals->keys!\n");
            return 0;
          }
        }

        argVals->keys[argVals->numberOfKeys] = malloc(sizeof(token) * sizeof(char));
        if (argVals->keys[argVals->numberOfKeys] == NULL) {
           printf("Error allocating space for argVals->keys!\n");
         return 0;
        }   

        strcpy(argVals->keys[argVals->numberOfKeys], token);
        ++argVals->numberOfKeys;

        token = strtok(NULL, ","); 
      }
  
      continue;
    }

    if (strcmp(arg, "-w") == 0 || strcmp(arg, "--warning") == 0) {

      if (nextArg[0] == '-') {
        printf("Invalid value for -w, --warning. See Nagios Plugin documentation\n\n%s", helpMessage);
        return 0;
      }

      if (!parseWarningOrCriticalValues(nextArg, 'w')) {
        return 0;
       }

      continue;
    }

    if (strcmp(arg, "-c") == 0 || strcmp(arg, "--critical") == 0) {
      
      if (nextArg[0] == '-') {
        printf("Invalid value for -c, --critical. See Nagios Plugin documentation\n\n%s", helpMessage);
        return 0;
      }  

      if (!parseWarningOrCriticalValues(nextArg, 'c')) {
        return 0;
      }

      continue;
    }

    if (strcmp(arg, "-t") == 0 || strcmp(arg, "--timeout") == 0) {

      argVals->timeout = 0;

      int i;
      for (i = 0; i < strlen(nextArg); i++) {
        if (!isdigit(nextArg[i])) {
          printf("Invalid value for -t, --timeout. This must be a number\n\n%s", helpMessage);
          return 0;
        }

        // Build int from char*
        argVals->timeout = (argVals->timeout * 10) + (nextArg[i] - '0');
      }

      continue;
    }

    printf("Bad argument: %s\n\n", arg);
    printf(helpMessage);
    return 0;
  }
  
  return 1;
}
