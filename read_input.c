#include <stdio.h>
#include <string.h>

#include "read_input.h"

int validateArguments(int argc, char** argv) {

  char helpMessage[] = "Usage: check_rest_api [OPTIONS..] <api_url>\n\nOptions:\n \
  -V, --http_verb   HTTP Verb (GET/POST/PUT/DELETE, etc.) \n \
  -O, --expected_output The output to compare to \n \
  \nReport Bugs to: teeterwyatt@gmail.com\n";

  if (argc % 2 > 0) {
    printf("<api_url> required!\n\n");
    printf(helpMessage);
    return 0;
  }

  // Verify all 
  int i;
  for (i = 1; i < argc - 1; i += 2) {

    char* arg = (char*) argv[i];

    if (strcmp(arg, "-V") == 0 || strcmp(arg, "--http_verb") == 0) continue;

    if (strcmp(arg, "-O") == 0 || strcmp(arg, "--expected_output") == 0) continue;

    if (strcmp(arg, "-H") == 0 || strcmp(arg, "--help") == 0) {
      printf(helpMessage);
      return 0;
    }

    printf("Bad argument: %s\n\n", arg);
    printf(helpMessage);
    return 0;
  }
  
  return 1;
}

// Parses out arguments and returns them in an array
//argValues* parseArguments(int argc, char** argv) {
  //TODO
//}
