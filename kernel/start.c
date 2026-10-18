/* Lab1 启动基础 + Lab2 的机器定时器/中断委托。 */
#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "course_sid.h"

void main(void);
extern char mtrapvec[], timervec[];
__attribute__((aligned(16))) char stack0[LAB1_STACK_KB * 1024];
/* timervec 使用的每核临时区；本实验仅 hart 0 会进入 start。 */
static uint64 timer_scratch[5] __attribute__((aligned(16)));

void start(void) {
  uint64 status = r_mstatus();
  status = (status & ~MSTATUS_MPP_MASK) | MSTATUS_MPP_S;
  status &= ~((1UL << 3) | (1UL << 1) | (1UL << 5));
  w_mstatus(status);
  w_mepc((uint64)main);
  w_satp(0); /* main 建好内核页表前仍使用物理地址。 */
  asm volatile("csrw mtvec, %0" : : "r"(mtrapvec));
  /* 0xf 的 A=01 是 TOR，R/W/X=1；授权 S 态访问物理地址。 */
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  /* 用户 ecall/异常交给 S 态；SSIP 和 UART 外部中断也交给 S 态。 */
  w_medeleg(0xffff);
  w_mideleg((1UL << 1) | (1UL << 9));
  volatile uint64 *compare = (uint64 *)0x02004000UL;
  uint64 interval = 100000UL * LAB2_TICK; /* virt 的 mtime=10MHz，基本周期10ms。 */
  *compare = *(volatile uint64 *)0x0200bff8UL + interval;
  timer_scratch[3] = (uint64)compare;
  timer_scratch[4] = interval;
  asm volatile("csrw mscratch, %0" : : "r"(timer_scratch));
  asm volatile("csrw mtvec, %0" : : "r"(timervec));
  w_mie(1UL << 7); /* M 定时器只置 SSIP；不在 M 态运行调度/控制台逻辑。 */
  asm volatile("mret");
  __builtin_unreachable();
}
