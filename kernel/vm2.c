#include "lab2.h"
static uint64 kroot[512] __attribute__((aligned(4096)));
static uint64 khigh[512] __attribute__((aligned(4096)));
static uint64 kleaf[512] __attribute__((aligned(4096)));
uint64 kernel_satp;
#define LEAF(pa, flags) ((((uint64)(pa) >> 12) << 10) | (flags) | 0xc1UL)
#define TABLE(pa) ((((uint64)(pa) >> 12) << 10) | 1)
void *kmemset(void *p, int c, uint64 n) {
  unsigned char *s = p;
  while (n--)
    *s++ = c;
  return p;
}
void *kmemcpy(void *d, const void *s, uint64 n) {
  unsigned char *a = d;
  const unsigned char *b = s;
  while (n--)
    *a++ = *b++;
  return d;
}
int kstrcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}
void vm_init(void) {
  kmemset(kroot, 0, sizeof(kroot));
  kmemset(khigh, 0, sizeof(khigh));
  kmemset(kleaf, 0, sizeof(kleaf));
  /* 1GiB 叶项：仅内核使用的物理直映和设备区域。 */
  kroot[0] = LEAF(0, 6);
  kroot[2] = LEAF(0x80000000UL, 14);
  kroot[255] = TABLE(khigh);
  khigh[511] = TABLE(kleaf);
  if ((uint64)trampoline & 4095)
    panic2("trampoline alignment");
  kleaf[511] = LEAF(trampoline, 10);
  kernel_satp = MAKE_SATP(kroot);
  w_satp(kernel_satp);
  sfence_vma();
}
void vm_process(struct process *p) {
  kmemset(p->pt, 0, sizeof(p->pt));
  p->pt[0][0] = TABLE(p->pt[1]);
  p->pt[1][0] = TABLE(p->pt[2]);
  for (int i = 0; i < USIZE / 4096; i++)
    p->pt[2][i] = LEAF(p->mem + i * 4096, 30);
  p->pt[0][255] = TABLE(p->pt[3]);
  p->pt[3][511] = TABLE(p->pt[4]);
  p->pt[4][511] = LEAF(trampoline, 10);
  /* tf 不必占满页，但起址必须页对齐，因此 process 结构单独放置 tf 页。 */
  p->pt[4][510] = LEAF(&p->tf, 6);
}
static int valid(uint64 a, int n) {
  return n >= 0 && a <= USIZE && (uint64)n <= USIZE - a;
}
int copyin2(void *d, uint64 s, int n) {
  if (!valid(s, n))
    return -1;
  kmemcpy(d, current->mem + s, n);
  return 0;
}
int copyout2(uint64 d, const void *s, int n) {
  if (!valid(d, n))
    return -1;
  kmemcpy(current->mem + d, s, n);
  return 0;
}
int copystr2(char *d, uint64 s, int n) {
  for (int i = 0; i < n; i++) {
    if (copyin2(d + i, s + i, 1) < 0)
      return -1;
    if (!d[i])
      return 0;
  }
  return -1;
}
