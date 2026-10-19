# 编译产物集中存放在 build/；源码目录仅保留课程兼容链接。
.DEFAULT_GOAL := all
CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
QEMU = qemu-system-riscv64
OBJCOPY ?= $(shell command -v riscv64-unknown-elf-objcopy 2>/dev/null || command -v riscv64-elf-objcopy 2>/dev/null)
CFLAGS = -Wall -Werror -O -std=gnu99 -mcmodel=medany -ffreestanding \
         -nostdlib -fno-common -ggdb -march=rv64gc -fno-stack-protector -fno-pie

# 课程升级片段保持原文件不变；新构建目标不使用其中的旧输出路径。
include Makefile.upgrade
UPROGS += badecall bufstorm lab2probe badaccess memcap vmprobe dump

BUILD := build
GEN := $(BUILD)/generated
KERNEL_C := start console printf main kalloc3 vmcheck3 vm2 proc2 trap2 console2 syscall2
KERNEL_OBJS := $(BUILD)/kernel/entry.o $(addprefix $(BUILD)/kernel/,$(KERNEL_C:=.o)) \
               $(BUILD)/kernel/vectors2.o $(BUILD)/kernel/trampoline.o $(BUILD)/kernel/userimg.o
USER_LIBS := $(addprefix $(BUILD)/user/,ulib.o printf.o usys.o)
USER_BINS := $(addprefix $(BUILD)/user/,$(UPROGS:=.bin))
KHEADERS := $(wildcard kernel/*.h)
UHEADERS := $(wildcard user/*.h) $(KHEADERS)

.PHONY: all clean clean-extra clean-build
.NOTPARALLEL:
.SECONDARY:
all: kernel/kernel

# 固定验收命令仍使用 kernel/kernel，实际 ELF 只存放于 build/。
kernel/kernel: $(BUILD)/kernel/kernel
	ln -sfn ../build/kernel/kernel $@

$(BUILD)/kernel/kernel: $(KERNEL_OBJS) kernel/kernel.ld Makefile
	$(LD) -T kernel/kernel.ld -o $@ $(KERNEL_OBJS)

$(BUILD)/kernel/%.o: kernel/%.c $(KHEADERS) Makefile Makefile.upgrade
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -I$(GEN) -Ikernel -c $< -o $@

$(BUILD)/kernel/%.o: kernel/%.S $(KHEADERS) Makefile Makefile.upgrade
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -Ikernel -c $< -o $@

$(BUILD)/kernel/trampoline.o: kernel/trampoline.S $(KHEADERS) Makefile Makefile.upgrade
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -Ikernel -c $< -o $@
	$(OBJCOPY) --set-section-flags trampsec=alloc,load,readonly,code --set-section-alignment trampsec=4096 $@

$(BUILD)/kernel/userimg.o: $(GEN)/userimg.S Makefile
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -Ikernel -c $< -o $@
	$(OBJCOPY) --set-section-alignment .rodata=8 $@

$(BUILD)/kernel/proc2.o: $(GEN)/userentry.h

$(GEN)/usys.S: user/usys.pl kernel/syscall.h
	mkdir -p $(@D)
	perl $< > $@.tmp
	mv $@.tmp $@

$(BUILD)/user/%.o: user/%.c $(UHEADERS) Makefile Makefile.upgrade
	mkdir -p $(@D)
	$(CC) $(UFLAGS) -MMD -MP -Iuser -c $< -o $@

$(BUILD)/user/usys.o: $(GEN)/usys.S kernel/syscall.h Makefile Makefile.upgrade
	mkdir -p $(@D)
	$(CC) $(UFLAGS) -MMD -MP -Iuser -c $< -o $@

$(BUILD)/user/%.elf: $(BUILD)/user/%.o $(USER_LIBS) user/lab3.ld
	$(LD) -T user/lab3.ld -o $@ $(filter %.o,$^)

$(BUILD)/user/%.bin: $(BUILD)/user/%.elf
	$(OBJCOPY) -O binary $< $@

$(GEN)/userimg.S: $(USER_BINS) tools/embed_users.py Makefile
	python3 tools/embed_users.py --output $@ --bin-dir $(BUILD)/user $(UPROGS)

$(GEN)/userentry.h: $(USER_BINS) tools/user_metadata.py Makefile
	python3 tools/user_metadata.py --input-dir $(BUILD)/user --output $@ $(UPROGS)

# 只清理确定的生成目录；固定的 clean/qemu 规则原样保留。
clean: clean-build
clean-extra: clean
clean-build:
	python3 -c "from pathlib import Path; import shutil; p=Path('build'); shutil.rmtree(p) if p.exists() else None; Path('kernel/userentry.h').unlink(missing_ok=True)"

-include $(wildcard $(BUILD)/kernel/*.d $(BUILD)/user/*.d)

# 验收环境固定(排雷环节禁止改动本行以下内容)
qemu: kernel/kernel
	$(QEMU) -machine virt -bios none -kernel kernel/kernel -nographic

clean:
	rm -f kernel/*.o kernel/kernel
