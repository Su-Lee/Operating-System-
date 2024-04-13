#include "kernel/types.h"
#include "user/setjmp.h"
#include "user/threads.h"
#include "user/user.h"
#define NULL 0
#define MAX_TASK 20

static struct thread* current_thread = NULL;
static int id = 1;
// static jmp_buf env_st;
// static jmp_buf env_tmp;

struct thread *thread_create(void (*f)(void *), void *arg){
    struct thread *t = (struct thread*) malloc(sizeof(struct thread));
    unsigned long new_stack_p;
    unsigned long new_stack;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    new_stack_p = new_stack +0x100*8-0x2*8;
    t->fp = f;
    t->arg = arg;
    t->ID  = id; //id starts from 1 and increment
    t->buf_set = -1; //indicate jmp_buf (env) not set
    t->num_tasks = -1;
    t->current_task = -1;
    t->thread_yield = 0;
    t->task_yield =0;
    t->stack = (void*) new_stack;
    t->stack_p = (void*) new_stack_p;
    t->next = NULL;
    t->previous = NULL;
    id++;
    //printf("Thread created.\n");
    return t;
}
void thread_add_runqueue(struct thread *t){
    if(current_thread == NULL){
        // TODO add the element as the first
        current_thread = t;
        current_thread->previous = current_thread;
        current_thread->next = current_thread;
    }
    else{
        // TODO add the element as and remember to link prev and next
        t->previous = current_thread->previous;
        t->next = current_thread;
        current_thread->previous->next = t;
        current_thread->previous = t;
    }
    //printf("done thread_add_runqueue\n");
}
void thread_yield(void){
    // TODO The function suspends the current thread by saving its context.
    if(current_thread->num_tasks >= 0 && current_thread->thread_yield == 0){
        int index = current_thread->current_task;
        //printf("task yield %d\n", index);
        current_thread->task_yield = 1;
        if(setjmp(current_thread->task_env[index]) == 0){
            current_thread->task_buf_set[index] = 1;
            schedule();
            dispatch();
        }else{
            current_thread->task_buf_set[index] = 0;
            if(setjmp(current_thread->task_env[index]) == 0){
                current_thread->task_buf_set[index] = 1;
            }else{
                //printf("setjmp setjmp setjmp\n");
            }
        }
    }else{
        //printf("thread yield\n");
        current_thread->thread_yield = 0;
        if (setjmp(current_thread->env) == 0){
            current_thread->buf_set = 1;
            schedule();
            dispatch();
        }else{
            current_thread->buf_set = 0;
        }
    }
}
void dispatch(void){
    // TODO The function executes a thread
    
    // run task
    while(current_thread->num_tasks >= 0){
        int i = current_thread->num_tasks;
        //printf("run task %d\n", i);
        if (i >= 0) { //double check
            current_thread->current_task = i;
            if (current_thread->task_buf_set[i] == 1) { // from round 2
                longjmp(current_thread->task_env[i], 1);
            } else if (current_thread->task_buf_set[i] == -1) { // initialize
                if (setjmp(current_thread->task_env[i]) == 0) {
                    current_thread->task_env[i]->sp = (unsigned long) current_thread->task_stack_p;
                    longjmp(current_thread->task_env[i], 1);
                }
                current_thread->task_buf_set[i] = 0;
                current_thread->task_fp[i](current_thread->task_arg[i]); // run task function
            }
        }
        //free(current_thread->task_stack[current_thread->num_tasks]);
        current_thread->num_tasks--;
    }
    current_thread->current_task = -1;
    
    // run thread
    if(current_thread->task_yield == 1){ // thread has been yielded from the task
        //printf("task yield resume\n");
        current_thread->task_yield = 0;
        if (current_thread->buf_set == 1) { // from round 2
            longjmp(current_thread->env, 1);
        } else {
            if(current_thread->buf_set == -1){ // initialize
                if(setjmp(current_thread->env)==0){
                    current_thread->env->sp = (unsigned long)current_thread->stack_p;
                    longjmp(current_thread->env, 1);
                }
                current_thread->buf_set = 0;
                current_thread->fp(current_thread->arg);
            }
            thread_exit();
        }
    }else{
        if (current_thread->buf_set == 1) { // from round 2
            current_thread->thread_yield = 1;
            longjmp(current_thread->env, 1);
        } else {
            if(current_thread->buf_set == -1){ // initialize
                if(setjmp(current_thread->env)==0){
                    current_thread->env->sp = (unsigned long)current_thread->stack_p;
                    longjmp(current_thread->env, 1);
                }
                current_thread->buf_set = 0;
                current_thread->thread_yield = 1;
                current_thread->fp(current_thread->arg);
            }
            thread_exit();
        }
    }
}
void schedule(void){
    // TODO
    current_thread = current_thread->next;
}
void thread_exit(void){
    if (current_thread == NULL) {
        // printf("Error: No current thread to exit.\n");
        return;
    }
    if(current_thread->next != current_thread){
        // TODO
        // printf("Exiting thread...%d\n",current_thread->ID);
        struct thread *temp = current_thread;
        current_thread->previous->next = current_thread->next;
        current_thread->next->previous = current_thread->previous;
        current_thread = current_thread->next;
        
        // Free the task stacks
        for (int i = 0; i <= temp->num_tasks; i++) {
            free(temp->task_stack[i]);
        }
        free(temp->stack);
        free(temp);
        dispatch();
    }else{
        // TODO
        // printf("Exiting last thread...%d\n",current_thread->ID);
        struct thread *temp = current_thread;
        
        // Free the task stacks
        for (int i = 0; i <= temp->num_tasks; i++) {
            free(temp->task_stack[i]);
        }
        free(temp->stack);
        free(temp);
        current_thread = NULL;
        // the illegal way !!!FIX to exit at main!!!
        printf("\nexited\n");
        exit(0);
        // thread_start_threading(); // Return control to the main function
    }
}
void thread_start_threading(void){
    // TODO
    while(current_thread != NULL){
        dispatch();
    }
    // printf("All threads exited.\n");
    return;
}

// part 2
void thread_assign_task(struct thread *t, void (*f)(void *), void *arg){
    // TODO
    unsigned long new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);
    unsigned long new_stack_p = new_stack +0x100*8-0x2*8;
    t->num_tasks++;
    // printf("assigning task %d\n",t->num_tasks);
    t->task_fp[t->num_tasks] = f;
    t->task_arg[t->num_tasks] = arg;
    t->task_stack[t->num_tasks] = (void*) new_stack;
    t->task_stack_p[t->num_tasks] = (void*) new_stack_p;
    t->task_buf_set[t->num_tasks] = -1;
}

/*
asm volatile (
    "mv %0, %1" // Move the value of the stack pointer register into stack_p
    : "=r" (current_thread->env->sp) // Output operand: current_thread->env[0].sp
    : "r" (current_thread->stack_p) // Input operand: stack_p
);
*/
