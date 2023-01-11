#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

extern struct mmap_area mmap_areas[MAXMMAPN];

uint64
sys_mmap(void) {
  // uint64 addr;
  int length;
  int prot;
  int flags;
  int fd;
  int offset;

  if (
    // argaddr(0, &addr)  < 0 ||
    argint(1, &length) < 0 ||
    argint(2, &prot)   < 0 ||
    argint(3, &flags)  < 0 ||
    argint(4, &fd)     < 0 ||
    argint(5, &offset) < 0
  ) {
    return -1;
  }

  struct proc* p = myproc();
  if (fd >= NOFILE) {
    return -1;
  }
  struct file* f = p->ofile[fd];
  if (!f) {
    return -1;
  }

  struct mmap_area* ma = 0;

  for (int i = 0; i < MAXMMAPN; i++)
  {
    if (!mmap_areas[i].p) {
      ma = mmap_areas + i;
      break;
    }
  }

  if (!ma)
    return -1;

  if (prot & PROT_READ && !f->readable)
    return -1;
  if (prot & PROT_WRITE && !f->writable && !(flags & MAP_PRIVATE))
    return -1;

  uint64 mapped_start = PGROUNDUP(p->sz);
  for (int i = 0; i < MAXMMAPN; i++) {
    uint64 mapped_end = mmap_areas[i].mapped_start + mmap_areas[i].len;
    if (mmap_areas[i].p == p && mapped_end > mapped_start) {
      mapped_start = PGROUNDUP(mapped_end);
    }
  }

  ma->p = p;
  ma->mapped_start = mapped_start;
  ma->file_start = ma->mapped_start - offset;
  ma->len = length;
  ma->permission = prot;
  ma->flags = flags;
  ma->f = f;
  filedup(f);

  return ma->mapped_start;
}

uint64
munmap(struct mmap_area *ma, uint64 addr, int len) {
  int should_close_file = (int)(addr <= ma->mapped_start && len >= ma->len);
  uint64 unmap_start;
  uint64 unmap_len;
  if (should_close_file) {
    unmap_start = ma->mapped_start;
    unmap_len = ma->len;
  } else {
    unmap_start = addr;
    unmap_len = len;
  }
  uint offset = unmap_start - ma->file_start;

  if (ma->flags & MAP_SHARED && !(ma->flags & MAP_PRIVATE)) {
    for (uint64 mmapped_pg = addr; mmapped_pg < addr + unmap_len;
         mmapped_pg += PGSIZE) {
      pte_t* pte = walk(ma->p->pagetable, mmapped_pg, 0);
      if (*pte & PTE_V && *pte & PTE_D) {
        int n = (mmapped_pg + PGSIZE >= addr + unmap_len
                     ? addr + unmap_len - mmapped_pg
                     : PGSIZE);
        if (filewrite_inode(ma->f->ip, 0, PTE2PA(*pte), n, &offset) != n) {
          return -1;
        }
      }
    }
  }

  for (uint64 mmapped_pg = addr; mmapped_pg < addr + unmap_len;
       mmapped_pg += PGSIZE) {
    pte_t* pte = walk(ma->p->pagetable, mmapped_pg, 0);
    if (*pte & PTE_V) {
      uvmunmap(ma->p->pagetable, mmapped_pg, 1, 1);
    }
  }

  if (should_close_file) {  // unmap the whole mmap area
    ma->p = 0;
    fileclose(ma->f);
  } else {
    if (addr == ma->mapped_start) {
      ma->mapped_start = addr + len;
    } else {
      ma->len -= len;
    }
  }
  return 0;
}

uint64
sys_munmap(void) {
  uint64 addr;
  int len;
  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0)
    return -1;

  if (addr % PGSIZE != 0)
    return -1;

  struct proc* p = myproc();
  struct mmap_area* ma = 0;
  for (ma = mmap_areas; ma != mmap_areas + MAXMMAPN; ma++) {
    if (ma->p == p && (ma->mapped_start == addr || ma->mapped_start + ma->len == addr + len))
      break;
  }
  if (ma == mmap_areas + MAXMMAPN)
    return -1;

  return munmap(ma, addr, len);
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
