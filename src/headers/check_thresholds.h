#ifndef CHECK_THRESHOLDS
#define CHECK_THRESHOLDS

int checkHTTPStatusCode(CURL* curl);
int checkHTTPBody(json_object* json, argValues* arguments);

#endif
