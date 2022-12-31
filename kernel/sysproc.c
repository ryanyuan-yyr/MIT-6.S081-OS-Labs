#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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

int sys_sigalarm(void) {
  int period;
  void (*handler)();
  if (argint(0, &period) < 0)
    return -1;
  if (argaddr(1, (uint64*)&handler) < 0)
    return -1;
  struct proc* proc = myproc();
  struct timing_signal timing_signal = {
    handler, 
    period, 
    0, 
    0, 
  };
  proc->timing_signal = timing_signal;
  return 0;
}
int sys_sigreturn(void) {
  struct proc* p = myproc();
  p->trapframe->epc = p->full_context.epc;
  p->trapframe->ra = p->full_context.ra;
  p->trapframe->sp = p->full_context.sp;
  p->trapframe->gp = p->full_context.gp;
  p->trapframe->tp = p->full_context.tp;
  p->trapframe->t0 = p->full_context.t0;
  p->trapframe->t1 = p->full_context.t1;
  p->trapframe->t2 = p->full_context.t2;
  p->trapframe->s0 = p->full_context.s0;
  p->trapframe->s1 = p->full_context.s1;
  p->trapframe->a0 = p->full_context.a0;
  p->trapframe->a1 = p->full_context.a1;
  p->trapframe->a2 = p->full_context.a2;
  p->trapframe->a3 = p->full_context.a3;
  p->trapframe->a4 = p->full_context.a4;
  p->trapframe->a5 = p->full_context.a5;
  p->trapframe->a6 = p->full_context.a6;
  p->trapframe->a7 = p->full_context.a7;
  p->trapframe->s2 = p->full_context.s2;
  p->trapframe->s3 = p->full_context.s3;
  p->trapframe->s4 = p->full_context.s4;
  p->trapframe->s5 = p->full_context.s5;
  p->trapframe->s6 = p->full_context.s6;
  p->trapframe->s7 = p->full_context.s7;
  p->trapframe->s8 = p->full_context.s8;
  p->trapframe->s9 = p->full_context.s9;
  p->trapframe->s10 = p->full_context.s10;
  p->trapframe->s11 = p->full_context.s11;
  p->trapframe->t3 = p->full_context.t3;
  p->trapframe->t4 = p->full_context.t4;
  p->trapframe->t5 = p->full_context.t5;
  p->trapframe->t6 = p->full_context.t6;
  // memmove((void*)(&p->trapframe->ra), (void*)(&p->full_context.ra),
  //         sizeof(p->full_context) - sizeof(p->full_context.epc));
  p->timing_signal.is_signaling = 0;
  return 0;
}