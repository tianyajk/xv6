#include "lab2.h"
#include "userentry.h"
struct process procs[NPROC2], *current;
static int nextpid;
extern unsigned char _uprog_table[];
void panic2(const char *s) {
  printf("\nPANIC: %s\n", s);
  intr_off();
  for (;;)
    asm volatile("wfi");
}
static struct process *allocproc2(void) {
  for (int i = 0; i < NPROC2; i++)
    if (procs[i].state == FREE) {
      struct process *p = &procs[i];
      kmemset(p, 0, sizeof(*p));
      p->pid = nextpid++;
      p->state = READY;
      vm_process(p);
      return p;
    }
  return 0;
}
int proc_exec(const char *name) {
  unsigned char *q = _uprog_table;
  while (*(uint64 *)q) {
    uint64 start = *(uint64 *)q, end = *(uint64 *)(q + 8);
    char *n = (char *)(q + 16);
    if (!kstrcmp(n, name)) {
      if (end < start || end - start > USIZE / 2)
        return -1;
      kmemset(current->mem, 0, USIZE);
      kmemcpy(current->mem, (void *)start, end - start);
      kmemset(&current->tf, 0, sizeof(current->tf));
      current->tf.sp = USIZE;
      for (uint64 i = 0; i < sizeof(user_entries) / sizeof(user_entries[0]); i++)
        if (!kstrcmp(user_entries[i].name, name)) {
          current->tf.epc = user_entries[i].entry;
          current->heap = (user_entries[i].end + 4095) & ~4095UL;
          return 0;
        }
      panic2("missing ELF entry metadata");
    }
    /* 预置生成器把程序字节夹在相邻表项之间；跳过本映像。 */
    q = (unsigned char *)((end + 7) & ~7UL);
  }
  return -1;
}
void proc_init(void) {
  kmemset(procs, 0, sizeof(procs));
  nextpid = 1;
  current = allocproc2();
  if (!current)
    panic2("init alloc");
  if (proc_exec("sh") < 0)
    panic2("init exec");
  current->state = RUNNING2;
}
int proc_fork(void) {
  struct process *p = allocproc2();
  if (!p)
    return -1;
  kmemcpy(p->mem, current->mem, USIZE);
  p->tf = current->tf;
  p->tf.a0 = 0;
  p->parent = current->pid;
  p->heap = current->heap;
  return p->pid;
}
int proc_wait(uint64 addr) {
  int found = 0;
  if (addr && (addr > USIZE - sizeof(int)))
    return -1;
  for (int i = 0; i < NPROC2; i++) {
    struct process *p = &procs[i];
    if (p->state == FREE || p->parent != current->pid)
      continue;
    found = 1;
    if (p->state == ZOMBIE2 && p->pending_reap == 0) {
      int pid = p->pid;
      if (addr)
        copyout2(addr, &p->status, sizeof(int));
      p->state = FREE;
      return pid;
    }
  }
  if (!found)
    return -1;
  current->waitaddr = addr;
  current->state = WAITING;
  for (int i = 0; i < NPROC2; i++)
    if (procs[i].state == READY) {
      current = &procs[i];
      current->state = RUNNING2;
      return -2;
    }
  panic2("wait without runnable child");
}
/* 退出先完成其托管子树。pending_reap 是依赖计数，不是时间片调度。
   这样祖先不能先于内部 wait/exit 完成交付，READY 孤儿也不会泄漏。 */
static void finish_exit(struct process *p) {
  if (p->state != ZOMBIE2 || p->pending_reap != 0)
    return;
  if (p->parent == 0 && p->pid != 1) {
    int owner = p->reap_owner;
    p->state = FREE;
    for (int i = 0; i < NPROC2; i++)
      if (procs[i].state != FREE && procs[i].pid == owner) {
        if (procs[i].pending_reap <= 0)
          panic2("reaper accounting");
        procs[i].pending_reap--;
        finish_exit(&procs[i]);
        return;
      }
    panic2("orphan without reaper owner");
  }
  for (int i = 0; i < NPROC2; i++) {
    struct process *parent = &procs[i];
    if (parent->pid != p->parent || parent->state != WAITING)
      continue;
    if (parent->waitaddr)
      kmemcpy(parent->mem + parent->waitaddr, &p->status, sizeof(p->status));
    parent->tf.a0 = p->pid;
    parent->state = READY;
    p->state = FREE;
    return;
  }
}
void proc_exit(int status) {
  struct process *old = current;
  old->status = status;
  old->state = ZOMBIE2;
  /* 先登记全部依赖，再回收，避免第一名子进程结束时提前释放 owner。 */
  for (int i = 0; i < NPROC2; i++)
    if (procs[i].state != FREE && procs[i].parent == old->pid) {
      procs[i].parent = 0;
      procs[i].reap_owner = old->pid;
      old->pending_reap++;
    }
  for (int i = 0; i < NPROC2; i++)
    if (procs[i].state == ZOMBIE2 && procs[i].reap_owner == old->pid)
      finish_exit(&procs[i]);
  finish_exit(old);
  for (int i = 0; i < NPROC2; i++)
    if (procs[i].state == READY) {
      current = &procs[i];
      current->state = RUNNING2;
      return;
    }
  if (procs[0].pid == 1 && procs[0].state == ZOMBIE2 && !procs[0].pending_reap) {
    current = &procs[0];
    current->state = RUNNING2;
    current->killed = 0;
    if (proc_exec("sh") < 0)
      panic2("restart sh");
    return;
  }
  panic2("no runnable process");
}
void proc_resume(void) {
  intr_off();
  current->tf.kernel_satp = kernel_satp;
  current->tf.kernel_sp = (uint64)(current->stack + sizeof(current->stack));
  current->tf.kernel_trap = (uint64)usertrap;
  current->tf.kernel_hartid = 0;
  w_stvec(TRAMPOLINE + (uservec - trampoline));
  w_sepc(current->tf.epc);
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  ((void (*)(uint64))(TRAMPOLINE + (userret - trampoline)))(MAKE_SATP(current->pt[0]));
  __builtin_unreachable();
}
