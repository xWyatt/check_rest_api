#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

enum STATUS{OK, WARNING, CRITICAL, UNKNOWN};

// Holds our response from cURL
typedef struct cURLHTTPBody {
  char* payload;
  size_t size;
} cURLHTTPBody;

// Define our cURL handle and struct
CURL* curl;
struct cURLHTTPBody* body;

// Cleanup
void end(int exitCode) {

  free(body->payload);
  free(body);

  exit(exitCode);
}


// Callback after cURL.. does its thing
size_t write_data(void *buf, size_t size, size_t nmemb, void *userp) {

  //struct cURLHTTPBody* body = (struct cURLHTTPBody*) userp;
  
  // Size of body plus null terminator
  body->size = nmemb + 1;
  
  // Expand our payload to account for the actual payload
  body->payload = (char*) malloc(body->size);

  if (body->payload == NULL) {

    // Malloc failed. Signal to handler to use HTTP Status code
    body->size = 0;
  } else {

    // Copy payload and set null terminiating byte
    memcpy(body->payload, buf, body->size - 1);
    body->payload[body->size] = '\0';
  }

  return nmemb;
}

void* callAPI(char* apiUrl) {

  CURLcode res;

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, apiUrl);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);    

    res = curl_easy_perform(curl);

    // This is the only place, besides main() that returns
    if (res != CURLE_OK) {
      printf("UNKNOWN - cURL call resulted in error: %s\n", curl_easy_strerror(res));
      end(UNKNOWN);
    }

    curl_easy_cleanup(curl);
    
    return curl;
  }

  return 0;
}

// Check passed parameters. Only verifies the correct switches exist
int checkParams(int argc, char* argv[]) {

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


// Program Entry Point
int main(int argc, char** argv) {

  // Initialize our body
  body = malloc(sizeof(struct cURLHTTPBody));

  if (!checkParams(argc, argv)) return CRITICAL;

  // Returns a CURL thing that we can use to get info from the API call
        CURL *curl = (CURL*) callAPI(argv[argc - 1]);

  // Extract the HTTP Response Code
  long httpResponseCode;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpResponseCode);

  if (httpResponseCode == 200) {
    printf("Output: %s", body->payload);
    printf("OK - API Returned 200-OK\n");
    end(OK);
  }

  
  if (httpResponseCode < 500) {
    printf("WARNING - Unexpected HTTP response code: %d\n", httpResponseCode);
    end(WARNING);
  }

  if (httpResponseCode > 500) {
    printf("CRITICAL - HTTP Reponse Code > 500: %d\n", httpResponseCode);
    end(CRITICAL);
  }

  // Not sure how we would get here
  printf("UNKNOWN - Something weird happened. HTTP Reponse Code: %d\n", httpResponseCode);
  end(UNKNOWN);      
}
