#ifndef CHECK_INPUT
#define CHECK_INPUT

typedef struct argValues {
  char arg;
  char* value;
} argValues;

int validateArguments(int argc, char** argv);
argValues* parseArguments(int argc, char** argv);

#endif
