/*
 * lab1 · console.c —— 控制台输出层（第 3 步：先只点亮一个字符）
 * ============================================================================
 * UART 16550 轮询输出协议（设计笔记思考题 5）
 *
 * 硬件事实（来自 memlayout.h，不可改）：
 *   基地址 UART0 = 0x10000000（QEMU virt 规范，全班统一）
 *   寄存器按字节偏移编址，本文件只用到两个：
 *     偏移 0  THR —— 发送保持寄存器。往这里写一个字节 = 发出一个字符
 *     偏移 5  LSR —— 线路状态寄存器。读它判断"能不能发"
 *
 * 状态机：
 *   循环忙等 LSR 的 bit5（THRE，发送保持寄存器空）变为 1，
 *   然后向 THR 写入字符。
 *
 * 为什么是忙等而不是 sleep：
 *   启动期间没有使能任何中断、也没有时钟，睡眠后没人能唤醒你 ——
 *   忙等是此刻唯一可靠的行为。lab2 引入中断后才会换成 sleep/wakeup。
 *
 * 并发假设：启动早期只有单核、单上下文在调用，声明不可重入。
 * ============================================================================
 */
#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "course_sid.h"

#define THR 0 /* 写：发送保持寄存器 (Transmit Holding Register) */
#define LSR 5 /* 读：线路状态寄存器 (Line Status Register)       */

#define LSR_TX_IDLE (1 << 5) /* bit5 = THRE，置 1 表示可以写下一个字符 */

/* 把"物理地址 + 偏移"变成可解引用的指针。
 * volatile 不可省：编译器不知道这个地址背后是硬件而不是内存，
 * 没有它，优化器会把"重复读同一个地址"合并成一次，忙等就变成死循环。 */
#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

/* 本函数只负责"单字节硬件发射"。学号 banner 的个性化修饰在上层做（层次分明）。*/
void
uartputc_sync(char c)
{
  while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;                /* 忙等：发送器还忙着，写进去会被丢 */
  WriteReg(THR, c);  /* 此刻 THR 空，字符落进硬件 */
}
