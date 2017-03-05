/* client.c

   Sample code of
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina

   (c) S. Anastasiadis, G. Kappes 2016

*/
#include "client.h"

/**
 * @name print_usage - Prints usage information.
 * @return
 */
void print_usage() {
  fprintf(stderr, "Usage: client [OPTION]...\n\n");
  fprintf(stderr, "Available Options:\n");
  fprintf(stderr, "-h:             Print this help message.\n");
  fprintf(stderr, "-a <address>:   Specify the server address or hostname.\n");
  fprintf(stderr, "-o <operation>: Send a single operation to the server.\n");
  fprintf(stderr, "                <operation>:\n");
  fprintf(stderr, "                PUT:key:value\n");
  fprintf(stderr, "                GET:key\n");
  fprintf(stderr, "-i <count>:     Specify the number of iterations.\n");
  fprintf(stderr, "-g:             Repeatedly send GET operations.\n");
  fprintf(stderr, "-p:             Repeatedly send PUT operations.\n");
}

/**
 * @name talk - Sends a message to the server and prints the response.
 * @server_addr: The server address.
 * @buffer: A buffer that contains a message for the server.
 *
 * @return
 */
void * talk() {
    while (1) {
        puts("edw");
        pthread_mutex_lock(&stack_mutex);
        while (!request_flag){
            pthread_cond_wait(&new_request , &stack_mutex);
            
        }
        Element *e = pop();
        request_flag = 0;
        pthread_mutex_unlock(&stack_mutex);
        //printf("element poped %s\n", e->buffer);
        
        if (e){
            char rcv_buffer[BUF_SIZE];
            int socket_fd, numbytes;

            //create socket.
            if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                ERROR("socket()");
            }

            //connect to server.
            if (connect(socket_fd , (struct sockaddr*) &e->server_addr , sizeof(e->server_addr)) == -1){
                ERROR("connect()");
            }

            // send message.
            write_str_to_socket(socket_fd, e->buffer, strlen(e->buffer));

            char * output_str = (char *)malloc(100);
            sprintf(output_str , "Operation: %s\n", e->buffer);
            strcat(output_str , "Result: ");
            do{
                memset(rcv_buffer , 0 , BUF_SIZE);
                numbytes = read_str_from_socket(socket_fd , rcv_buffer , BUF_SIZE);
                if(numbytes != 0){
                    strcat(output_str , rcv_buffer);
                }
            }while(numbytes > 0);
            strcat(output_str , "\n");
            printf("%s\n", output_str);
            close(socket_fd);
        }
    }
    pthread_exit(NULL);
}

/**
 * @name main - The main routine.
 */
int main(int argc, char **argv) {
    Thread_info worker_thread[MAX_THREAD_NUMBER];
    pthread_mutex_init(&stack_mutex , NULL);
    pthread_cond_init(&new_request , NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr , PTHREAD_CREATE_JOINABLE);

    request_stack.stack_pointer = 0;

    int wt,i;

    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
        worker_thread[i].thread_num = i;
        wt = pthread_create(&worker_thread[i].thread_id , &attr , talk , NULL);
        if (wt){
            printf("ERROR; return code from pthread_create() is %d\n" , wt);
            exit(-1);
        }
    }
    
    for (i = 0 ; i < MAX_THREAD_NUMBER ; i++){
        
        wt = pthread_join(worker_thread[i].thread_id , NULL);

        if (wt){
            printf("ERROR; return code from pthread_join() is %d\n", wt);
            exit(-1);
        }
    }
    
    char *host = NULL;
    char *request = NULL;
    int mode = 0;
    int option = 0;
    int count = ITER_COUNT;

    char snd_buffer[BUF_SIZE]; //TO STRUCT
    int station, value;

    struct sockaddr_in server_addr; //TO STRUCT
    struct hostent *host_info;

    // Parse user parameters.
    while ((option = getopt(argc, argv,"i:hgpo:a:")) != -1) {

        switch (option) {
            case 'h':
                print_usage();
                exit(0);
            case 'a':
                host = optarg;
                break;
            case 'i':

                count = atoi(optarg);
	            break;
            case 'g':
                if (mode) {
                    fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
                    exit(EXIT_FAILURE);
                }
                mode = GET_MODE;
                break;
            case 'p':
                if (mode) {
                    fprintf(stderr, "You can only specify one of the following: -g, -p, -o\n");
                    exit(EXIT_FAILURE);
                }
                mode = PUT_MODE;
                break;
            case 'o':
                if (mode) {
                    fprintf(stderr, "You can only specify one of the following: -r, -w, -o\n");
                    exit(EXIT_FAILURE);
                }
                mode = USER_MODE;
                request = optarg;
                break;
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }

    // Check parameters.
    if (!mode) {
        fprintf(stderr, "Error: One of -g, -p, -o is required.\n\n");
        print_usage();
        exit(0);
    }
    if (!host) {
        fprintf(stderr, "Error: -a <address> is required.\n\n");
        print_usage();
        exit(0);
    }

    // get the host (server) info
    if ((host_info = gethostbyname(host)) == NULL) {
        ERROR("gethostbyname()");
    }

    // create socket adress of server (type, IP-adress and port number)
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = *((struct in_addr*)host_info->h_addr);
    server_addr.sin_port = htons(SERVER_PORT);

    if (mode == USER_MODE) {
        memset(snd_buffer, 0, BUF_SIZE);
        strncpy(snd_buffer, request, strlen(request));

        Element e;
        e.server_addr = server_addr;
        e.buffer = snd_buffer;

        pthread_mutex_lock(&stack_mutex);
       
        push(e);
        request_flag = 1;

        pthread_mutex_unlock(&stack_mutex);
        pthread_cond_signal(&new_request);
    } else {
        while(--count>=0) {
            for (station = 0; station <= MAX_STATION_ID; station++) {
                memset(snd_buffer, 0, BUF_SIZE);

                if (mode == GET_MODE) {
                    // Repeatedly GET.
                    sprintf(snd_buffer, "GET:station.%d", station);
                } else if (mode == PUT_MODE) {
                    // Repeatedly PUT.
                    // create a random value.
                    value = rand() % 65 + (-20);
                    sprintf(snd_buffer, "PUT:station.%d:%d", station, value);
                }

                Element e;
                e.server_addr = server_addr;
                e.buffer = snd_buffer;

                fprintf(stderr, "pushing element %s\n", e.buffer);
                
                pthread_mutex_lock(&stack_mutex);

                push(e);
                request_flag = 1;

                pthread_mutex_unlock(&stack_mutex);
                pthread_cond_signal(&new_request);
            }
        }
    }   

    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&stack_mutex);
    pthread_cond_destroy(&new_request);
    pthread_exit(NULL);
    return 0;
}
