#include "fifo.h"

#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

/*
 typedef struct queue {
   uint32 size;
   uint64 bucket[PG_BUF_SIZE];
   uint32 pin; // 1 indicate pinned, 0 indicate not pinned
 } queue_t;
 */
void q_init(queue_t *q){
	//panic("Not implemented yet\n");
  q->size = 0;
}

int q_push(queue_t *q, uint64 e, uint32 flag){
	//panic("Not implemented yet\n");
  //printf("q_push %d\n", e);
  
  if (q->size > PG_BUF_SIZE) {
    // Queue is full
    printf("queue is full\n");
    return -1;
  }
  q->bucket[q->size++] = e;
  q->pin = flag;
  return 0;
}

uint64 q_pop_idx(queue_t *q, int idx){
	//panic("Not implemented yet\n");
  //printf("q_pop %d\n", idx);
  //while(q->pin == 1){
  //  idx++;
  //}
  if (idx < 0 || idx >= q->size) {
    // Invalid index
    panic("Invalid index in fifo_pop_idx");
  }
  // it only walks for the pinned object being at the most front
  uint64 page = q->bucket[idx];
  for (int i = idx; i < q->size - 1; i++) {
    q->bucket[i] = q->bucket[i + 1];
  }
  q->size--;
  return page;
}

int q_empty(queue_t *q){
	//panic("Not implemented yet\n");
  return q->size == 0;
}

int q_full(queue_t *q){
	//panic("Not implemented yet\n");
  return q->size >= PG_BUF_SIZE;
}

int q_clear(queue_t *q){
	//panic("Not implemented yet\n");
  q->size = 0;
  return 0;
}

int q_find(queue_t *q, uint64 e){
	//panic("Not implemented yet\n");
  if(q->size==0){
    //printf("first q_find %d\n", e);
    return -1;
  }
  for (int i = 0; i < q->size; i++) {
    if (q->bucket[i] == e) {
      return i;
    }
  }
  return -1; // Not found
}
