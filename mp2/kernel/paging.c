#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

/* NTU OS 2024 */
/* Allocate eight consecutive disk blocks. */
/* Save the content of the physical page in the pte */
/* to the disk blocks and save the block-id into the */
/* pte. */
char *swap_page_from_pte(pte_t *pte) {
  char *pa = (char*) PTE2PA(*pte);
  uint dp = balloc_page(ROOTDEV);

  write_page_to_disk(ROOTDEV, pa, dp); // write this page to disk
  *pte = (BLOCKNO2PTE(dp) | PTE_FLAGS(*pte) | PTE_S) & ~PTE_V;

  return pa;
}

/* NTU OS 2024 */
/* Page fault handler */
int handle_pgfault() {
  /* Find the address that caused the fault */
  /* uint64 va = r_stval(); */

  /* TODO */
  // panic("not implemented yet\n");
  //printf("handle pgfault\n");
  uint64 va = r_stval(); // Find the address that caused the fault
  struct proc *p = myproc();
  //printf("struct proc *p = myproc();\n");
  if (va >= MAXVA)
    panic("handle_page_fault: invalid virtual address");

  // Round down the virtual address to the page boundary
  va = PGROUNDDOWN(va);
  //printf("va = PGROUNDDOWN(va);\n");
  
  // Find the page table entry corresponding to the virtual address
  pte_t *pte = walk(p->pagetable, va, 1);
  //printf("error on *pte %p\n", pte);
  
  if (!pte)
    panic("handle_page_fault: walk failed");

  // Allocate a physical page for the offending virtual address
  char *pa = kalloc();
  if (!pa)
    panic("handle_page_fault: kalloc failed");

  // Map the physical page to the virtual address
  memset(pa, 0, PGSIZE); // Zero out the memory
  mappages(p->pagetable, va, PGSIZE, (uint64)pa, PTE_W | PTE_R | PTE_U | PTE_X);
  //printf("mappages();\n");
  
  return 0;
}
