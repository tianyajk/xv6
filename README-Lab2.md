# Lab2：单核陷入、中断与交互式控制台

## 构建与运行

```sh
make
make qemu
```

环境：RISC-V RV64GC 交叉 GCC/ld、GNU objcopy、QEMU RISC-V、Python 3、Perl。
自动优先查找 `riscv64-unknown-elf-objcopy`，其次查找本机的 `riscv64-elf-objcopy`；也可用 `make OBJCOPY=riscv64-unknown-elf-objcopy` 覆盖。
预置 `Makefile.upgrade` 原样引用，固定 QEMU 验收命令保持不变；QEMU 默认一个 hart。
`make -j4` 也可执行，Makefile 用 `.NOTPARALLEL` 防止预置规则反复写共享用户库目标导致竞争。

启动后首先输出原 Lab1 banner，再出现 `sh> `：

```text
OSLAB1 sid=2024302111377 mod97=0x46
sh> hi
hi: user program running, pid=2
sh> badecall
TEST-1 PASS: unknown syscalls all return -1
```

可执行的内嵌程序：`sh`、`hi`、`spin`、`badecall`、`bufstorm`、`lab2probe`。
此 Shell 不支持命令参数、管道、重定向和 cd；沿用课程预置实现。

- `Ctrl+C`：终止当前前台子进程，返回 Shell；Shell 自身不被此快捷键终止。
- `Ctrl+D`：输入 EOF；无已读数据时 read 返回 0；若本次已读数据，先返回数据，再让下一次读取得 EOF。
- `Ctrl+P`：打印 ticks、UART 接收中断、软件满缓冲丢弃次数和读等待唤醒次数；不进入输入队列。
- 当前是字符流模式，不提供行编辑。退格字节会原样交给应用；另一个行缓冲分支仅能撤回尚未提交给用户的字符。

## 测试

```sh
python3 tools/verify_lab2.py

# 或逐项运行
python3 check_expect.py 2024302111377
python3 tests/test_lab1.py
python3 tests/test_lab2.py
```

`test_lab1.py` 是升级后的回归检查：逐字节校验 banner 前缀及只出现一次，并检查随后进入 Shell；Lab2 已有正常中断/系统调用，不能再要求整个 trap 日志为空。

`test_lab2.py` 自动启动真实单核 QEMU：
- 重复 hi、非法系统调用、找不到程序的 exec；
- 官方 bufstorm 四轮受控输入及总计 19 字节；
- 整数 caller/callee 寄存器（除返回值 a0）经系统调用/定时器保存恢复、用户指针拒绝、fork 私有副本、退出码与 wait，父先退出的普通/嵌套孤儿收尾（各8轮）；
- 总共 8 个进程槽（含 Shell 与 probe），槽耗尽返回 -1，回收后可以重用；
- 用户写内核地址触发用户异常并只结束该子进程；
- 没有换行的单字节立刻读回；1024 字节分批输入顺序/数量完全一致；
- 暂停消费者时注入 96 字节，64 字节保留、32 字节软件丢弃；
- spin 中 ticks 持续增长且 UART 响应；Ctrl+C 结束后 Shell 可继续运行；
- 阻塞 read/pause 可被 Ctrl+C 中断，EOF 返回，以及额外 32 次进程创建/回收。

原始串口证据在 `tests/results/shell.log`；`tests/results/verification.txt` 保存最终构建、测试、预置文件一致性等记录。
测试结束会主动终止 QEMU；这不是内核自己退出。probe 故意制造一次用户 fault 属于隔离测试，不是非预期内核崩溃。

`lab2probe` 需要测试脚本提供输入，请使用自动测试，而不要在终端只执行后等待它自行结束。
官方 `support/inject_uart.py` 原样保留：它依赖 `fs.img` 且等待 `$ `，与本轮无文件系统和预置 `sh> ` 不一致，因此以不依赖磁盘的 `tests/test_lab2.py` 作为本轮适配驱动。

## 设计边界与提交

详见 `lab2-设计笔记.md`。本轮提前采用最小 Sv39 映射，以兼容课程预置跳板和地址 0 的用户链接布局；未实现 Lab3 个性化页分配、保护页或 Lab4 完整时间片调度。
用户程序来自内核内嵌表，不依赖磁盘与文件系统。

GitHub 交付包含相关源码、官方增量支撑代码、设计笔记和测试，不包含本地临时工作目录、主机绝对路径备份或编译产物。
验收标签为 `lab2-submit`；ZIP 使用 `git archive` 从该标签生成。
