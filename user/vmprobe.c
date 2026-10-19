/* Lab3 自编用户测试：权限、跨页、安全拷贝、分配失败和事务式exec。 */
#include "kernel/types.h"
#include "kernel/course_sid.h"
#define __USER_VM3__
#include "kernel/lab3.h"
#include "user/user.h"
static volatile int marker = 123;
static long call3(long n, uint64 x) {
  register uint64 a0 asm("a0") = x;
  register uint64 a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return (long)a0;
}
static void check(int ok, const char *s) {
  if (!ok) {
    printf("VM-PROBE FAIL: %s\n", s);
    exit(1);
  }
}
static struct vmstats3 stats(void) {
  struct vmstats3 s;
  check(call3(SYS_vmstats3, (uint64)&s) == 0, "stats");
  return s;
}
int main(void) {
  struct vmstats3 base = stats();
  check(base.limit + LAB3_MEMCAP_DELTA == base.total, "physical quota");
  check(base.guard_end - base.guard_begin == LAB3_GUARD_PAGES * 4096UL, "guard count");
  check(base.allocs - base.frees == base.used, "accounting");
  for (int i = 0; i < 4; i++)
    check(call3(SYS_vmtest3, 0) == 0, "kernel allocation rollback");
  check(stats().used == base.used, "kernel selftest leak");
  char *p = sbrk(3 * 4096);
  check(p != (char *)-1, "heap grow");
  memcpy(p + 4093, "CROSS!\n", 7);
  check(write(1, p + 4093, 7) == 7, "cross-page write");
  printf("VM copyout ready\n");
  check(read(0, p + 4093, 7) == 7, "cross-page copyout");
  check(memcmp(p + 4093, "ABCDEFG", 7) == 0, "cross-page copyout content");
  p[3 * 4096 - 2] = 'Q';
  p[3 * 4096 - 1] = 'R';
  check(read(0, p + 3 * 4096 - 2, 4) == -1 && p[3 * 4096 - 2] == 'Q' &&
            p[3 * 4096 - 1] == 'R',
        "copyout preflight atomicity");
  check(read(0, (void *)0, 1) == -1, "copyout rejects RX text");
  check(write(1, (void *)base.guard_begin, 1) == -1, "copyin rejects guard");
  check(write(1, (void *)0x80000000UL, 1) == -1, "kernel pointer");
  check(write(1, (void *)-4UL, 16) == -1, "pointer overflow");
  check(write(1, p + 3 * 4096 - 2, 4) == -1, "partial invalid range");
  memcpy(p + 4093, "no-such-image", 14);
  check(exec(p + 4093, 0) == -1, "cross-page string");
  check(sbrk(-3 * 4096) != (char *)-1, "heap shrink");
  check(write(1, p, 1) == -1, "unmapped shrink page");
  check(stats().used == base.used, "grow shrink leak");
  int child = fork();
  check(child >= 0, "guard child");
  if (child == 0) {
    *(volatile char *)(base.guard_end - 1) = 1;
    exit(0);
  }
  int code = 0;
  check(wait(&code) == child && code == -1, "guard fault containment");
  check(stats().used == base.used, "guard child leak");
  for (int n = 0; n < 24; n++) {
    uint64 before = stats().used;
    call3(SYS_vmfail3, n);
    int c = fork();
    if (c == 0) {
      call3(SYS_vmfail3, -1UL);
      exit(77);
    }
    call3(SYS_vmfail3, -1UL);
    if (c >= 0) {
      int status = 0;
      check(wait(&status) == c && status == 77, "injected fork status");
    }
    check(marker == 123 && stats().used == before, "fork failure rollback");
  }
  int failed_exec = 0, successful_exec = 0;
  for (int n = 0; n < 17; n++) {
    uint64 before = stats().used;
    int c = fork();
    check(c >= 0, "exec injection child");
    if (c == 0) {
      uint64 own = stats().used;
      call3(SYS_vmfail3, n);
      int rc = exec("hi", 0); /* 成功则不返回，由原版 hi 正常退出。 */
      call3(SYS_vmfail3, -1UL);
      check(rc == -1 && marker == 123 && stats().used == own, "exec failure atomicity");
      exit(77);
    }
    int status = -1;
    check(wait(&status) == c, "exec injection wait");
    call3(SYS_vmfail3, -1UL);
    if (status == 77)
      failed_exec++;
    else {
      check(status == 0, "exec success status");
      successful_exec++;
    }
    check(stats().used == before, "exec transaction cleanup");
  }
  check(failed_exec > 0 && successful_exec > 0, "exec failure/success coverage");
  check(stats().used == base.used, "final page leak");
  printf("VM-PROBE PASS: cross-page, permissions, guard, shrink, quota, fork/exec "
         "rollback\n");
  exit(0);
}
