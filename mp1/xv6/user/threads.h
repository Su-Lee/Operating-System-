#ifndef THREADS_H_
#define THREADS_H_
#define NULL_FUNC ((void (*)(int))-1)
// TODO: necessary includes, if any
#include "user/setjmp.h"
// TODO: necessary defines, if any

struct thread {
    void (*fp)(void *arg);
    void *arg;
    void *stack;
    void *stack_p;
    jmp_buf env; // for thread function
    int buf_set; // 1: indicate jmp_buf (env) has been set, 0: indicate jmp_buf (env) not set
    // TASK variables
    void (*task_fp[20])(void *task_arg);
    void *task_arg[20];
    void *task_stack[20];
    void *task_stack_p[20];
    jmp_buf task_env[20];
    int task_buf_set[20];
    int num_tasks; // Number of tasks associated with the thread
    int current_task;
    int task_yield; // flag to indicate task_yield
    int thread_yield; // flag to indicate thread_yield
    int ID;
    struct thread *previous;
    struct thread *next;
};

struct thread *thread_create(void (*f)(void *), void *arg);
void thread_add_runqueue(struct thread *t);
void thread_yield(void);
void dispatch(void);
void schedule(void);
void thread_exit(void);
void thread_start_threading(void);

// part 2
void thread_assign_task(struct thread *t, void (*f)(void *), void *arg);
#endif // THREADS_H_
