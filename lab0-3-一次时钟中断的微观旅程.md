# Lab0 材料三：一次用户态时钟中断到 `sret` 的微观旅程

**选定现场**：单核 CPU0 正在 U 态运行普通用户进程 `P`，当前使用 `P` 的用户页表与用户栈；`stvec` 已由上次 `prepare_return` 指到 trampoline 的 `uservec`。时钟中断 `scause=0x8000000000000005`；可能存在其他 `RUNNABLE` 进程 `Q`。此参考树用 S 态 `stimecmp` 触发时钟中断，进入陷阱后执行主体在 **S 态**，不是全程 M 态。

```mermaid
sequenceDiagram
  participant U as P 用户指令/U/用户栈
  participant HW as RISC-V 硬件
  participant TV as trampoline.S:uservec
  participant KT as trap.c:usertrap/devintr
  participant SC as proc.c:sched/scheduler
  participant RT as prepare_return/userret
  U->>HW: 时钟中断到来
  HW->>TV: scause=0x800...005; sepc=受中断PC; SPP=U; SIE→SPIE; 进入S态并跳stvec
  TV->>TV: sscratch暂存a0; 保存用户通用寄存器到P.trapframe
  TV->>KT: sp=P.kstack顶部; 切内核satp; jalr usertrap
  KT->>KT: stvec=kernelvec; 保存sepc到trapframe.epc
  KT->>KT: devintr识别时钟; clockintr更新ticks/唤醒并重设stimecmp; 返回2
  KT->>SC: which_dev=2; yield: P→RUNNABLE; sched→swtch
  SC->>SC: 调度器在专用栈运行; 可选Q; 后来再选P
  SC->>KT: swtch恢复P内核context; yield返回
  KT->>RT: prepare_return: 关中断; stvec=uservec; 设置trapframe.kernel_*、SPP=U、SPIE=1、sepc
  RT->>TV: usertrap返回用户satp; userret切用户页表并恢复通用寄存器
  TV->>U: sret: PC=sepc; 恢复U态中断状态; 从原指令位置继续
```

## 逐步状态记录

| 步骤 | 动作与寄存器／内存变化 | 特权级、页表、栈、锁 |
|---|---|---|
| 0 | `P` 正在用户指令流；`satp=P.pagetable`，`stvec=uservec`，`sscratch` 可由入口使用。 | `U`、`P` 用户页表、`P` 用户栈、无自旋锁。 |
| 1 **硬件陷入** | 硬件将当前 PC 写入 `sepc`，把中断原因写入 `scause=0x8000000000000005`，`sstatus.SPP` 记录原模式 U，`SPIE←SIE`，清 `SIE`，切到 S 态并跳 `stvec`。**硬件不会自动切栈、切页表，也不会保存全部通用寄存器。** | 刚到 `uservec` 时是 `S` 态，但仍在 `P` 用户页表、原用户 `sp`；无锁。 |
| 2 **汇编保存** | `uservec` 先用 `sscratch` 暂存用户 `a0`，将 `a0=TRAPFRAME`；把 `ra/sp/gp/tp/t0...t6` 等用户通用寄存器写到 `P.trapframe`，最后将原 `a0` 存入偏移 112。 | `S`，仍使用 `P` 用户页表；trapframe 是 `P` 私有页的高地址映射，`PTE_U=0`，S 态可写；尚未用内核栈。 |
| 3 **转到 C** | 从 trapframe 取 `kernel_sp/kernel_hartid/kernel_trap/kernel_satp`；`sp=P.kstack+PGSIZE`，`tp=hartid`；`sfence.vma`，写 `satp=kernel_satp`，再 `sfence.vma`，`jalr usertrap`。 | `S`、内核页表、`P` 内核栈；无自旋锁。trampoline 在两套页表同一 VA 映射，切页表前后均可执行。 |
| 4 **分类中断** | `usertrap` 验证来自 U 态，将 `stvec` 改为 `kernelvec`，保存 `sepc` 到 `P.trapframe->epc`；`devintr` 匹配时钟中断，调用 `clockintr`。CPU0 更新 `ticks`，唤醒等待 tick 的进程，重设 `stimecmp=r_time()+1000000`，返回 `which_dev=2`。 | `S/P` 内核栈；`clockintr` 更新 `ticks` 时短暂持 `tickslock`，`wakeup(&ticks)` 可能短暂持被检查进程的 `p->lock`；返回后均已释放。 |
| 5 **主动让步** | `usertrap` 看到 `which_dev==2` 调 `yield`；`yield` 持 `P->lock` 将 `P->state=RUNNABLE`；`sched` 验证只持这把锁且中断关闭；`swtch(&P->context,&cpu->context)` 仅保存/恢复内核 callee-saved 寄存器、`ra/sp`。 | `S/P` 内核栈 → `S/CPU 调度器栈`；切换交接期间持 `P->lock`，调度器返回该 `swtch` 后释放。**用户完整现场在 trapframe，不在 context。** |
| 6 **调度决定** | `scheduler` 扫描 `proc[NPROC]`，逐个持 `p->lock` 找 `RUNNABLE`，标 `RUNNING` 并 `swtch`。可先运行 `Q`，也可稍后再次选择 `P`；不保证这次 tick 一定换到别的进程。 | `S/调度器栈`；选中进程时持它的 `p->lock`，新进程侧负责放锁。 |
| 7 **恢复 P 的内核续点** | 某次扫描再次选中 `P`，`swtch` 读回 `P->context`，从先前 `sched` 的调用点继续；`sched→yield` 返回，`yield` 释放 `P->lock`，回到 `usertrap`。 | `S/P` 内核栈、内核页表；释放后无自旋锁。 |
| 8 **准备返 U** | `prepare_return` 关中断，将 `stvec` 指回 trampoline `uservec`；将下次陷入需要的 `kernel_satp/kernel_sp/kernel_trap/kernel_hartid` 写入 `P.trapframe`；设置 `sstatus.SPP=0`、`SPIE=1`，`sepc=P.trapframe->epc`。`usertrap` 返回 `MAKE_SATP(P.pagetable)` 给 `userret`。 | `S/P` 内核栈、内核页表；无自旋锁。时钟中断不是 `ecall`，**`epc` 不加 4**。 |
| 9 **汇编返 U** | `userret` 执行 `fence.i`、切到 `P` 用户页表并刷新 TLB，按 trapframe 恢复用户通用寄存器和 `sp`，最后恢复 `a0`；执行 `sret`。硬件按 `sepc` 恢复 PC，按 `SPP=0` 进入 U 态，并由 `SPIE` 恢复中断使能。 | `S` 的 trampoline → `U/P` 用户页表与用户栈；无自旋锁；继续执行被中断位置。 |

## 现场区分与推演批注

1. **硬件与软件各保存什么？** 硬件自动处理 `sepc/scause/sstatus` 等陷阱控制状态，`trampoline.S` 负责全部通用寄存器；`swtch.S` 只负责内核上下文，因此不能拿 `context` 代替 `trapframe`。
2. **为什么不能像系统调用那样 `epc += 4`？** 时钟中断异步发生，硬件记录的是应当继续执行的用户 PC；回到硬件提供的 `sepc` 才能保持连续性。此树只在 `r_scause()==8` 的 `ecall` 分支把 PC 加 4。
3. **为什么返回前要重新写 `stvec`？** 内核期间若再次陷入应进 `kernelvec`；返 U 前改回 `uservec`，下一次用户陷入才能通过 trapframe 安全换栈和页表。
4. **为什么 `yield` 不等于“必定换人”？** `scheduler` 只按扫描顺序选 `RUNNABLE`；若没有其他候选或扫描再次到 `P`，CPU 可以继续运行同一个进程，但仍经历一次让步与上下文切换链路。

**源码定位**：`/Users/wangjane/Desktop/xv6-riscv/kernel/start.c`、`trap.c`、`trampoline.S`、`proc.c`、`proc.h`、`swtch.S`、`riscv.h`、`memlayout.h`。
