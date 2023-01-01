// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

unsigned char rc[(PHYSTOP >> 12) + 1];
struct spinlock rc_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (*cow_refcount((uint64)pa) != 0){
    printf("kfree: rc = %d", *cow_refcount((uint64)pa));
    panic("kfree a mapped page");
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  *cow_refcount((uint64)r) = 0;
  return (void *)r;
}

int cow_map(pte_t* pte_parent, pagetable_t child_pagetable, uint64 va) {
  uint64 old_pte_parent = *pte_parent;
  uint64 new_pte_parent = (old_pte_parent & ~PTE_W) | PTE_COW | PTE_R | PTE_X;
  uint64 flags = PTE_FLAGS(new_pte_parent);
  uint64 pa = PTE2PA(new_pte_parent);
  if(mappages(child_pagetable, va, PGSIZE, pa, flags) != 0) {
    return -1;
  }
  *pte_parent = new_pte_parent;
  return 0;
}

unsigned char* cow_refcount(uint64 pa) {
  return &rc[pa >> 12];
}