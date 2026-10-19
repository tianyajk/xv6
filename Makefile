# lab1 骨架 Makefile(课程提供;新增源文件时把它加进 OBJS)
CC   = riscv64-unknown-elf-gcc
LD   = riscv64-unknown-elf-ld
CFLAGS = -Wall -Werror -O -std=gnu99 -mcmodel=medany -ffreestanding \
         -nostdlib -fno-common -ggdb -march=rv64gc -fno-stack-protector -fno-pie
QEMU = qemu-system-riscv64

OBJS = kernel/entry.o kernel/start.o kernel/console.o kernel/printf.o kernel/main.o kernel/kalloc3.o kernel/vmcheck3.o kernel/vm2.o kernel/proc2.o kernel/trap2.o kernel/console2.o kernel/syscall2.o kernel/vectors2.o kernel/trampoline.o

kernel/kernel: $(OBJS) kernel/kernel.ld
	$(LD) -T kernel/kernel.ld -o $@ $(OBJS)

%.o: %.c kernel/riscv.h kernel/course_sid.h
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<

kernel/vm2.o kernel/proc2.o kernel/trap2.o kernel/console2.o kernel/syscall2.o: kernel/lab2.h

# 内嵌表首项也保持课程 ABI 的8字节对齐，仅处理派生产物。
kernel/userimg.o: kernel/userimg.S
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<
	$(OBJCOPY) --set-section-alignment .rodata=8 $@

# 预置汇编未声明 trampsec 的装载属性；只修正派生目标文件，不改源码。
kernel/trampoline.o: kernel/trampoline.S kernel/riscv.h kernel/memlayout.h
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<
	$(OBJCOPY) --set-section-flags trampsec=alloc,load,readonly,code --set-section-alignment trampsec=4096 $@

.NOTPARALLEL:

# Lab2：接入课程预置规则，保持原 kernel 目标为默认目标。
OBJCOPY ?= $(shell command -v riscv64-unknown-elf-objcopy 2>/dev/null || command -v riscv64-elf-objcopy 2>/dev/null)
include Makefile.upgrade
OBJS += kernel/userimg.o
kernel/kernel: kernel/userimg.o
UPROGS += badecall bufstorm lab2probe badaccess memcap vmprobe dump
USER_BINS := $(addprefix user-flat/,$(UPROGS:=.bin))
kernel/userimg.S: $(USER_BINS)
kernel/userentry.h: $(USER_BINS) tools/user_metadata.py
	python3 tools/user_metadata.py $(UPROGS)
kernel/proc2.o: kernel/userentry.h
# 补齐预置片段的头文件/链接脚本依赖；内核与用户 ABI 同步重建。
kernel/syscall2.o: kernel/syscall.h
$(filter %.o,$(OBJS)): kernel/types.h kernel/memlayout.h
$(USER_BINS): kernel/types.h kernel/syscall.h kernel/stat.h kernel/fcntl.h kernel/param.h kernel/vm.h kernel/course_sid.h user/user.ld
user/usys.S: kernel/syscall.h

# Lab3 只覆盖派生用户构建规则；课程链接脚本与升级片段保持原样。
$(USER_BINS): user-flat/%.bin: user-flat/%.elf
	$(OBJCOPY) -O binary $< $@
user-flat/%.elf: user-flat/%.o user-flat/ulib.o user-flat/printf.o user-flat/usys.o user/lab3.ld
	$(LD) -T user/lab3.ld -o $@ $(filter %.o,$^)
user-flat/%.o: user/%.c user/user.h kernel/types.h kernel/lab3.h kernel/course_sid.h
	mkdir -p user-flat
	$(CC) $(UFLAGS) -Iuser -c $< -o $@
user-flat/usys.o: user/usys.S
	mkdir -p user-flat
	$(CC) $(UFLAGS) -Iuser -c $< -o $@
$(filter %.o,$(OBJS)): kernel/lab2.h kernel/lab3.h

# 验收环境固定(排雷环节禁止改动本行以下内容)
qemu: kernel/kernel
	$(QEMU) -machine virt -bios none -kernel kernel/kernel -nographic

clean:
	rm -f kernel/*.o kernel/kernel
