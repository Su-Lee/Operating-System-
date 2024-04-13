#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "fifo.h"
#include "lru.h"

// NTU OS 2024
// you may want to declare page replacement buffer here
// or other files
#ifdef PG_REPLACEMENT_USE_LRU
// TODO
lru_t pr_buffer; // dclare global var, hope data allocated by the system

#elif defined(PG_REPLACEMENT_USE_FIFO)
// TODO
queue_t pr_buffer;

pte_t array[PG_BUF_SIZE];
int array_index;
#endif

pte_t first_three[3];
pte_t pinned;
int fifo_flag;
/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // map kernel stacks
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// Initialize the one kernel_pagetable
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }

  pte_t *pte = &pagetable[PX(0, va)];
  pte_t t = pagetable[PX(0, va)];

// NTU OS 2024
// pte is accessed, so determine how
// it affects the page replacement buffer here
#ifdef PG_REPLACEMENT_USE_LRU
// TODO
  for(int i = 0; i<3; i++){
    if(first_three[i] == t){
      return pte;
    }
  }
  if (lru_find(&pr_buffer, pte) == -1) {
    if (lru_full(&pr_buffer)) {
      uint64 evicted_page = lru_pop(&pr_buffer, 0); // Pop the oldest page from LRU
    }
    // Push the new page into LRU and mark it as valid in the buffer
    for(int i = 0; i<3; i++){
      if(first_three[i] == t){
        //printf("no %p\n", t);
        return pte;
      }
    }
    lru_push(&pr_buffer, pte);
  }else if(lru_find(&pr_buffer, pte) >= 0){
    uint64 idx = lru_find(&pr_buffer, pte);
    lru_pop(&pr_buffer, idx);
    for(int i = 0; i<3; i++){
      if(first_three[i] == t){
        //printf("no %p\n", t);
        return pte;
      }
    }
    lru_push(&pr_buffer, pte);
    // printf("the element already exists in the buffer\n");
    // pop from the original place and add it to the top
  }
  return pte;
#elif defined(PG_REPLACEMENT_USE_FIFO)
// TODO
// Check if the page table entry is already in the FIFO queue
  for(int i = 0; i<3; i++){
    if(first_three[i] == t){
      return pte;
    }
  }
  if (q_find(&pr_buffer, pte) == -1) {
    if (q_full(&pr_buffer)) {
      if(pinned == t){
        // printf("pinned found\n");
        q_pop_idx(&pr_buffer, 1);
        for (int i = 1; i < array_index - 1; i++) {
          array[i] = array[i + 1];
        }
        array_index--;
      }else if(pr_buffer.bucket[0] == t){
        //printf("pinned found\n");
        q_pop_idx(&pr_buffer, 1);
        for (int i = 1; i < array_index - 1; i++) {
          array[i] = array[i + 1];
        }
        array_index--;
      }else if(fifo_flag == 1){
        //printf("fifo flag\n");
        q_pop_idx(&pr_buffer, 1);
        for (int i = 1; i < array_index - 1; i++) {
          array[i] = array[i + 1];
        }
        array_index--;
        return pte;
      }else {
        uint64 evicted_page = q_pop_idx(&pr_buffer, 0); // Pop the oldest page from FIFO
        for (int i = 0; i < array_index - 1; i++) {
          array[i] = array[i + 1];
        }
        array_index--;
      }
    }
    // Push the new page into FIFO and mark it as valid in the buffer
  
    for(int i = 0; i<3; i++){
      if(first_three[i] == t){
        //printf("no %p\n", t);
        return pte;
      }
    }
    //if(t&PTE_V){
      q_push(&pr_buffer, pte, 0);
      array[array_index++] = pte;
    //}
    
    
  }else if(q_find(&pr_buffer, pte) >= 0){
    // if pinned object is being referred again, it should stay in the original place
  }
  return pte;
#endif
  return pte;
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");
  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");

    if(*pte & PTE_S) {
      /* NTU OS 2024 */
      /* int blockno = PTE2BLOCKNO(*pte); */
      /* bfree_page(ROOTDEV, blockno); */
      /* HACK: Do nothing here.
         The wait() syscall holds holds the proc lock and calls into this method.
         It causes bfree_page() to panic. Here we leak the swapped pages anyway.
      */
      continue;
    }

    if((*pte & PTE_V) == 0)
      continue;

    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

/* NTU OS 2024 */
/* Map pages to physical memory or swap space. */
int madvise(uint64 base, uint64 len, int advice) {
  // printf("starting madivse base %d len %d advice %d\n", base, len, advice); //2==MADV_DONTNEED
  //pgprint();
  struct proc *p = myproc();
  pagetable_t pgtbl = p->pagetable;

  if (base > p->sz || (base + len) > p->sz) {
    return -1;
  }

  if (len == 0) {
    return 0;
  }

  uint64 begin = PGROUNDDOWN(base);
  uint64 last = PGROUNDDOWN(base + len - 1);

  if (advice == MADV_NORMAL) {
    // TODO
    return 0; // No special treatment
    
  } else if (advice == MADV_WILLNEED) { // Expect an access in the near future. It swaps in the pages on the disk to the memory, and allocates new physical memory pages for unallocated pages.
    // TODO
    // Ensure the pages within the memorfy region are physically allocated
    begin_op();

    for (uint64 va = begin; va <= last; va += PGSIZE) {
      pte_t *pte = walk(pgtbl, va, 0);
      if (pte == 0) {
        end_op();
        return -1; // Failed to find page table entry
      }
      if (!(*pte & PTE_V)) {
        char *pa = kalloc();
        if (pa == 0) {
          end_op();
          return -1; // Failed to allocate physical memory
        }
        memset(pa, 0, PGSIZE);
        *pte = PA2PTE(pa) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_U; // Mark page as valid and readable/writable/executable by user
      }
    }

    end_op();
    return 0;
    
  } else if (advice == MADV_DONTNEED) {
    begin_op();

    pte_t *pte;
    for (uint64 va = begin; va <= last; va += PGSIZE) {
      //fifo_flag = 1;
      pte = walk(pgtbl, va, 0);
      //printf("dontneed: %p\n",pte);
      if (pte != 0 && (*pte & PTE_V)) {
        char *pa = (char*) swap_page_from_pte(pte);
        if (pa == 0) {
          end_op();
          return -1;
        }
        kfree(pa);
      }
    }

    // NTU OS 2024
    // Swapped out page should not appear in
    // page replacement buffer
    #ifdef PG_REPLACEMENT_USE_LRU
    // TODO
    for (uint64 va = begin; va <= last; va += PGSIZE) {
      //printf("find swapped out and pop\n");
      // pte_t *pte = walk(pgtbl, va, 0);
      uint64 index = lru_find(&pr_buffer, pte);
      //printf("lru pop index is %p idx %d\n", pte, index);
      //pgprint();
      if(lru_find(&pr_buffer, pte) != -1){
        lru_pop(&pr_buffer, index);
        //printf("q_pop_idx in vm.c %p %d\n", pte, index);
      }else {
        //printf("not popoing\n");
      }
      //if (pte != 0 && (*pte & PTE_V)) {
      //  uint64 pte_pa = PTE2PA(*pte);
      //}
    }
    #elif defined(PG_REPLACEMENT_USE_FIFO)
    // TODO
    //printf("begin %d to last %d\n", begin, last);
    for (uint64 va = begin; va <= last; va += PGSIZE) {
      //printf("find swapped out and pop\n");
      // pte_t *pte = walk(pgtbl, va, 0);
      uint64 index = q_find(&pr_buffer, pte);
      //printf("fifo pop index is %d\n", index);
      //pgprint();
      if(q_find(&pr_buffer, pte) != -1){
        q_pop_idx(&pr_buffer, index);
        for (int i = index; i < array_index - 1; i++) {
          array[i] = array[i + 1];
        }
        array_index--;
        //printf("q_pop_idx in vm.c %p %d\n", pte, index);
      }else {
        //printf("not poping\n");
      }
      //if (pte != 0 && (*pte & PTE_V)) {
      //  uint64 pte_pa = PTE2PA(*pte);
      //}
    }
    #endif

    end_op();
    return 0;

  } else if(advice == MADV_PIN) { // cannot be pageout
    // TODO
    // Ensure the pages within the memory region are pinnedd
    //pgprint();
    for (uint64 va = begin; va <= last; va += PGSIZE) {
      fifo_flag = 1;
      //printf("flag\n");
      
      pte_t *pte = walk(pgtbl, va, 0); //when pin it is called and allocating random var to buffer! assumebly
      //pgprint();
      if (pte == 0) {
        return -1; // Failed to find page table entry
      }
      *pte |= PTE_P; // Set PTE_P bit to pin page
      // fifo_flag = 1;
      // pinned = pte;
    }
    return 0;
    
  } else if(advice == MADV_UNPIN) { // can be pageout
    // TODO
    // Unpin the pages within the memory region
    for (uint64 va = begin; va <= last; va += PGSIZE) {
      pte_t *pte = walk(pgtbl, va, 0);
      if (pte == 0) {
        return -1; // Failed to find page table entry
      }
      *pte &= ~PTE_P; // Clear PTE_P bit to unpin page
    }
    return 0;

  }
  else {
    return -1;
  }
}

/* NTU OS 2024 */
/* print pages from page replacement buffers */
#if defined(PG_REPLACEMENT_USE_LRU) || defined(PG_REPLACEMENT_USE_FIFO)
void pgprint() {
  // printf("start pgprint\n");
  #ifdef PG_REPLACEMENT_USE_LRU
  // TODO
  printf("Page replacement buffers\n");
  printf("------Start------------\n");
  //printf("size: %d\n",pr_buffer.size);
  for (int i = 0; i < pr_buffer.size; i++) {
    printf("pte: %p\n", pr_buffer.bucket[i]);
  }
  //printf("\n");
  printf("------End--------------\n");
  
  #elif defined(PG_REPLACEMENT_USE_FIFO)
  // TODO
  printf("Page replacement buffers\n");
  printf("------Start------------\n");
  //printf("size: %d\n",pr_buffer.size);
  for (int i = 0; i < pr_buffer.size; i++) {
    printf("pte: %p\n", pr_buffer.bucket[i]);
  }
  //printf("\n");
  printf("------End--------------\n");
  
  #endif
  // panic("not implemented yet\n");
}
#endif


/* NTU OS 2024 */
/* Print multi layer page table. */

//void vmprint(pagetable_t pagetable) {
  /* TODO */
 // printf("starting vmprint %p\n", pagetable);
  //  vmprint_recursive(pagetable, 2, 0, MAXVA);
//}

/* Helper function for recursive traversal of page table */
/* Helper function for recursive traversal of page table */
/*
void vmprint_recursive(pagetable_t pagetable, int level, uint64 start_va, uint64 end_va) {
    // Base case: If level is less than 0, return
    if (level < 0)
        return;
    
    // Iterate over all possible entries at this level
    for (uint64 i = 0; i < PGSIZE/sizeof(pte_t); i++) {
        pte_t pte = pagetable[i];

        // Calculate the virtual address corresponding to this entry
        uint64 va = start_va + (i << (PGSHIFT + level * 9));
        //uint64 va = (i << (PGSHIFT + level * 9));
        
        // Check if this entry is valid
        if (pte & PTE_V) {
            // If it's a leaf entry, print the mapping
            uint64 pa = PTE2PA(pte);
            printf("first pte=%p va=%p pa=%p\n", &pte, va, pa);
            // printf("new pte %p, and %p\n", &pte, (void *)PTE2PA(&pte));
            // walk(pagetable_t pagetable, uint64 va, int alloc)
            if (level == 0) {
                // uint64 pa = PTE2PA(pte);
                printf("|   +-- %lu: pte=%p va=%p pa=%p", i, &pte, va, pa);
                if (pte & PTE_V)
                    printf(" V");
                if (pte & PTE_R)
                    printf(" R");
                if (pte & PTE_W)
                    printf(" W");
                if (pte & PTE_X)
                    printf(" X");
                if (pte & PTE_U)
                    printf(" U");
                if (pte & PTE_D)
                    printf(" D");
                printf("\n");
            } else {
                // If it's an intermediate entry, recursively print its children
                printf("+-- %lu: pte=%p va=%p pa=%p V\n", i, &pte, va, pa);
                pagetable_t next_pagetable = (pagetable_t)PTE2PA(pte);
                vmprint_recursive(next_pagetable, level - 1, va, va + (i << (PGSHIFT + level * 9)));
            }
        }
    }
}
*/

//https://stackoverflow.com/questions/78211340/is-it-possible-to-manually-change-page-tables-pte-value-xv6-risc-v-c
static int level = 0;
static int print_end = 0;
static uint64 vadd_lv0 = 0;
static uint64 vadd_lv1 = 0;
static uint64 vadd_lv2 = 0;
static pte_t pte = 0;

void vmprint(pagetable_t pagetable) {
  if (level == 0){
    printf("page table %p\n", pagetable);
    print_end = 0;
    pte = pagetable;
  }
  // iterate 512 PTEs
  for (uint64 i = 0; i < 512; i++) {
    uint64 va = 0;
    switch(level){
      case 0:
        vadd_lv0 = i<<30;
        va = vadd_lv0;
        break;
      case 1:
        vadd_lv1 = i<<21;
        va = vadd_lv0+vadd_lv1;
        break;
      case 2:
        vadd_lv2 = i<<12;
        va = vadd_lv0+vadd_lv1+vadd_lv2;
        break;
    }
    pte_t t = pagetable[i];
    if (t & PTE_V) {
      uint64 pa = PTE2PA(t);
      if (level==0 && i==255)
        print_end = 1;
      if (level != 0 && !print_end){
        printf("|");
      }else if (level!=0){
        printf(" ");
      }
      for (int j = 0; j < (level-1)*4+3; j++) printf(" ");

      pte_t* temp = &pagetable[i];
      
      if(i <= 2){
        //printf("first three t %p\n", temp);
        first_three[i] = t;
      }
      if(t&PTE_P){
        pinned = temp;
      }
      printf("+-- %d: pte=%p va=%p pa=%p",i, temp, va, pa);
      printf((t&PTE_V)?" V":"");
      printf((t&PTE_R)?" R":"");
      printf((t&PTE_W)?" W":"");
      printf((t&PTE_X)?" X":"");
      printf((t&PTE_U)?" U":"");
      printf((t&PTE_D)?" D":"");
      printf((t&PTE_P)?" P":"");
      //printf((t&PTE_S)?" S":"");
      printf("\n");
      // PTE without any WRX bit set points to low-level page table
      if ((t & (PTE_W|PTE_R|PTE_X)) == 0){
        level++;
        vmprint((pagetable_t)pa);
        level--;
      }
    }else if(t & PTE_S){
      // printf("hello im blockno\n");
      // pte_t *pte = walk(pagetable, va, 0);
      uint64 pa = PTE2PA(t);
      if (level==0 && i==255)
        print_end = 1;
      if (level != 0 && !print_end){
        printf("|");
      }else if (level!=0){
        printf(" ");
      }
      for (int j = 0; j < (level-1)*4+3; j++) printf(" ");

      pte_t* temp = &pagetable[i];
      uint64 blockno = PTE2BLOCKNO(t);
      printf("+-- %d: pte=%p va=%p blockno=%p",i, temp, va, blockno);
      printf((t&PTE_V)?" V":"");
      printf((t&PTE_R)?" R":"");
      printf((t&PTE_W)?" W":"");
      printf((t&PTE_X)?" X":"");
      printf((t&PTE_U)?" U":"");
      printf((t&PTE_D)?" D":"");
      printf((t&PTE_S)?" S":"");
      printf((t&PTE_P)?" P":"");
      printf("\n");

      /*
      if (*pte & PTE_S) {
        
        uint64 blockno = PTE2BLOCKNO(*pte);
  
        read_page_from_disk(ROOTDEV, pa, blockno);

        *pte = BLOCKNO2PTE(pa) | PTE_FLAGS(*pte);
        printf("+-- %d: pte=%p va=%p blockno=%p",i, pte, va, blockno);
      }*/
      /*
      pte_t* temp = &pagetable[i];
      //printf("hello im blockno\n");
      pte_t *block_pte = walk(pagetable, va, 0);
      //printf("hello im blockno\n");
      uint64 blockno = PTE2BLOCKNO(*block_pte);
      printf("%d\n", blockno);*/
      
    }
  }
}

