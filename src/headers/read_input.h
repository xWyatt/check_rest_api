#ifndef CHECK_INPUT
#define CHECK_INPUT

/* If adding a char* to this list instantiate that value as NULL in read_input.c and free() it in check_rest_api.c */
typedef struct argValues {
  char* hostname;
  char* username;
  char* password;
  char** keys;
  int numberOfKeys;
  double* warningMax;
  double* warningMin;
  int* warningInclusive;
  double* criticalMax;
  double* criticalMin;
  int* criticalInclusive;
  long timeout;
  int insecureSSL;
  char* header;
  int debug;
  int httpMethod;
} argValues;

struct argValues* argVals;

int validateArguments(int argc, char** argv);

#endif
