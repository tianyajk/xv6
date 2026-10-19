#ifndef LAB2_H
#define LAB2_H
#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "course_sid.h"
#define NPROC2 8
/* 与预置 trampoline.S 的字节偏移逐项一致。 */
struct trapframe {
  uint64 kernel_satp, kernel_sp, kernel_trap, epc, kernel_hartid;
  uint64 ra, sp, gp, tp, t0, t1, t2, s0, s1, a0, a1, a2, a3, a4, a5, a6, a7;
  uint64 s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, t3, t4, t5, t6;
};
_Static_assert(__builtin_offsetof(struct trapframe, a0) == 112, "trapframe a0 ABI");
_Static_assert(__builtin_offsetof(struct trapframe, t6) == 280, "trapframe t6 ABI");
enum pstate { FREE, READY, RUNNING2, BLOCKED_IO, WAITING, ZOMBIE2 };
struct process {
  int pid, parent, status, killed;
  int reap_owner, pending_reap;
  enum pstate state;
  uint64 waitaddr, heap, heap_base;
  struct trapframe *tf;
  pagetable_t pagetable;
  unsigned char stack[8192] __attribute__((aligned(16)));
};
extern struct process procs[NPROC2], *current;
extern volatile uint64 ticks, rx_irqs, rx_dropped, rx_wakeups;
extern uint64 kernel_satp;
extern char trampoline[], uservec[], userret[], kernelvec[], timervec[];
void *kmemset(void *, int, uint64);
void *kmemcpy(void *, const void *, uint64);
int kstrcmp(const char *, const char *);
void panic2(const char *) __attribute__((noreturn));
int printf(const char *, ...);
void uartputc_sync(char);
void vm_init(void);
int vm_process(struct process *);
int copyin2(void *, uint64, int);
int copyout2(uint64, const void *, int);
int copystr2(char *, uint64, int);
void proc_init(void);
int proc_fork(void);
int proc_exec(const char *);
int proc_wait(uint64);
void proc_exit(int);
void proc_resume(void) __attribute__((noreturn));
uint64 usertrap(void);
void kerneltrap(void);
void irq_init(void);
int devintr2(void);
void console_init2(void);
void consoleintr2(void);
int console_read2(uint64, int);
void syscall2(void);
void lab2_main(void) __attribute__((noreturn));
#include "lab3.h"
#endif
