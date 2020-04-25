#ifndef CHECK_INPUT
#define CHECK_INPUT

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
} argValues;

struct argValues* argVals;

int validateArguments(int argc, char** argv);

#endif
