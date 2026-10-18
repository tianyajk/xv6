/* 自编验收程序：只使用课程公开 ABI，不修改预置用户程序。 */
#include "kernel/course_sid.h"
#include "kernel/types.h"
#include "user/user.h"

static volatile int marker = 7;
static void require(int ok, const char *s) {
  if (!ok) {
    printf("PROBE FAIL: %s\n", s);
    exit(1);
  }
}
int probe_registers(void);
asm(".text\n"
    ".balign 4\n"
    ".global probe_registers\n"
    "probe_registers:\n"
    "addi sp, sp, -128\n"
    "sd ra, 96(sp)\n"
    "sd gp, 104(sp)\n"
    "sd tp, 112(sp)\n"
    "sd s0, 0(sp)\n"
    "sd s1, 8(sp)\n"
    "sd s2, 16(sp)\n"
    "sd s3, 24(sp)\n"
    "sd s4, 32(sp)\n"
    "sd s5, 40(sp)\n"
    "sd s6, 48(sp)\n"
    "sd s7, 56(sp)\n"
    "sd s8, 64(sp)\n"
    "sd s9, 72(sp)\n"
    "sd s10, 80(sp)\n"
    "sd s11, 88(sp)\n"
    "li ra, 100\n"
    "li gp, 101\n"
    "li tp, 102\n"
    "li t0, 103\n"
    "li t1, 104\n"
    "li t2, 105\n"
    "li t3, 106\n"
    "li t4, 107\n"
    "li t5, 108\n"
    "li t6, 109\n"
    "li s0, 110\n"
    "li s1, 111\n"
    "li s2, 112\n"
    "li s3, 113\n"
    "li s4, 114\n"
    "li s5, 115\n"
    "li s6, 116\n"
    "li s7, 117\n"
    "li s8, 118\n"
    "li s9, 119\n"
    "li s10, 120\n"
    "li s11, 121\n"
    "li a1, 122\n"
    "li a2, 123\n"
    "li a3, 124\n"
    "li a4, 125\n"
    "li a5, 126\n"
    "li a6, 127\n"
    "li a7, 14\n"
    "ecall\n"
    "li a0, 2000000\n"
    "1:\n"
    "addi a0, a0, -1\n"
    "bnez a0, 1b\n"
    "li a0, 100\n"
    "bne ra, a0, 2f\n"
    "li a0, 101\n"
    "bne gp, a0, 2f\n"
    "li a0, 102\n"
    "bne tp, a0, 2f\n"
    "li a0, 103\n"
    "bne t0, a0, 2f\n"
    "li a0, 104\n"
    "bne t1, a0, 2f\n"
    "li a0, 105\n"
    "bne t2, a0, 2f\n"
    "li a0, 106\n"
    "bne t3, a0, 2f\n"
    "li a0, 107\n"
    "bne t4, a0, 2f\n"
    "li a0, 108\n"
    "bne t5, a0, 2f\n"
    "li a0, 109\n"
    "bne t6, a0, 2f\n"
    "li a0, 110\n"
    "bne s0, a0, 2f\n"
    "li a0, 111\n"
    "bne s1, a0, 2f\n"
    "li a0, 112\n"
    "bne s2, a0, 2f\n"
    "li a0, 113\n"
    "bne s3, a0, 2f\n"
    "li a0, 114\n"
    "bne s4, a0, 2f\n"
    "li a0, 115\n"
    "bne s5, a0, 2f\n"
    "li a0, 116\n"
    "bne s6, a0, 2f\n"
    "li a0, 117\n"
    "bne s7, a0, 2f\n"
    "li a0, 118\n"
    "bne s8, a0, 2f\n"
    "li a0, 119\n"
    "bne s9, a0, 2f\n"
    "li a0, 120\n"
    "bne s10, a0, 2f\n"
    "li a0, 121\n"
    "bne s11, a0, 2f\n"
    "li a0, 122\n"
    "bne a1, a0, 2f\n"
    "li a0, 123\n"
    "bne a2, a0, 2f\n"
    "li a0, 124\n"
    "bne a3, a0, 2f\n"
    "li a0, 125\n"
    "bne a4, a0, 2f\n"
    "li a0, 126\n"
    "bne a5, a0, 2f\n"
    "li a0, 127\n"
    "bne a6, a0, 2f\n"
    "li a0, 14\n"
    "bne a7, a0, 2f\n"
    "li a0, 0\n"
    "j 3f\n"
    "2:\n"
    "li a0, -1\n"
    "3:\n"
    "ld s0, 0(sp)\n"
    "ld s1, 8(sp)\n"
    "ld s2, 16(sp)\n"
    "ld s3, 24(sp)\n"
    "ld s4, 32(sp)\n"
    "ld s5, 40(sp)\n"
    "ld s6, 48(sp)\n"
    "ld s7, 56(sp)\n"
    "ld s8, 64(sp)\n"
    "ld s9, 72(sp)\n"
    "ld s10, 80(sp)\n"
    "ld s11, 88(sp)\n"
    "ld ra, 96(sp)\n"
    "ld gp, 104(sp)\n"
    "ld tp, 112(sp)\n"
    "addi sp, sp, 128\n"
    "ret\n");
int main(void) {
  require(probe_registers() == 0, "register restore");
  require(write(1, (void *)0x80000000UL, 1) == -1, "kernel pointer");
  require(read(0, (void *)0x3fffffe000UL, 1) == -1, "trapframe pointer");
  require(write(1, (void *)-1UL, 8) == -1, "overflow pointer");
  require(read(0, (void *)0, -1) == -1, "negative count");
  require(read(0, (void *)0, 0) == 0, "zero read");
  require(wait(0) == -1, "wait no child");
  require(exec("no-such-image", 0) == -1, "exec missing");
  int pids[6];
  for (int i = 0; i < 6; i++) {
    int p = fork();
    require(p >= 0, "fork slot");
    if (p == 0) {
      marker = 99;
      exit(40 + i);
    }
    pids[i] = p;
  }
  require(fork() == -1, "full process table");
  for (int i = 0; i < 6; i++) {
    int status = -1;
    require(wait(&status) == pids[i], "wait pid");
    require(status == 40 + i, "exit status");
  }
  require(marker == 7, "private memory");
  for (int round = 0; round < 8; round++) {
    int parent = fork();
    require(parent >= 0, "orphan parent fork");
    if (parent == 0) {
      int orphan = fork();
      require(orphan >= 0, "orphan child fork");
      if (orphan == 0) {
        printf("PROBE orphan child PASS\n");
        exit(77);
      }
      exit(55);
    }
    int code = -1;
    require(wait(&code) == parent && code == 55, "orphan reaper status");
  }
  for (int round = 0; round < 8; round++) {
    int parent = fork();
    require(parent >= 0, "nested parent fork");
    if (parent == 0) {
      int orphan = fork();
      require(orphan >= 0, "nested orphan fork");
      if (orphan == 0) {
        int grandchild = fork();
        require(grandchild >= 0, "nested grandchild fork");
        if (grandchild == 0)
          exit(88);
        int code = -1;
        require(wait(&code) == grandchild && code == 88, "nested orphan wait");
        printf("PROBE nested orphan PASS\n");
        exit(66);
      }
      exit(55);
    }
    int code = -1;
    require(wait(&code) == parent && code == 55, "nested parent status");
  }
  int child = fork();
  require(child >= 0, "reused slot");
  if (child == 0) {
    *(volatile int *)0x80000000UL = 123;
    exit(9);
  }
  int status = 0;
  require(wait(&status) == child && status == -1, "user fault isolation");
  uint begin = uptime();
  pause(3);
  require(uptime() >= begin + 3, "timer advances");
  printf("PROBE core PASS\n");

  /* 阻塞 read 的逐字节试验：字符流必须在没有换行时返回。 */
  printf("PROBE char ready\n");
  char c = 0;
  require(read(0, &c, 1) == 1 && c == 'Z', "character stream");
  printf("PROBE char PASS\n");

  printf("PROBE stream ready\n");
  int total = 0;
  while (total < 1024) {
    char batch[32];
    int n = read(0, batch, 32 - total % 32);
    require(n > 0, "stream read");
    for (int i = 0; i < n; i++)
      require(batch[i] == 'a' + (total + i) % 26, "stream order");
    total += n;
    if (total % 32 == 0)
      printf("PROBE stream bytes=%d\n", total);
  }
  printf("PROBE stream PASS bytes=1024\n");

  /* 给输入留出积累时间，精确验证满缓冲保留最早的 64 字节。 */
  printf("PROBE overflow ready\n");
  pause(100);
  char buf[LAB2_BUF_SIZE];
  int n = read(0, buf, sizeof(buf));
  require(n == LAB2_BUF_SIZE, "buffer capacity");
  for (int i = 0; i < n; i++)
    require(buf[i] == 'x', "buffer FIFO data");
  printf("PROBE overflow PASS capacity=%d\n", n);
  printf("PROBE all PASS\n");
  printf("PROBE interruptible pause ready\n");
  pause(100000);
  printf("PROBE FAIL: pause returned early\n");
  exit(1);
}
