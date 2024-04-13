typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

#define PG_BUF_SIZE 8

typedef struct queue {
	uint32 size;
	uint64 bucket[PG_BUF_SIZE];
  uint32 pin; // 1 indicate pinned, 0 indicate not pinned
} queue_t;

void q_init(queue_t *q);
int q_push(queue_t *q, uint64 e, uint32 flag);
uint64 q_pop_idx(queue_t *q, int idx);
int q_find(queue_t *q, uint64 e);
int q_empty(queue_t *q);
int q_full(queue_t *q);
int q_clear(queue_t *q);
