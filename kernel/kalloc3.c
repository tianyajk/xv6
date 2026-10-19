#include "lab2.h"
extern char kernel_end[];
#define MAXPAGES ((PHYSTOP - KERNBASE) / PGSIZE)
static unsigned char busy[MAXPAGES];
static uint64 base, total, limit, used, hint, allocations, releases;
static long fail_after;
void kinit3(void) {
  base = PGROUNDUP((uint64)kernel_end);
  total = (PHYSTOP - base) / PGSIZE;
  limit = total > LAB3_MEMCAP_DELTA ? total - LAB3_MEMCAP_DELTA : 0;
  used = hint = allocations = releases = 0;
  fail_after = -1;
  kmemset(busy, 0, sizeof(busy));
  if (LAB3_ALLOC_ORDER != 0)
    panic2("this profile requires ascending page order");
}
void kfail3(long n) { fail_after = n; }
void *kalloc3(void) {
  if (fail_after == 0 || used == limit)
    return 0;
  while (hint < limit && busy[hint])
    hint++;
  if (hint == limit)
    return 0;
  uint64 i = hint++;
  busy[i] = 1;
  used++;
  allocations++;
  if (fail_after > 0)
    fail_after--;
  void *p = (void *)(base + i * PGSIZE);
  kmemset(p, 0, PGSIZE);
  return p;
}
void kfree3(void *p) {
  uint64 a = (uint64)p;
  if (a < base || a >= base + limit * PGSIZE || (a & (PGSIZE - 1)))
    panic2("kfree range");
  uint64 i = (a - base) / PGSIZE;
  if (!busy[i])
    panic2("double free");
  busy[i] = 0;
  if (i < hint)
    hint = i;
  used--;
  releases++;
}
uint64 kused3(void) { return used; }
void kstats3(struct vmstats3 *s) {
  s->total = total;
  s->limit = limit;
  s->used = used;
  s->free = limit - used;
  s->allocs = allocations;
  s->frees = releases;
}
