#include <stdio.h>

#define STACK_SIZE                50

//Definition of the request element type which the stack holds.
typedef struct stack_element{
  int fd;
  long start_time;
  char * key;
} Element;

//Definition of the stack type.
typedef struct stack_s{
    int stack_pointer;
    Element requests[STACK_SIZE];
} Stack;

int is_empty();
int is_full();
int push(Element e);
Element *pop();
int stack_size();

Stack request_stack;