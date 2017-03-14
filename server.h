#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "utils/utils.h"
#include "kissdb.h"
#include "utils/server_stack.h"

#define MY_PORT                 6767
#define BUF_SIZE                1160
#define KEY_SIZE                 128
#define HASH_SIZE               1024
#define VALUE_SIZE              1024
#define MAX_PENDING_CONNECTIONS   10
#define MAX_THREAD_NUMBER         10

// Enable (1) / Disable (0) debugging printouts.
#define DEBUG                      1

// Definition of the operation type.
typedef enum operation {
  PUT,
  GET
} Operation;

// Definition of the thread_info type.
typedef struct thread_info{
  pthread_t thread_id;
  int thread_num;
} Thread_info;

// Definition of the request.
typedef struct request {
  Operation operation;
  char key[KEY_SIZE];
  char value[VALUE_SIZE];
} Request;

// Definition of the database.
KISSDB *db = NULL;

pthread_mutex_t stack_mutex; 
pthread_mutex_t completed_requests_mutex;
pthread_cond_t new_request;

int i;
struct timeval tv; //Definition of the timeval struct.

int completed_requests = 0; // Definition of the value that counts all the completed requests.
long total_waiting_time; // Definition of the value that holds the total waiting time.
long total_service_time; // Definition of the value that holds the total service time.

void signal_handler(int signo);
Request *parse_request(char *buffer);
void * worker();
void * producer();
