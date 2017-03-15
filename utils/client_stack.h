#include "utils.h"
#include <stdio.h>
#define STACK_SIZE       100

//Definition of the request element type which the stack holds.
typedef struct stack_element{
    char *buffer;
    struct sockaddr_in server_addr;
} Element;

//Definition of the stack type.
typedef struct stack_s{
    int stack_pointer;
    Element *elements;
} Stack;

int is_empty();
int is_full();
int push(Element e);
Element *pop();
int stack_size();
void to_string();
void init_stack();

Stack request_stack;



