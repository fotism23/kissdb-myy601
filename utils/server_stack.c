#include "server_stack.h"

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

/* @name size - Returns the number of items stack contains.
 * 
 * @return Number of items in stack.
 */
int stack_size() {
    return request_stack.stack_pointer;
}