#include "lab2.h"
static void dump_level(pagetable_t pt, int level, uint64 base) {
  for (int i = 0; i < 512; i++)
    if (pt[i] & PTE_V) {
      uint64 va = base + ((uint64)i << (12 + 9 * level)), p = pt[i];
      char perm[5];
      perm[0] = (p & PTE_R) ? 'r' : '-';
      perm[1] = (p & PTE_W) ? 'w' : '-';
      perm[2] = (p & PTE_X) ? 'x' : '-';
      perm[3] = (p & PTE_U) ? 'u' : '-';
      perm[4] = 0;
      if (p & (PTE_R | PTE_W | PTE_X))
        printf("LEAF va=0x%lx pa=0x%lx perm=%s\n", va, PTE2PA(p), perm);
      else {
        printf("L%d va=0x%lx pa=0x%lx perm=%s\n", level, va, PTE2PA(p), perm);
        if (level > 0)
          dump_level((pagetable_t)PTE2PA(p), level - 1, va);
      }
    }
}
void dump_pagetable(pagetable_t pt) {
  printf("DUMP-PAGETABLE begin\n");
  dump_level(pt, 2, 0);
  printf("DUMP-PAGETABLE end\n");
}
int vm_selftest3(void) {
  uint64 before = kused3();
  void *first = kalloc3(), *second = kalloc3();
  if (!first || !second) {
    if (first)
      kfree3(first);
    if (second)
      kfree3(second);
    return -1;
  }
  int ascending = (uint64)first < (uint64)second;
  kfree3(first);
  void *reuse = kalloc3();
  ascending = ascending && reuse == first;
  if (reuse)
    kfree3(reuse);
  kfree3(second);
  if (!ascending || kused3() != before)
    return -1;
  struct trapframe *tf = kalloc3();
  if (!tf)
    return -1;
  pagetable_t pt = vm_create3(tf);
  if (!pt) {
    kfree3(tf);
    return -1;
  }
  if (vm_alloc3(pt, 0, PGSIZE, PTE_R | PTE_W | PTE_U) < 0) {
    vm_destroy3(pt);
    kfree3(tf);
    return -1;
  }
  pte_t *seed = walk3(pt, 0, 0);
  *(unsigned char *)PTE2PA(*seed) = 0x5a;
  uint64 saved = *seed, stable = kused3();
  int ok = 1;
  if (walk3(pt, MAXVA, 1) != 0 || map3(pt, 1, PTE2PA(*seed), PTE_R) != -1)
    ok = 0;
  if (map3(pt, PGSIZE, PTE2PA(*seed), PTE_W) != -1 || kused3() != stable)
    ok = 0;
  /* 跨越2MiB边界：故意让新的L0页表与第三个数据页分配失败。 */
  for (int n = 0; n < 6; n++) {
    kfail3(n);
    int rc = vm_alloc3(pt, 2UL * 1024 * 1024, 2UL * 1024 * 1024 + 3 * PGSIZE,
                       PTE_R | PTE_W | PTE_U);
    kfail3(-1);
    if (rc == 0)
      vm_unmap3(pt, 2UL * 1024 * 1024, 2UL * 1024 * 1024 + 3 * PGSIZE);
    if (((rc == 0) != (n >= 4)) || kused3() != stable || *seed != saved ||
        *(unsigned char *)PTE2PA(*seed) != 0x5a)
      ok = 0;
  }
  vm_destroy3(pt);
  kfree3(tf);
  return ok && kused3() == before ? 0 : -1;
}
