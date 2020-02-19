#ifndef CHECK_INPUT
#define CHECK_INPUT

typedef struct argValues {
  char* hostname;
  char* port;
  char* protocol;
  char** keys;
  int numberOfKeys;
  double* warningMax;
  double* warningMin;
  int* warningInclusive;
  double* criticalMax;
  double* criticalMin;
  int* criticalInclusive;
  int timeout;
} argValues;

struct argValues* argVals;

int validateArguments(int argc, char** argv);

#endif
