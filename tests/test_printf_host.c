#include <stddef.h>
#include <string.h>
#include <unistd.h>

int printf(const char *fmt, ...);
void lab1_putchar(char c);

static char output[4096];
static size_t used;

void uartputc_sync(char c)
{
  if (used < sizeof(output) - 1)
    output[used++] = c;
  output[used] = 0;
}

static int check(const char *want, int count)
{
  int ok = strcmp(output, want) == 0 && (int)used == count;
  used = 0;
  output[0] = 0;
  return ok;
}

int main(void)
{
  if (printf("%d %d %d", 0, -2147483647 - 1, 2147483647) != 24 ||
      !check("0 -2147483648 2147483647", 24))
    return 1;
  if (printf("%ld 0x%x", 2024302111377L, 70U) != 18 ||
      !check("2024302111377 0x46", 18))
    return 2;
  if (printf("[%s][%c][%%][%x]", "", 'A', 0U) != 11 ||
      !check("[][A][%][0]", 11))
    return 3;
  char long_text[1025];
  memset(long_text, 'a', 1024);
  long_text[1024] = 0;
  if (printf("%s", long_text) != 1024 || !check(long_text, 1024))
    return 4;
  lab1_putchar('Z');
  if (!check("Z", 1))
    return 5;
  write(1, "PASS printf host boundaries\n", 28);
  return 0;
}
