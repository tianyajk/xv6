/* 保留 Lab1 精确 banner；随后进入 Lab2 的单核交互系统。 */
#include "types.h"
#include "course_sid.h"

void lab2_main(void) __attribute__((noreturn));

int printf(const char *format, ...);

void
main(void)
{
#if LAB1_BANNER_PROTOCOL == 0
  printf("OSLAB1 sid=%ld mod97=0x%x\n", (long)COURSE_SID,
         (unsigned int)(COURSE_SID % 97UL));
#else
#error "This course baseline expects LAB1_BANNER_PROTOCOL == 0"
#endif
  lab2_main();
}
