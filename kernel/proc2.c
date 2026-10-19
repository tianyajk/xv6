#include "lab2.h"
#include <userentry.h> /* 只使用构建目录中的生成元数据，避免旧文件遮蔽。 */
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
      if (vm_process(p) < 0) {
        p->state = FREE;
        return 0;
      }
      return p;
    }
  return 0;
}
static void release_memory3(struct process *p) {
  vm_destroy3(p->pagetable);
  p->pagetable = 0;
  if (p->tf) {
    kfree3(p->tf);
    p->tf = 0;
  }
}
int proc_exec(const char *name) {
  unsigned char *q = _uprog_table;
  while (*(uint64 *)q) {
    uint64 start = *(uint64 *)q, end = *(uint64 *)(q + 8);
    const char *n = (char *)(q + 16);
    if (!kstrcmp(n, name)) {
      for (uint64 i = 0; i < sizeof(user_entries) / sizeof(user_entries[0]); i++) {
        if (kstrcmp(user_entries[i].name, name))
          continue;
        pagetable_t next = vm_create3(current->tf);
        if (!next)
          return -1;
        for (uint64 j = 0; j < user_entries[i].nsections; j++) {
          const struct image_section3 *s = &user_entries[i].sections[j];
          for (uint64 va = PGROUNDDOWN(s->va); va < s->va + s->size; va += PGSIZE) {
            pte_t *pte = walk3(next, va, 0);
            if (pte && (*pte & PTE_V)) {
              *pte |= s->flags | ((s->flags & PTE_W) ? PTE_D : 0);
            } else if (vm_alloc3(next, va, va + PGSIZE, s->flags) < 0)
              goto fail;
          }
        }
        if (vm_load3(next, 0, (void *)start, end - start) < 0 ||
            vm_alloc3(next, UGUARD_BEGIN, USTACK_TOP - USTACK_PAGES * PGSIZE,
                      PTE_R | PTE_W) < 0 ||
            vm_alloc3(next, USTACK_TOP - USTACK_PAGES * PGSIZE, USTACK_TOP,
                      PTE_R | PTE_W | PTE_U) < 0)
          goto fail;
        /* 所有可能失败的工作先完成，最后一次提交新的地址空间。 */
        pagetable_t old = current->pagetable;
        current->pagetable = next;
        kmemset(current->tf, 0, sizeof(*current->tf));
        current->tf->epc = user_entries[i].entry;
        current->tf->sp = USTACK_TOP;
        current->heap = current->heap_base = PGROUNDUP(user_entries[i].end);
        vm_destroy3(old);
        return 0;
      fail:
        vm_destroy3(next);
        return -1;
      }
      return -1;
    }
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
  dump_pagetable(current->pagetable);
}
int proc_fork(void) {
  struct process *p = allocproc2();
  if (!p)
    return -1;
  if (vm_clone3(current->pagetable, p->pagetable) < 0) {
    release_memory3(p);
    p->state = FREE;
    return -1;
  }
  *p->tf = *current->tf;
  p->tf->a0 = 0;
  p->parent = current->pid;
  p->heap = current->heap;
  p->heap_base = current->heap_base;
  return p->pid;
}
int proc_wait(uint64 addr) {
  int found = 0;
  if (addr && vm_check3(current->pagetable, addr, sizeof(int), 1) < 0)
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
      vm_copy3(parent->pagetable, &p->status, parent->waitaddr, sizeof(p->status), 1);
    parent->tf->a0 = p->pid;
    parent->state = READY;
    p->state = FREE;
    return;
  }
}
void proc_exit(int status) {
  struct process *old = current;
  old->status = status;
  release_memory3(old);
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
    if (vm_process(current) < 0)
      panic2("restart address space");
    if (proc_exec("sh") < 0)
      panic2("restart sh");
    return;
  }
  panic2("no runnable process");
}
void proc_resume(void) {
  intr_off();
  current->tf->kernel_satp = kernel_satp;
  current->tf->kernel_sp = (uint64)(current->stack + sizeof(current->stack));
  current->tf->kernel_trap = (uint64)usertrap;
  current->tf->kernel_hartid = 0;
  w_stvec(TRAMPOLINE + (uservec - trampoline));
  w_sepc(current->tf->epc);
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  ((void (*)(uint64))(TRAMPOLINE + (userret - trampoline)))(
      MAKE_SATP(current->pagetable));
  __builtin_unreachable();
}
