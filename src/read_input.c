#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#include "headers/read_input.h"
#include "headers/check_rest_api.h"
extern struct argValues* argVals;
// Help Message
char helpMessage[] = "Usage: check_rest_api [OPTIONS..]\n\nOptions:\n\
  -h, --help\n\
    Print detailed help screen\n\
  -V, --version\n\
    Print Version information\n\
  -b, --auth-basic <username>:<password>\n\
    Uses HTTP Basic authentication with provided <username> and <password>\n\
  -bf, --auth-basic-file <filename> \n\
    Uses HTTP Basic authentication and takes the required 'username:password' from the file provided\n\
    This file should only have one line that contains text in the format '<username>:<password>' (Excluding quotes)\n\
  -H, --hostname HOSTNAME\n\
    The hostname, or IP address of the API you want to check\n\
  -m, --http-method METHOD\n\
    Optional; the HTTP method for the API call. Only 'GET', 'POST', and 'PUT' are supported at this time.\n\
    If omitted 'GET' is assumed.\n\
  -K, --key jsonKey\n\
    Optional; a comma delimited list of JSON keys to check. The value of this key must be a number\n\
     If not provided, check_rest_api will check the HTTP status code. Anything < 400 will return OK,\n\
     Anthing >=400 and < 500 will return WARNING, and >= 500 will return CRITICAL.\n\
  -w, --warning warningThreshold\n\
    Optional; a comma delimited list of WARNING thresholds that map to the corresponding -K, --key (JSON key)\n\
      Returns WARNING if the corresponding --key is outside the defined -w range\n\
  -c, --critical criticalThreshold\n\
    Optiona; a comma delimited list of CRITICAL thresholds that map to the corresponding -K, --key (JSON key)\n\
      Returns CRITICAL if the corresponding --key is outisde the defined -c range\n\
  -t, --timeout timeoutValue\n\
    Optional, seconds before connection times out (default: 10 seconds)\n\
  -D, --header\n\
    Optional HTTP Header(s)\n\
  -d, --debug\n\
    Enable trace mode for CURL communication\n\
  -k, --insecure\n\
    Disables checking peer's SSL certificate (if using SSL/HTTPS). Not recommended to use\n\
  \nReport Bugs to: teeterwyatt@gmail.com\n";

char version[] = "check_rest_api version: 1.1.1\n";

// Used to make a string uppercase
void toUpper(char* str) {
  int character;
  character = 0;

  while (str[character]) {
    str[character] = toupper(str[character]);
    character++;
  } 

  return;
}

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
    //  <min> => 0,1,2,3,4,5,6,7,8,9,~,.
    //  <max> => 0,1,2,3,4,5,6,7,8,9,.
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
          int j, inDecimal = 0;
          for (j = inclusive ? 1 : 0; j < strlen(innerToken); j++) {
            if (!isdigit(innerToken[j]) && (inDecimal && innerToken[j] == '.')) {
              printf("Invalid token '%c' in value '%s' for %s\n\n%s", innerToken[j], token, switchType, helpMessage);
              return 0;
            }
 
	    if (innerToken[j] == '.') {
              inDecimal = 1;
	    } else {
	      // Build int from consecutive chars                        
              min = (min * 10) + (innerToken[j] - '0');

	      if (inDecimal) ++inDecimal;
            }
          }
          // Turn our int into a double if we had decimals
          //   Note we decrease inDecimal by one (if we had decimals) so
          //   the while loop works as expected- otherwise we would be off be one
          inDecimal = inDecimal ? inDecimal - 1 : 0;
          while (inDecimal) {
            min = min / 10;
            --inDecimal;
          }
        }
      }

      // Parse Max
      if (numberOfColonTokens == 1) {
        int j, inDecimal = 0;
        for (j = 0; j < strlen(innerToken); j++) {
          if (!isdigit(innerToken[j]) && (inDecimal && innerToken[j] == '.')) {
            printf("Invalid token '%c' in value '%s' for %s\n\n%s", innerToken[j], token, switchType, helpMessage);
            return 0;
          }

          if (innerToken[j] == '.') {
            inDecimal = 1;
          } else {
            // Build int from consecutive chars
            max = (max * 10) + (innerToken[j] - '0');

            if (inDecimal) ++inDecimal;
          }
        }
        // Turn our int into a double if we had decimals
        //  Note we decrease inDecimal by one (if we had decimals) so 
        //  the while loop works as expected- otherwise we would be off by one
        inDecimal = inDecimal ? inDecimal - 1 : 0;
        while (inDecimal) {
          max = max / 10;
          --inDecimal;
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
  argVals->username = NULL;
  argVals->password = NULL;
  argVals->keys = NULL;
  argVals->numberOfKeys = 0;
  argVals->warningMax = NULL;
  argVals->warningMin = NULL;
  argVals->warningInclusive = NULL;
  argVals->criticalMin = NULL;
  argVals->criticalMax = NULL;
  argVals->criticalInclusive = NULL;
  argVals->timeout = 10L;
  argVals->insecureSSL = 0;
  argVals->httpMethod = 0;
  argVals->debug = 0;
  argVals->header = NULL;

  // Require arguments
  if (argc == 1) {
    printf("Arguments Required!\n\n%s", helpMessage);
    return 0;
  }

  // Verify all arguments passed
  int i;
  int lastArgHadNoInput = 0;
  for (i = 1; i < argc; i += 2) {

    // For --insecure and the like that have no nextArg
    if (lastArgHadNoInput == 1) {
      i = i-1;
      lastArgHadNoInput = 0;
    }

    char* arg = (char*) argv[i];
    char* nextArg = (char*) argv[i + 1];

    // Remove '=' from argument if it exists
    int j;
    for (j = 0; j < strlen(arg); j++) {
      if (arg[j] == '=') arg[j] = ' ';
    }

    // Help Message
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      printf("%s", version);
      printf("%s", helpMessage);
      return 0;
    }

    // Version
    if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0){
      printf("%s", version);
      return 0;
    };

    // Basic Auth - CLI
    if (strcmp(arg, "-b") == 0 || strcmp(arg, "--auth-basic") == 0) {
      if (nextArg[0] == '-') {
        printf("Invalid value for -b, --auth-basic. Must be a string of <username>:<password>\n\n%s", helpMessage);
        return 0;
      }

      char* password = strpbrk(nextArg, ":");

      if (password == NULL) {
        printf("Invalid value for -b, --auth-basic. Must be a string of <username>:<password>. The ':' is literal.\n\n%s", helpMessage);
        return 0;
      }

      // password[0] is the ':' of <username>:<password>. ':' designates the end of the username
      password[0] = '\0';
      password = password + 1; // Skip over the ':'
  
      argVals->username = (char *) malloc(strlen(nextArg) * sizeof(char) + 1);
      argVals->password = (char *) malloc(strlen(password) * sizeof(char) + 1);
      if (argVals->username == NULL || argVals->password == NULL) return 0;

      strcpy(argVals->username, nextArg);
      strcpy(argVals->password, password);
   
      continue;
    }

    // Basic Auth - File
    if (strcmp(arg, "-bf") == 0 || strcmp(arg, "--auth-basic-file") == 0) {
      if (nextArg[0] == '-') {
        printf("Invalid value for -bf, --auth-basic-file. Must be a file path.\n\n%s", helpMessage);
        return 0;
      }

      // Verify file exists and we can read it
      if (access(nextArg, R_OK) != -1) {
      
        // Open File
        FILE *basicAuthFile = (FILE*) fopen(nextArg, "r");

        // Read line into buffer
        char* buffer = NULL;
        size_t len;
        if (getline(&buffer, &len, basicAuthFile)) {
          // Remove newline from line
          buffer[strlen(buffer) - 1] = '\0';

          // Split into username and password
          char* password = strpbrk(buffer, ":");

          if (password == NULL) {
            printf("Bad data in file '%s'. Verify the file has only one line and contains only '<username>:<password>'\n\n%s", nextArg, helpMessage); 
          }

          password[0] = '\0';
          password = password + 1;

          argVals->username = (char *) malloc(strlen(buffer) * sizeof(char) + 1);
          argVals->password = (char *) malloc(strlen(password) * sizeof(char) + 1);
          if (argVals->username == NULL || argVals->password == NULL) {
            printf("Bad data in file '%s'. Verify the file has only one line and contains only '<username>:<password>'\n\n%s", nextArg, helpMessage); 
            return 0;
          }
          
          strcpy(argVals->username, buffer);
          strcpy(argVals->password, password);


          // Cleanup
          fclose(basicAuthFile);
          free(buffer);

          continue;
        } else {
          // Error; cleanup
          fclose(basicAuthFile);
          free(buffer);
          printf("No data in file '%s'. Verify the file has only one line and contains only '<username>:<password>'\n\n%s", nextArg, helpMessage); 
          return 0;
        }
      } else {
        printf("Cannot read from file '%s' to retrieve Basic Auth credentials. Verify the file exists and has read permission.\n\n%s", nextArg, helpMessage);
        return 0;
      }
    }

    // Hostname
    if (strcmp(arg, "-H") == 0 || strcmp(arg, "--hostname") == 0) {
      if (nextArg[0] == '-') {
        printf("Invalid value for -H, --hostname. Must be an IP address or URL\n\n%s", helpMessage);
        return 0;
      }

      argVals->hostname = (char*) malloc(strlen(nextArg) * sizeof(char) + 1);
      if (argVals->hostname == NULL) return 0;

      strcpy(argVals->hostname, nextArg);    
      
      continue;
    }

    // Optional HTTP Header
    if (strcmp(arg, "-D") == 0 || strcmp(arg, "--header") == 0) {
       argVals->header = (char*) malloc(strlen(nextArg) * sizeof(char) + 1);

      if (argVals->header == NULL) return 0;

      strcpy(argVals->header, nextArg);

      continue;
    }

    // Optional Debug ouput
    if (strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0) {
     argVals->debug = 1;

     i--; // Since no second parm, move i back one to get the next arg
     continue;

    }

    // JSON Key
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

        argVals->keys[argVals->numberOfKeys] = malloc((strlen(token) + 1) * sizeof(char));
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

    // Warning threshold
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

    // Critical threshold
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

    // Timeout
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

    // Insecure SSL
    if (strcmp(arg, "-k") == 0 || strcmp(arg, "--insecure") == 0) {
      // Set our insecure SSL Flag
      argVals->insecureSSL = 1;

      // We don't use nextArg here
      lastArgHadNoInput = 1;

      i--; // Since no second parm, move i back one to get the next arg
      continue;
    }

    // HTTP Method
    if (strcmp(arg, "-m") == 0 || strcmp(arg, "--http-method") == 0) {
      toUpper(nextArg);
    
      if (strcmp(nextArg, "GET") == 0) {
        argVals->httpMethod = 0;
      } else if (strcmp(nextArg, "POST") == 0) {
        argVals->httpMethod = 1;
      } else if (strcmp(nextArg, "PUT") == 0) {
        argVals->httpMethod = 2;
      } else { // Invalid value
        printf("Invalid value for -m/--http-method. This must be 'GET', 'POST', or 'PUT'\n\n%s", helpMessage);
        return 0;
      }
      
      continue;
    }

    // Bad argument
    printf("Bad argument: %s\n\n%s", arg, helpMessage);
    return 0;
  }
  
  return 1;
}
