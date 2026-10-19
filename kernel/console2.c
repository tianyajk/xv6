#include "lab2.h"
static unsigned char input[LAB2_BUF_SIZE];
static uint64 rd, wr;
void console_init2(void) {
  rd = wr = 0;
  kmemset(input, 0, sizeof(input));
}
static int available(void) {
  if (rd == wr)
    return 0;
  if (LAB2_BUF_SEMANTICS == 1)
    return 1;
  for (uint64 i = rd; i < wr; i++)
    if (input[i % LAB2_BUF_SIZE] == '\n' || input[i % LAB2_BUF_SIZE] == 4)
      return 1;
  return wr - rd == LAB2_BUF_SIZE;
}
void consoleintr2(void) {
  rx_irqs++;
  while (*(volatile unsigned char *)(UART0 + 5) & 1) {
    int c = *(volatile unsigned char *)UART0;
    if (c == 16) {
      printf("\n[lab2 ticks=%ld rx=%ld dropped=%ld wakeups=%ld]\n", (long)ticks,
             (long)rx_irqs, (long)rx_dropped, (long)rx_wakeups);
      struct vmstats3 stats;
      kstats3(&stats);
      printf("[vm3 used=%ld free=%ld limit=%ld]\n", (long)stats.used, (long)stats.free,
             (long)stats.limit);
      continue;
    }
    if (c == 3) {
      if (current && current->pid != 1)
        current->killed = 1;
      uartputc_sync('^');
      uartputc_sync('C');
      uartputc_sync('\n');
      continue;
    }
    if (c == '\r')
      c = '\n';
    if (LAB2_BUF_SEMANTICS == 0 && (c == 127 || c == 8)) {
      if (wr != rd && input[(wr - 1) % LAB2_BUF_SIZE] != '\n') {
        wr--;
        uartputc_sync('\b');
        uartputc_sync(' ');
        uartputc_sync('\b');
      }
      continue;
    }
    if (wr - rd == LAB2_BUF_SIZE) {
      rx_dropped++;
      continue;
    }
    input[wr++ % LAB2_BUF_SIZE] = (unsigned char)c;
    if (c != 4)
      uartputc_sync(c);
    if (available() && current && current->state == BLOCKED_IO) {
      current->state = RUNNING2;
      rx_wakeups++;
    }
  }
}
int console_read2(uint64 dst, int n) {
  if (n < 0 || vm_check3(current->pagetable, dst, (uint64)n, 1) < 0)
    return -1;
  if (n == 0)
    return 0;
  /* 单核：检查和 WFI 均在 SIE=0 下，避免检查后、睡眠前丢失唤醒。
     WFI 可因已使能的待处理中断返回，再短暂开中断交给 kerneltrap 消费。 */
  while (!available()) {
    if (current->killed) {
      current->state = RUNNING2;
      return -1;
    }
    current->state = BLOCKED_IO;
    asm volatile("wfi");
    intr_on();
    asm volatile("nop");
    intr_off();
  }
  current->state = RUNNING2;
  int count = 0;
  while (count < n && rd < wr) {
    unsigned char c = input[rd % LAB2_BUF_SIZE];
    if (c == 4) {
      if (count == 0)
        rd++;
      break;
    }
    rd++;
    if (copyout2(dst + count, &c, 1) < 0)
      return -1;
    count++;
    if (c == '\n')
      break;
  }
  return count;
}
