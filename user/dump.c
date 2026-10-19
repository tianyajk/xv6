/* 现场重放 Shell(pid1) 的真实页表；不增加课程预置系统调用桩。 */
#include "kernel/types.h"
#include "kernel/course_sid.h"
#define __USER_VM3__
#include "kernel/lab3.h"
#include "user/user.h"
int main(void) {
  register uint64 a0 asm("a0") = 1;
  register uint64 a7 asm("a7") = SYS_dump3;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  if ((long)a0 < 0)
    printf("dump: process not found\n");
  exit((long)a0 < 0);
}
