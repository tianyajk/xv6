#include "lab2.h"
#include "syscall.h"
static uint64 arg(int n) {
  uint64 *a = &current->tf.a0;
  return a[n];
}
void syscall2(void) {
  uint64 num = current->tf.a7, a0 = arg(0), a1 = arg(1), a2 = arg(2);
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
      /* 课程 sh 直接把 gets 的换行交给 exec；只去掉行结束符。 */
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
    if ((a0 == 1 || a0 == 2) && a2 <= USIZE && a1 <= USIZE && a2 <= USIZE - a1) {
      for (uint64 i = 0; i < a2; i++)
        uartputc_sync(current->mem[a1 + i]);
      result = (long)a2;
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
  case SYS_sbrk:
    if ((long)(int)a0 >= 0 && current->heap + (uint64)(int)a0 <= USIZE - 8192) {
      result = current->heap;
      current->heap += (int)a0;
    }
    break;
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
  default:
    result = -1;
    break;
  }
  current->tf.a0 = (uint64)result;
}
