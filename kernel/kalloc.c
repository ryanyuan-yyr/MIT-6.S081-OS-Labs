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

struct run {
  struct run *next;
};

typedef struct {
  struct spinlock lock;
  struct run *freelist;
} kmem_t;

kmem_t kmem_array[NCPU];
char kmem_lock_names[NCPU][8];

kmem_t *kmem() {
  int cpu_id = cpuid();
  return kmem_array + cpu_id;
}

void
kinit()
{
  push_off();
  for (int cpu_id = 0; cpu_id < NCPU; cpu_id++)
  {
    if (snprintf(kmem_lock_names[cpu_id], 8, "kmem_%d", cpu_id) < 0) {
      pop_off();
      panic("kinit");
    }
    initlock(&kmem_array[cpu_id].lock, kmem_lock_names[cpu_id]);
    uint64 mem_size = PHYSTOP - (uint64)end;
    void *free_start = end + mem_size / NCPU * cpu_id;
    void *free_end = cpu_id == NCPU - 1 ? (void *)PHYSTOP : free_start + mem_size / NCPU - 1;
    freerange(free_start, free_end);
  }
  
  pop_off();
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  acquire(&kmem()->lock);
  r->next = kmem()->freelist;
  kmem()->freelist = r;
  release(&kmem()->lock);
  pop_off();
}

void *kalloc_from(int cpu_id);

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  push_off();
  int cpu_id = cpuid();
  void *r;
  if ((r = kalloc_from(cpu_id))) {
    pop_off();
    return r;
  }
  int steal_page_n = 0;
  for (int i = 1; i < NCPU; i++)
  {
    void* r = kalloc_from((cpu_id + i) % NCPU);
    if (r) {
      steal_page_n++;
      kfree(r);
      if (steal_page_n >= 1024)
        break;
    }
  }
  if ((r = kalloc_from(cpu_id))) {
    pop_off();
    return r;
  }
  pop_off();
  return (void *)0;
}

void *
kalloc_from(int cpu_id)
{
  struct run *r;
  kmem_t *kmem = kmem_array + cpu_id;

  acquire(&kmem->lock);
  r = kmem->freelist;
  if(r)
    kmem->freelist = r->next;
  release(&kmem->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
