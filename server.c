/* server.c

   Sample code of
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina

   (c) S. Anastasiadis, G. Kappes 2016

*/
#include "server.h"

/*
 * @name signal_handler - Overrides the default functionality of signals from the console.
 * @param signo: Signal Id.
 *   
 * @return Void
 */
void signal_handler(int signo){   
    if (signo == SIGTSTP){
        double avg_waiting_time = (double) total_waiting_time / (double) completed_requests;
        double avg_waiting_time_sec = (double) avg_waiting_time * 0.000001;
        double avg_service_time = (double) total_service_time / (double) completed_requests;
        double avg_service_time_sec = (double) avg_service_time * 0.000001;
        fprintf(stdout, "\n============================================\n");
        fprintf(stdout, "Total completed requests : %d\n", completed_requests);
        fprintf(stdout, "Average waiting time : %f usec (%f sec)\n", avg_waiting_time, avg_waiting_time_sec);
        fprintf(stdout, "Average service time : %f usec (%f sec)\n", avg_service_time, avg_service_time_sec);
        fprintf(stdout, "============================================\n");
        exit(0);
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
void * worker(){
    while(1){
        // Lock shared variables (request_stack).
        pthread_mutex_lock(&stack_mutex);
        
        while (is_empty()){
            pthread_cond_wait(&new_request, &stack_mutex);
        }
        
        // Pop element from stack.
        Element *element_poped = pop();
        
        // Unlock shared variables.
        pthread_mutex_unlock(&stack_mutex);

        if (element_poped){
            // Printouts if debug is enabled.
            if (DEBUG) {
                fprintf(stderr, "Consumer | Element poped from stack fd : %d start_time : %lo\n" ,element_poped->fd , element_poped->start_time  );
                fprintf(stderr, "Consumer | Stack size : %d\n", stack_size());
            }
            
            // Calculate request waiting_time in request_stack.
            gettimeofday(&tv , NULL);
            long waiting_time = tv.tv_usec - element_poped->start_time;
            long service_start = tv.tv_usec;

            char response_str[BUF_SIZE], request_str[BUF_SIZE];
            memset(response_str, 0, BUF_SIZE);
            memset(request_str, 0, BUF_SIZE);

            // Retrieve the file decriptor.
            int new_fd = element_poped->fd;
           
            // Declare and allocate space for the value.
            char * value = (char *)malloc(VALUE_SIZE);

			//fprintf(stderr, "key : %s\n", element_poped->key);
    
            // Retrieve info from the Database.
            if (KISSDB_get(db, element_poped->key, value))
                sprintf(response_str, "GET ERROR\n");
            else
                sprintf(response_str, "GET OK: %s\n", value);
                fprintf(stderr, "key : %s value : %s\n", element_poped->key, value);
            write_str_to_socket(new_fd , response_str , strlen(response_str));

            close(new_fd);

            // Calculate request service_time.
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


/**
 * @name worker - Responsible for setting up the server, wait and accecp/reject incoming connections,
 *                serving PUT request and fowrding GET request to the worker threads.
 *
 * @return Void pointer.
 */
void * producer(){
    int socket_fd;              // listen on this socket for new connections
    int new_fd;                 // use this socket to service a new connection
    socklen_t clen;
    struct sockaddr_in server_addr,  // my address information
                     client_addr;  // connector's address information

    // Create socket.
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
      ERROR("socket()");

    // Ignore the SIGPIPE signal in order to not crash when a
    // client closes the connection unexpectedly.
    signal(SIGPIPE, SIG_IGN);
    
    // Create socket adress of server (type, IP-adress and port number).
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // any local interface
    server_addr.sin_port = htons(MY_PORT);

    // Bind socket to address
    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
        ERROR("bind()");
    }
    
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

    // Main loop: wait for new connection/requests.
    while (1) {
        // Wait for incomming connection.
        if ((new_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &clen)) == -1) {
            ERROR("accept()");
        }

        // Got connection, serve request.
        fprintf(stderr, "(Info) main: Got connection from '%s'\n", inet_ntoa(client_addr.sin_addr));
        int numbytes = 0;
        Request *request = NULL;

        char response_str[BUF_SIZE], request_str[BUF_SIZE];
        memset(response_str , 0 , BUF_SIZE);
        memset(request_str , 0 , BUF_SIZE);

        numbytes = read_str_from_socket(new_fd , request_str , BUF_SIZE);
        if (numbytes){
            request = parse_request(request_str);
            if (request){
                if (request->operation == GET){
                    gettimeofday(&tv , NULL);
                    
                    // Definition for the element that will be pushed to the stack.
                    Element element_to_push;
                    
                    element_to_push.fd = new_fd;
                    element_to_push.start_time = tv.tv_usec;
                    element_to_push.key = (char *)malloc(KEY_SIZE);


                    strcpy(element_to_push.key, request->key);
                   
                    // Lock stack before pushing request to stack.
                    pthread_mutex_lock(&stack_mutex);

                    // Push request to stack
                    push(element_to_push);

                    // Unlock stack and signal worker thread.
                    pthread_cond_signal(&new_request);
                    pthread_mutex_unlock(&stack_mutex);
                    
                    // Printouts if debug is enabled.
                    if (DEBUG){
                        printf("Poducer | Element pushed to stack fd : %d start_time : %lo\n" ,element_to_push.fd , element_to_push.start_time );
                        printf("Poducer | Stack size : %d\n", stack_size());
                    }
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

                    // Calculate request service_time.
                    gettimeofday(&tv , NULL);
                    long service_time = tv.tv_usec - service_start;

                    pthread_mutex_lock(&completed_requests_mutex);
                    
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
 * @return 0 on success, -1 on error.
 */
int main() {
    gettimeofday(&tv , NULL);

    signal(SIGTSTP, signal_handler); //Set signal ctrl+c handler.
    
    // Definition of worker thread data array.
    Thread_info worker_threads[MAX_THREAD_NUMBER];
    
    // Definition of producer thread data array.
    Thread_info producer_thread;
    
    // Initialize mutex variables and condition variables.
    pthread_mutex_init(&stack_mutex , NULL);
    pthread_mutex_init(&completed_requests_mutex , NULL);
    pthread_cond_init(&new_request , NULL);

    // Initialize stack attributes.
    request_stack.stack_pointer = 0;
   
    // Set attributes for threads.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr , PTHREAD_CREATE_JOINABLE);

    int wt, pt;
    
    // Initialize producer thread.
    pt = pthread_create(&producer_thread.thread_id , &attr , producer , NULL);
    if (pt){
      printf("ERROR; return code from pthread_create() is %d\n" , pt);
      exit(-1);
    }
    
    // Initialize worker threads.
    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
      worker_threads[i].thread_num = i;
      wt = pthread_create(&worker_threads[i].thread_id , &attr , worker , NULL);
      if (wt){
        printf("ERROR; return code from pthread_create() is %d\n" , wt);
        exit(-1);
      }
    }
    
    // Set joinable attribute for producer thread.
    pt = pthread_join(producer_thread.thread_id , NULL);
    if (pt){
      printf("ERROR; return code from pthread_join() is %d\n", pt);
      exit(-1);
    }
    
    // Set joinable attribute for worker threads.
    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
      wt = pthread_join(worker_threads[i].thread_id , NULL);
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
