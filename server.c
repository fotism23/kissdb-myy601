/* server.c

   Sample code of
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina

   (c) S. Anastasiadis, G. Kappes 2016

*/
#include "server.h"

/* @name is_empty - Checks if the requests Stack is empty.
 * 
 * @return int 1 on empty. 0 on not-empty.
 */
int is_empty(){
    if (request_stack.stack_pointer == 0){
        return 1;
    }
    return 0;
}

/* @name is_full - Checks if the requests Stack is full.
 * 
 * @return int 1 on full. 0 on not-full.
 */
int is_full(){
    if (request_stack.stack_pointer == STACK_SIZE){
        return 1;
    }
    return 0;
}


/* @name push - Pushes an element to the request stack.
 * 
 * @return int 1 on success. 0 on Error.
 */
int push(Element e){
    if (is_full() == 0){
        request_stack.requests[request_stack.stack_pointer] = e;
        request_stack.stack_pointer++;
        return 1;
    }
    return 0;
}

/* @name pop - Pops an element from the request stack.
 * 
 * @return First request on the stack. NULL on Error.
 */
Element *pop(){
    if (is_empty() == 0){
        request_stack.stack_pointer--;
        return &request_stack.requests[request_stack.stack_pointer];
    }
    return NULL;
}

/*
 * @name signal_handler - Overrides the default functionality of signals from the console.
 * @param signo: Signal Id.
 *   
 * @return Void
 */
void signal_handler(int signo){
    signal(SIGTSTP , signal_handler);
    if (signo == SIGTSTP){
        double avg_waiting_time = (double) total_waiting_time / (double) completed_requests;
        double avg_service_time = (double) total_service_time / (double) completed_requests;
        fprintf(stderr, "Total completed requests : %d\n", completed_requests);
        fprintf(stderr, "Average waiting time : %f\n", avg_waiting_time);
        fprintf(stderr, "Average service time : %f\n", avg_service_time);
    }
}

/**
 * @name parse_request - Parses a received message and generates a new request.
 * @param buffer: A pointer to the received message.
 *
 * @return Initialized request on Success. NULL on Error.
 */
Request *parse_request(char *buffer) {
  char *token = NULL;
  Request *req = NULL;

  // Check arguments.
  if (!buffer)
    return NULL;

  // Prepare the request.
  req = (Request *) malloc(sizeof(Request));
  memset(req->key, 0, KEY_SIZE);
  memset(req->value, 0, VALUE_SIZE);

  // Extract the operation type.
  token = strtok(buffer, ":");
  if (!strcmp(token, "PUT")) {
    req->operation = PUT;
  } else if (!strcmp(token, "GET")) {
    req->operation = GET;
  } else {
    free(req);
    return NULL;
  }

  // Extract the key.
  token = strtok(NULL, ":");
  if (token) {
    strncpy(req->key, token, KEY_SIZE);
  } else {
    free(req);
    return NULL;
  }

  // Extract the value.
  token = strtok(NULL, ":");
  if (token) {
    strncpy(req->value, token, VALUE_SIZE);
  } else if (req->operation == PUT) {
    free(req);
    return NULL;
  }
  return req;
}

/**
 * @name worker - Responsible for serving the request placed on the request_stack.
 *
 * @return Void pointer.
 */
//worker thread routine.
//serves all of GET requests.
void * worker(){
    while(1){
        pthread_mutex_lock(&stack_mutex);//Lock shared variables (request_stack).
        pthread_cond_wait(&new_request , &stack_mutex);

        //Pop element from stack.
        Element *e = pop();

        request_flag = 0;
        
        pthread_mutex_unlock(&stack_mutex);//Unlock shared variables.
        printf("poped element fd:%d start_time:%lo\n",e->fd,e->start_time );

        if (e){
            printf("Element poped from stack fd:%d start_time:%lo\n" ,e->fd , e->start_time );
            //calculate request waiting_time in request_stack.
            gettimeofday(&tv , NULL);
            long waiting_time = tv.tv_usec - e->start_time;
            long service_start = tv.tv_usec;

            int new_fd;
            char response_str[BUF_SIZE];
            char request_str[BUF_SIZE];

            Request *request = NULL;

            memset(response_str, 0, BUF_SIZE);
            memset(request_str, 0, BUF_SIZE);

            new_fd = e->fd;
            request = e->req;
            //Execute GET request.
            if (request){
                if (KISSDB_get(db, request->key, request->value))
                    sprintf(response_str, "GET ERROR\n");
                else
                    sprintf(response_str, "GET OK: %s\n", request->value);
                write_str_to_socket(new_fd , response_str , strlen(response_str));
            }
            close(new_fd);
            
            //calculate request service_time.
            gettimeofday(&tv , NULL);
            long service_time = tv.tv_usec - service_start;
            
            pthread_mutex_lock(&completed_requests_mutex);
            completed_requests ++;
            total_service_time += service_time;
            total_waiting_time += waiting_time;
            pthread_mutex_unlock(&completed_requests_mutex);
        }
    }
    pthread_exit(NULL);
}

void * producer(){
    int socket_fd;              // listen on this socket for new connections
    int new_fd;                 // use this socket to service a new connection
    socklen_t clen;
    struct sockaddr_in server_addr,  // my address information
                     client_addr;  // connector's address information

    // Create socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
      ERROR("socket()");

    // Ignore the SIGPIPE signal in order to not crash when a
    // client closes the connection unexpectedly.

    // create socket adress of server (type, IP-adress and port number)
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // any local interface
    server_addr.sin_port = htons(MY_PORT);

    // Bind socket to address
    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
     ERROR("bind()");

    // Start listening to socket for incomming connections
    listen(socket_fd, MAX_PENDING_CONNECTIONS);
    fprintf(stderr, "(Info) main: Listening for new connections on port %d ...\n", MY_PORT);
    clen = sizeof(client_addr);

    // Allocate memory for the database.
    if (!(db = (KISSDB *)malloc(sizeof(KISSDB)))) {
      fprintf(stderr, "(Error) main: Cannot allocate memory for the database.\n");
      pthread_exit((void *) 0);
    }

    // Open the database.
    if (KISSDB_open(db, "mydb.db", KISSDB_OPEN_MODE_RWCREAT, HASH_SIZE, KEY_SIZE, VALUE_SIZE)) {
      fprintf(stderr, "(Error) main: Cannot open the database.\n");
      pthread_exit((void *) 0);
    }

    // Main loop: wait for new connection/requests
    while (1) {
        // Wait for incomming connection
        if ((new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &clen)) == -1) {
            ERROR("accept()");
        }

        // Got connection, serve request
        fprintf(stderr, "(Info) main: Got connection from '%s'\n", inet_ntoa(client_addr.sin_addr));

        char response_str[BUF_SIZE], request_str[BUF_SIZE];
        int numbytes = 0;
        Request *request = NULL;

        memset(response_str , 0 , BUF_SIZE);
        memset(request_str , 0 , BUF_SIZE);

        numbytes = read_str_from_socket(new_fd , request_str , BUF_SIZE);
        if (numbytes){
            request = parse_request(request_str);
            if (request){
                if (request->operation == GET){
                    gettimeofday(&tv , NULL);
                    
                    // Definition for the element that will be pushed to the stack.
                    Element e;
                    e.req = (Request *)malloc(sizeof(Request *));//Memory allocation for the element.
                    e.req = request;
                    e.fd = new_fd;
                    e.start_time = tv.tv_usec;

                    pthread_mutex_lock(&stack_mutex);

                    request_flag = 1;
                    // Push request to stack
                    push(e);

                    // Singal to inform worker threads that there is a new request in the stack.
                    pthread_cond_signal(&new_request);
                    pthread_mutex_unlock(&stack_mutex);
                    //printf("after push values are socket_fd:%d start_time:%lo\n",request_stack.requests[request_stack.stack_pointer -1].fd , request_stack.requests[request_stack.stack_pointer - 1].start_time);
                }else if(request->operation == PUT){
                    gettimeofday(&tv , NULL);
                    long waiting_time = 0;
                    long service_start = tv.tv_usec;

                    // Write the given key/value pair to the database.
                    if (KISSDB_put(db, request->key, request->value))
                        sprintf(response_str, "PUT ERROR\n");
                    else
                        sprintf(response_str, "PUT OK\n");

                    write_str_to_socket(new_fd, response_str, strlen(response_str));
                    close(new_fd);

                    //calculate request service_time.
                    gettimeofday(&tv , NULL);
                    long service_time = tv.tv_usec - service_start;

                    pthread_mutex_lock(&completed_requests_mutex);
                    request_flag = 1;

                    completed_requests ++;
                    total_service_time += service_time;
                    total_waiting_time += waiting_time;
                    
                    pthread_mutex_unlock(&completed_requests_mutex);
                }else{
                    sprintf(response_str, "UNKOWN OPERATION\n");
                    write_str_to_socket(new_fd, response_str, strlen(response_str));
                    close(new_fd);
                }
                // Reply to client.
                if (request){
                    free(request);
                }
                request = NULL;
            }
        }else{
            // Send an Error reply to the client.
            write_str_to_socket(new_fd, "FORMAT ERROR\n", strlen("FORMAT ERROR\n"));
            close(new_fd);
        }
    }
    pthread_exit((void *) 0);//Exit the thread.
}

/*
 * @name main - The main routine.
 *
 * @return 0 on success, 1 on error.
 */
int main() {
    gettimeofday(&tv , NULL);

    signal(SIGPIPE, SIG_IGN); //Set signal ctrl+c handler.
    signal(SIGTSTP , signal_handler); //Set signal ctrl+z handler.
    
    // Definition of worker thread data array.
    Thread_info worker_threads[MAX_THREAD_NUMBER];
    
    // Definition of producer thread data array.
    Thread_info producer_thread;
    
    // Initialize mutex variables and condition variables.
    pthread_mutex_init(&stack_mutex , NULL);
    pthread_mutex_init(&completed_requests_mutex , NULL);
    pthread_cond_init(&new_request , NULL);

    void *status;

    request_stack.stack_pointer = 0;
   
    // Set attributes for threads
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr , PTHREAD_CREATE_JOINABLE);

    int wt;
    int pt;
    
    // Initialize producer thred.
    pt = pthread_create(&producer_thread.thread_id , &attr , producer , NULL);
    if (pt){
      printf("ERROR; return code from pthread_create() is %d\n" , pt);
      exit(-1);
    }
    
    // Initialize worker threads.
    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
      worker_threads[i].thread_num = i;
      worker_threads[i].working = 0;
      wt = pthread_create(&worker_threads[i].thread_id , &attr , worker , NULL);
      if (wt){
        printf("ERROR; return code from pthread_create() is %d\n" , wt);
        exit(-1);
      }
    }

    // Set joinable attribute for producer thread.
    pt = pthread_join(producer_thread.thread_id , &status);
    if (pt){
      printf("ERROR; return code from pthread_join() is %d\n", pt);
      exit(-1);
    }

    // Set joinable attribute for worker threads.
    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
      wt = pthread_join(worker_threads[i].thread_id , &status);
      if (wt){
        printf("ERROR; return code from pthread_join() is %d\n", wt);
        exit(-1);
      }
    }

    // Destroy attributes , mutex and condition variables.
    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&stack_mutex);
    pthread_mutex_destroy(&completed_requests_mutex);
    pthread_cond_destroy(&new_request);
    pthread_exit(NULL);

    return 0;
}
