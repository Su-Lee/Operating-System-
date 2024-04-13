#include "lru.h"

#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

/*
 typedef struct lru {
   uint32 size;
   uint64 bucket[PG_BUF_SIZE];
 } lru_t;
 */
void lru_init(lru_t *lru){
  //panic("not implemented yet\n");
  lru->size = 0;
}

int lru_push(lru_t *lru, uint64 e){
  //panic("not implemented yet\n");
  if (lru->size == PG_BUF_SIZE) {
    return -1; // LRU is full
  }
  lru->bucket[lru->size++] = e;
  return 0; // Successfully pushed
}

uint64 lru_pop(lru_t *lru, int idx){
  //panic("not implemented yet\n");
  if (idx < 0 || idx >= lru->size) {
    panic("Invalid index for popping from LRU\n");
  }
  uint64 e = lru->bucket[idx];
  for (int i = idx; i < lru->size - 1; i++) {
    lru->bucket[i] = lru->bucket[i + 1];
  }
  lru->size--;
  return e;
}

int lru_empty(lru_t *lru){
  //panic("not implemented yet\n");
  return (lru->size == 0);
}

int lru_full(lru_t *lru){
  //panic("not implemented yet\n");
  return (lru->size == PG_BUF_SIZE);
}

int lru_clear(lru_t *lru){
  //panic("not implemented yet\n");
  lru->size = 0;
  return 0; // Successfully cleared
}

int lru_find(lru_t *lru, uint64 e){
  //panic("not implemented yet\n");
  if(lru->size==0){
    //printf("first q_find %d\n", e);
    return -1;
  }
  for (int i = 0; i < lru->size; i++) {
    if (lru->bucket[i] == e) {
      return i; // Element found at index i
    }
  }
  return -1; // Element not found
}
