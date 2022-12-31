#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 va;
  int npages;
  char* mask_addr;

  if(argaddr(0, &va) < 0)
    return -1;
  if(argint(1, &npages) < 0)
    return -1;
  if(argaddr(2, (uint64*)&mask_addr) < 0)
    return -1;

  va = PGROUNDDOWN(va);
  
  uint64 mask = 0;
  pagetable_t pagetable = myproc()->pagetable;

  for(uint64 a = va, i = 0; a < va + npages*PGSIZE; a += PGSIZE, i++){
    pte_t *pte;
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("sys_pgaccess: walk");
    if((*pte & PTE_V) == 0)
      panic("sys_pgaccess: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("sys_pgaccess: not a leaf");
    if((*pte & PTE_A) != 0){
      mask |= (1L << i);
      *pte &= ~PTE_A;
    }
  }
  copyout(pagetable, (uint64)mask_addr, (char*)&mask, sizeof(mask));
  return 0;
}
#endif

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
