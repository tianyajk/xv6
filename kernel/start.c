/*
 * lab1 · start.c —— M 态的最后一段代码：把 CPU 交给 S 态的 main()
 * ============================================================================
 * 谁在等这个文件交货？
 *   entry.S  : la sp, stack0   → 要符号 stack0
 *   entry.S  : call start      → 要符号 start
 *   链接器   : start.c 引用 main → 要符号 main（在 main.c）
 * 三个符号都必须是外部可见的（不能加 static），否则链接器看不见。
 * ============================================================================
 */
#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "course_sid.h"

void main(void);
void mtrapvec(void); /* 定义在 entry.S，带 .globl */

/* ---------------------------------------------------------------------------
 * stack0 —— 内核栈的内存本体
 *
 * 【为什么必须定义在这里、长成这个样】
 *  · 名字必须精确是 stack0：entry.S 里 `la sp, stack0` 是外部引用，
 *    靠链接期解析。拼错、或加了 static，都会 undefined reference。
 *  · 必须是【数组定义】而不是指针：entry.S 取的是它的【地址】，
 *    内存本体由链接器在 .bss 里划出来。
 *  · 长度必须走 LAB1_STACK_KB 宏：说明书 §3 严禁硬编码数字，
 *    且这是"sp 初值必须与之强一致"的那道定位题的答案。
 *  · 16 字节对齐：RISC-V psABI 要求函数入口处 sp 必须 16 字节对齐。
 *    entry.S 算出的栈顶 = stack0 + LAB1_STACK_KB*1024，
 *    只要基数 16 对齐、偏移是 16 的倍数，结果就 16 对齐。
 *
 * 【关于 .bss 清零】kernel.ld 的注释说 .bss "启动时由内核自行清零"，
 * 但这个链接脚本没有导出 __bss_start/__bss_end，而且它"预置·不可改"——
 * 拿不到边界符号就没法安全清零。lab1 进 .bss 的唯一对象就是 stack0，
 * 而它【只取地址、从不读初值】，所以不清零不影响正确性。
 * （这一点值得写进设计笔记：契约与可实施性不一致，我做了取舍。）
 * ------------------------------------------------------------------------- */
__attribute__((aligned(16))) char stack0[LAB1_STACK_KB * 1024];

void
start(void)
{
  /* ---- ① mstatus：降权目标 + 中断开关 ------------------------------------
   * MPP 决定 mret 之后回到哪个特权级 —— 这是降权的【唯一】途径。
   * 读-改-写而不是直接写常量：mstatus 里还有别的域，整体覆盖会误伤。
   *
   * 顺便把三个中断位清掉（这是设计笔记思考题 3 点名要答的"mstatus.MIE 状态"）：
   *   bit3 MIE  = M 态中断总开关  —— entry.S 已清过，这里再确认一次
   *   bit1 SIE  = S 态中断总开关
   *   bit5 SPIE = "上一次 S 态的中断开关"暂存位
   *
   * mret 会按 MPIE 更新 MIE，但不会做 SIE ← SPIE（那是 sret 的行为）。
   * lab1 把 SIE 和 SPIE 都置零，保证当前不响应 S 态中断，
   * 且未来若走 sret 路径也不会意外打开中断。
   */
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;                              /* 降权目标 = S 态 */
  x &= ~((1UL << 3) | (1UL << 1) | (1UL << 5));    /* MIE=0, SIE=0, SPIE=0 */
  w_mstatus(x);

  /* ---- ② mepc：mret 的跳转目标 ------------------------------------------
   * mret 一次做两件事：降权 + 跳到 mepc。所以"降权后从哪继续"就写在这。
   * 能把函数名当地址用，靠的是 Makefile 里的 -mcmodel=medany（绝对地址可寻址）。
   */
  w_mepc((uint64)main);

  /* ---- ③ satp = 0：关闭分页（Bare 模式）—— 设计笔记思考题 4 --------------
   * satp 最高位是分页使能位。写 0 = 关分页 = 虚拟地址就是物理地址。
   *
   * 为什么 lab1 选 Bare 而不是恒等映射：
   *   本实验的"地址空间"就是物理地址空间本身。引入页表要先建表、填 PTE、
   *   处理 Sv39 的三级结构 —— 那全是 lab3 的内容，而它在这里只增加
   *   启动期的失败面，不带来任何收益。
   */
  w_satp(0);

  /* ---- ④ mtvec ← mtrapvec：装陷阱向量 -----------------------------------
   * riscv.h 里【没有】w_mtvec（只有 w_stvec），所以只能内联汇编。
   *
   * 值的低 2 位是模式位：0 = Direct（所有异常都跳基址）。mtrapvec 在
   * entry.S 里带 .balign 4，保证低 2 位为 0，两者是【一对】——
   * 少了任一个，mtvec 的写入会被硬件静默丢弃，之后异常全跳地址 0。
   *
   * 【这里特意不做中断/异常委托】
   * 参考实现在这里会写 w_medeleg(0xffff) / w_mideleg(0xffff)，把所有异常
   * 交给 S 态。但 lab1 没有设置 stvec —— 一旦委托了又真出异常，CPU 会跳到
   * 未初始化的 stvec(=0)，比不委托更难查。全部留在 M 态由 mtrapvec 兜住，
   * 现象至少是"看得见的自旋"。
   */
  asm volatile("csrw mtvec, %0" : : "r"((uint64)mtrapvec));

  /* ---- ⑤ PMP：物理内存保护 —— 不配就是全黑屏（定位题）-------------------
   * 新版 QEMU 要求 S 态的访存（含取指）必须显式落在 PMP 授权区内。
   * 不配这两行，mret 降入 S 态的【第一条取指】就触发
   *   instruction access fault（cause = 1）
   * 现象：全程无任何输出，-d int 日志首行即 cause=1。
   *
   * 【逐字段解释】
   *   pmpaddr0 = 0x3fffffffffffff
   *     A 字段为 TOR(Top Of Range) 时，它表示区间的【上界】，单位是 4 字节，
   *     即实际上界 = 0x3fffffffffffff << 2 = 0xfffffffffffffc ≈ 2^56。
   *     条目 0 的下界恒为 0，所以覆盖区间 = [0, 2^56) —— 全部物理内存。
   *
   *   pmpcfg0 = 0xf = 0b0000_1111，按规范的字段表逐位解码：
   *     bit0 R = 1  允许读
   *     bit1 W = 1  允许写
   *     bit2 X = 1  允许执行   ← 少了这位就是 fault_fetch
   *     bit4:3 A = 0b01 = TOR（不是 NAPOT；NAPOT 是 0b11，对应 0x1f）
   *     bit7 L = 0  未锁定（M 态可绕过该条目）
   *
   * 注：说明书此处注释写作 "A=NAPOT"，与编码不符 —— TOR 是 0b01、NAPOT 是 0b11。
   *     两种模式配上 0x3fffffffffffff 都能覆盖全部物理内存，所以功能上无差别，
   *     但设计笔记里按规范字段表写 TOR 并附实测证据最稳妥。
   */
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  /* ---- ⑥ mret：降权并跳进 main() ----------------------------------------
   * 执行完这行，控制流【永不】回到 start()。
   * （GCC 仍会在后面补一段 ret 尾声，因为签名是 void，它不知道我们不返回；
   *   但那段代码永远执行不到。）
   */
  asm volatile("mret");
}
