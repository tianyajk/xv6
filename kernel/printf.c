/* lab1: minimal formatted output. UART transmission itself stays in console.c. */
#include <stdarg.h>
#include "types.h"
#include "course_sid.h"

void uartputc_sync(char c);

#define THROTTLE_INTERVAL (16UL + (COURSE_SID % 16UL))
#define THROTTLE_NOPS 256U

/* Nonzero initializer puts this state in .data rather than relying on BSS clear. */
static unsigned long until_throttle = THROTTLE_INTERVAL;

void
lab1_putchar(char c)
{
  uartputc_sync(c);
  if (--until_throttle == 0) {
    for (volatile unsigned int i = 0; i < THROTTLE_NOPS; i++)
      asm volatile("nop");
    until_throttle = THROTTLE_INTERVAL;
  }
}

static void
emit_char(char c, int *count)
{
  lab1_putchar(c);
  ++*count;
}

static void
emit_unsigned(uint64 value, unsigned int base, int *count)
{
  char digits[32];
  static const char hex[] = "0123456789abcdef";
  unsigned int n = 0;
  do {
    digits[n++] = hex[value % base];
    value /= base;
  } while (value != 0);
  while (n != 0)
    emit_char(digits[--n], count);
}

static void
emit_signed(long value, int *count)
{
  uint64 magnitude;
  if (value < 0) {
    emit_char('-', count);
    magnitude = (uint64)(-(value + 1)) + 1;
  } else {
    magnitude = (uint64)value;
  }
  emit_unsigned(magnitude, 10, count);
}

/* Supports %c, %s, %d, %u, %x, %ld, %lu, %lx, and %%. */
int
printf(const char *format, ...)
{
  va_list args;
  int count = 0;
  va_start(args, format);
  for (; *format != 0; ++format) {
    if (*format != '%') {
      emit_char(*format, &count);
      continue;
    }
    ++format;
    if (*format == 0) {
      emit_char('%', &count);
      break;
    }
    int is_long = 0;
    if (*format == 'l') {
      is_long = 1;
      ++format;
      if (*format == 0) {
        emit_char('%', &count);
        emit_char('l', &count);
        break;
      }
    }
    switch (*format) {
    case 'c':
      emit_char((char)va_arg(args, int), &count);
      break;
    case 's': {
      const char *s = va_arg(args, const char *);
      if (s == 0)
        s = "(null)";
      while (*s != 0)
        emit_char(*s++, &count);
      break;
    }
    case 'd':
      emit_signed(is_long ? va_arg(args, long) : (long)va_arg(args, int),
                  &count);
      break;
    case 'u':
      emit_unsigned(is_long ? va_arg(args, unsigned long)
                            : (uint64)va_arg(args, unsigned int),
                    10, &count);
      break;
    case 'x':
      emit_unsigned(is_long ? va_arg(args, unsigned long)
                            : (uint64)va_arg(args, unsigned int),
                    16, &count);
      break;
    case '%':
      emit_char('%', &count);
      break;
    default:
      emit_char('%', &count);
      if (is_long)
        emit_char('l', &count);
      emit_char(*format, &count);
      break;
    }
  }
  va_end(args);
  return count;
}
