/* lab1: emit the personalized banner once, then keep the boot hart parked. */
#include "types.h"
#include "course_sid.h"

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
  for (;;)
    asm volatile("wfi");
}
