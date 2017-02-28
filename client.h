#include "utils.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <signal.h>

#define SERVER_PORT     6767
#define BUF_SIZE        2048
#define MAXHOSTNAMELEN  1024
#define MAX_STATION_ID   128
#define ITER_COUNT         1
#define GET_MODE           1
#define PUT_MODE           2
#define USER_MODE          3
#define STACK_SIZE       200
#define MAX_THREAD_NUMBER 10

typedef struct thread_info{
    pthread_t thread_id;
    int thread_num;
} Thread_info;

typedef struct stack_element{
    char *buffer;
    struct sockaddr_in server_addr;
} Element;

typedef struct stack_s{
    int stack_pointer;
    Element elements[STACK_SIZE];
} Stack;

Stack request_stack;

pthread_mutex_t stack_mutex;
pthread_cond_t new_request;