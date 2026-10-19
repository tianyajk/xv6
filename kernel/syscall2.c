#include "lab2.h"
#include "syscall.h"
static uint64 arg(int n) {
  switch (n) {
  case 0:
    return current->tf->a0;
  case 1:
    return current->tf->a1;
  default:
    return current->tf->a2;
  }
}
void syscall2(void) {
  uint64 num = current->tf->a7, a0 = arg(0), a1 = arg(1), a2 = arg(2);
  long result = -1;
  switch (num) {
  case SYS_fork:
    result = proc_fork();
    break;
  case SYS_exit:
    proc_exit((int)a0);
    return;
  case SYS_wait:
    result = proc_wait(a0);
    if (result == -2)
      return;
    break;
  case SYS_exec: {
    char name[32];
    if (copystr2(name, a0, sizeof(name)) == 0) {
      for (int i = 0; name[i]; i++)
        if (name[i] == '\n' || name[i] == '\r') {
          name[i] = 0;
          break;
        }
      result = proc_exec(name);
    }
    break;
  }
  case SYS_read:
    if (a0 == 0)
      result = console_read2(a1, (int)a2);
    break;
  case SYS_write:
    if ((a0 == 1 || a0 == 2) && a2 <= (uint64)0x7fffffff &&
        vm_check3(current->pagetable, a1, a2, 0) == 0) {
      char buf[64];
      uint64 off = 0;
      while (off < a2) {
        int n = a2 - off > sizeof(buf) ? sizeof(buf) : (int)(a2 - off);
        if (copyin2(buf, a1 + off, n) < 0)
          break;
        for (int i = 0; i < n; i++)
          uartputc_sync(buf[i]);
        off += n;
      }
      result = off;
    }
    break;
  case SYS_getpid:
    result = current->pid;
    break;
  case SYS_uptime:
    result = ticks;
    break;
  case SYS_kill:
    for (int i = 0; i < NPROC2; i++)
      if (procs[i].state != FREE && procs[i].pid == (int)a0 && procs[i].pid != 1) {
        procs[i].killed = 1;
        result = 0;
        break;
      }
    break;
  case SYS_sbrk: {
    uint64 old;
    if (vm_sbrk3(current, (int)a0, &old) == 0)
      result = old;
    break;
  }
  case SYS_pause: {
    if ((int)a0 < 0)
      break;
    uint64 end = ticks + (unsigned int)a0;
    while (ticks < end && !current->killed) {
      asm volatile("wfi");
      intr_on();
      asm volatile("nop");
      intr_off();
    }
    result = current->killed ? -1 : 0;
    break;
  }
  case SYS_close:
    if (a0 < 3)
      result = 0;
    break;
  case SYS_sync:
    result = 0;
    break;
  case SYS_vmstats3: {
    struct vmstats3 s;
    kmemset(&s, 0, sizeof(s));
    kstats3(&s);
    s.guard_begin = UGUARD_BEGIN;
    s.guard_end = USTACK_TOP - USTACK_PAGES * PGSIZE;
    s.heap = current->heap;
    s.heap_base = current->heap_base;
    result = copyout2(a0, &s, sizeof(s));
    break;
  }
  case SYS_vmtest3:
    result = vm_selftest3();
    break;
  case SYS_vmfail3:
    kfail3((long)a0);
    result = 0;
    break;
  case SYS_dump3:
    for (int i = 0; i < NPROC2; i++)
      if (procs[i].state != FREE && procs[i].pagetable &&
          (!a0 ? &procs[i] == current : procs[i].pid == (int)a0)) {
        dump_pagetable(procs[i].pagetable);
        result = 0;
        break;
      }
    break;
  default:
    result = -1;
    break;
  }
  current->tf->a0 = (uint64)result;
}
