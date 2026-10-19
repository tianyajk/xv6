#include "lab2.h"
volatile uint64 ticks, rx_irqs, rx_dropped, rx_wakeups;
void irq_init(void) {
  ticks = rx_irqs = rx_dropped = rx_wakeups = 0;
  console_init2();
  *(volatile uint32 *)(PLIC + UART0_IRQ * 4) = 1;
  *(volatile uint32 *)PLIC_SENABLE(0) = 1 << UART0_IRQ;
  *(volatile uint32 *)PLIC_SPRIORITY(0) = 0;
  /* UART 16550：8N1，FIFO，接收中断；沿用 Lab1 的轮询发送。 */
  *(volatile unsigned char *)(UART0 + 1) = 0;
  *(volatile unsigned char *)(UART0 + 3) = 3;
  *(volatile unsigned char *)(UART0 + 2) = 7;
  *(volatile unsigned char *)(UART0 + 1) = 1;
  w_stvec((uint64)kernelvec);
  w_sie((1UL << 1) | (1UL << 9));
}
int devintr2(void) {
  uint64 cause = r_scause();
  if (cause == ((1UL << 63) | 9)) {
    int irq = *(volatile uint32 *)PLIC_SCLAIM(0);
    if (irq == UART0_IRQ)
      consoleintr2();
    if (irq)
      *(volatile uint32 *)PLIC_SCLAIM(0) = irq;
    return 1;
  }
  if (cause == ((1UL << 63) | 1)) {
    ticks++;
    w_sip(r_sip() & ~2UL);
    return 1;
  }
  return 0;
}
void kerneltrap(void) {
  uint64 epc = r_sepc(), status = r_sstatus();
  if (!devintr2()) {
    printf("kernel cause=%lx epc=%lx val=%lx\n", r_scause(), epc, r_stval());
    panic2("kernel trap");
  }
  w_sepc(epc);
  w_sstatus(status);
}
uint64 usertrap(void) {
  w_stvec((uint64)kernelvec);
  current->tf->epc = r_sepc();
  if (r_scause() == 8) {
    current->tf->epc += 4;
    syscall2();
  } else if (!devintr2()) {
    printf("user fault pid=%d cause=%ld epc=%lx\n", current->pid, (long)r_scause(),
           current->tf->epc);
    proc_exit(-1);
  }
  while (current->killed)
    proc_exit(-1);
  proc_resume();
}
void lab2_main(void) {
  vm_init();
  irq_init();
  proc_init();
  proc_resume();
}
