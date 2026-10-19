#include "lab2.h"
uint64 kernel_satp;
void *kmemset(void *p, int c, uint64 n) {
  unsigned char *q = p;
  while (n--)
    *q++ = c;
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
static int leaf(pte_t p) { return (p & (PTE_R | PTE_W | PTE_X)) != 0; }
/* 单次 walk 创建的中间页也按事务回滚，不留下空目录页。 */
pte_t *walk3(pagetable_t pt, uint64 va, int alloc) {
  if (va >= MAXVA)
    return 0;
  pte_t *created[2];
  void *pages[2];
  int n = 0;
  for (int level = 2; level > 0; level--) {
    pte_t *p = &pt[PX(level, va)];
    if (*p & PTE_V) {
      if (leaf(*p))
        goto bad;
      pt = (pagetable_t)PTE2PA(*p);
    } else {
      if (!alloc)
        goto bad;
      pagetable_t next = kalloc3();
      if (!next)
        goto bad;
      *p = PA2PTE(next) | PTE_V;
      created[n] = p;
      pages[n++] = next;
      pt = next;
    }
  }
  return &pt[PX(0, va)];
bad:
  while (n) {
    n--;
    *created[n] = 0;
    kfree3(pages[n]);
  }
  return 0;
}
int map3(pagetable_t pt, uint64 va, uint64 pa, int flags) {
  if (!(flags & (PTE_R | PTE_W | PTE_X)) || ((flags & PTE_W) && !(flags & PTE_R)) ||
      (flags & ~0x3ff))
    return -1;
  if ((va & (PGSIZE - 1)) || (pa & (PGSIZE - 1)) || va >= MAXVA)
    return -1;
  pte_t *p = walk3(pt, va, 1);
  if (!p || (*p & PTE_V))
    return -1;
  *p = PA2PTE(pa) | flags | PTE_V | PTE_A | ((flags & PTE_W) ? PTE_D : 0);
  return 0;
}
static int prune(pagetable_t pt, int level) {
  int empty = 1;
  for (int i = 0; i < 512; i++)
    if (pt[i] & PTE_V) {
      if (!leaf(pt[i]) && level > 0) {
        pagetable_t next = (pagetable_t)PTE2PA(pt[i]);
        if (prune(next, level - 1)) {
          pt[i] = 0;
          kfree3(next);
          continue;
        }
      }
      empty = 0;
    }
  return empty;
}
void vm_unmap3(pagetable_t pt, uint64 start, uint64 end) {
  for (uint64 a = PGROUNDUP(start); a < end; a += PGSIZE) {
    pte_t *p = walk3(pt, a, 0);
    if (p && (*p & PTE_V)) {
      void *page = (void *)PTE2PA(*p);
      *p = 0; /* 先取消映射，再归还页面。 */
      kfree3(page);
    }
  }
  prune(pt, 2);
  sfence_vma();
}
int vm_alloc3(pagetable_t pt, uint64 start, uint64 end, int flags) {
  uint64 a = PGROUNDUP(start), begin = a;
  if (start > end || end > USTACK_TOP)
    return -1;
  for (; a < end; a += PGSIZE) {
    void *page = kalloc3();
    if (!page)
      goto rollback;
    if (map3(pt, a, (uint64)page, flags) < 0) {
      kfree3(page);
      goto rollback;
    }
  }
  return 0;
rollback:
  vm_unmap3(pt, begin, a);
  return -1;
}
static void destroy(pagetable_t pt, int level, uint64 base) {
  for (int i = 0; i < 512; i++)
    if (pt[i] & PTE_V) {
      pte_t old = pt[i];
      uint64 va = base + ((uint64)i << (12 + 9 * level));
      pt[i] = 0;
      if (leaf(old)) {
        /* 跳板和 trapframe 是外部所有者；其余用户区含 guard 由此释放。 */
        if (va < USTACK_TOP)
          kfree3((void *)PTE2PA(old));
      } else if (level > 0)
        destroy((pagetable_t)PTE2PA(old), level - 1, va);
    }
  kfree3(pt);
}
void vm_destroy3(pagetable_t pt) {
  if (pt)
    destroy(pt, 2, 0);
}
pagetable_t vm_create3(struct trapframe *tf) {
  pagetable_t pt = kalloc3();
  if (!pt)
    return 0;
  if (map3(pt, TRAMPOLINE, (uint64)trampoline, PTE_R | PTE_X) < 0 ||
      map3(pt, TRAPFRAME, (uint64)tf, PTE_R | PTE_W) < 0) {
    vm_destroy3(pt);
    return 0;
  }
  return pt;
}
int vm_process(struct process *p) {
  p->tf = kalloc3();
  if (!p->tf)
    return -1;
  p->pagetable = vm_create3(p->tf);
  if (!p->pagetable) {
    kfree3(p->tf);
    p->tf = 0;
    return -1;
  }
  return 0;
}
static int clone_level(pagetable_t src, pagetable_t dst, int level, uint64 base) {
  for (int i = 0; i < 512; i++)
    if (src[i] & PTE_V) {
      uint64 va = base + ((uint64)i << (12 + 9 * level));
      if (va >= USTACK_TOP)
        continue;
      if (!leaf(src[i])) {
        if (level == 0 ||
            clone_level((pagetable_t)PTE2PA(src[i]), dst, level - 1, va) < 0)
          return -1;
      } else {
        void *p = kalloc3();
        if (!p)
          return -1;
        kmemcpy(p, (void *)PTE2PA(src[i]), PGSIZE);
        if (map3(dst, va, (uint64)p, PTE_FLAGS(src[i]) & ~PTE_V) < 0) {
          kfree3(p);
          return -1;
        }
      }
    }
  return 0;
}
int vm_clone3(pagetable_t src, pagetable_t dst) { return clone_level(src, dst, 2, 0); }
int vm_check3(pagetable_t pt, uint64 va, uint64 n, int write) {
  if (va >= USTACK_TOP || n > USTACK_TOP - va)
    return -1;
  for (uint64 a = va; n;) {
    pte_t *p = walk3(pt, a, 0);
    if (!p || !(*p & PTE_V) || !(*p & PTE_U) || !(*p & (write ? PTE_W : PTE_R)))
      return -1;
    uint64 chunk = PGSIZE - (a & (PGSIZE - 1));
    if (chunk > n)
      chunk = n;
    a += chunk;
    n -= chunk;
  }
  return 0;
}
/* out=1: 内核src→用户va；out=0: 用户va→内核dst。先校验全范围避免半拷贝。 */
int vm_copy3(pagetable_t pt, void *kernel, uint64 va, uint64 n, int out) {
  if (vm_check3(pt, va, n, out) < 0)
    return -1;
  unsigned char *k = kernel;
  while (n) {
    pte_t *p = walk3(pt, va, 0);
    uint64 off = va & (PGSIZE - 1), chunk = PGSIZE - off;
    if (chunk > n)
      chunk = n;
    void *pa = (void *)(PTE2PA(*p) + off);
    if (out)
      kmemcpy(pa, k, chunk);
    else
      kmemcpy(k, pa, chunk);
    va += chunk;
    k += chunk;
    n -= chunk;
  }
  return 0;
}
int vm_load3(pagetable_t pt, uint64 va, const void *src, uint64 n) {
  const unsigned char *s = src;
  while (n) {
    pte_t *p = walk3(pt, va, 0);
    if (!p || !(*p & PTE_V))
      return -1;
    uint64 off = va & (PGSIZE - 1), chunk = PGSIZE - off;
    if (chunk > n)
      chunk = n;
    kmemcpy((void *)(PTE2PA(*p) + off), s, chunk);
    s += chunk;
    va += chunk;
    n -= chunk;
  }
  return 0;
}
int copyin2(void *d, uint64 s, int n) {
  return n < 0 ? -1 : vm_copy3(current->pagetable, d, s, n, 0);
}
int copyout2(uint64 d, const void *s, int n) {
  return n < 0 ? -1 : vm_copy3(current->pagetable, (void *)s, d, n, 1);
}
int copystr2(char *d, uint64 s, int n) {
  for (int i = 0; i < n; i++) {
    if (s + (uint64)i < s || copyin2(d + i, s + i, 1) < 0)
      return -1;
    if (!d[i])
      return 0;
  }
  return -1;
}
int vm_sbrk3(struct process *p, int n, uint64 *previous) {
  uint64 old = p->heap, next;
  if (n >= 0) {
    if ((uint64)n > UGUARD_BEGIN - old)
      return -1;
    next = old + (uint64)n;
    if (vm_alloc3(p->pagetable, old, next, PTE_R | PTE_W | PTE_U) < 0)
      return -1;
  } else {
    uint64 sub = (uint64)(-(long)n);
    if (sub > old - p->heap_base)
      return -1;
    next = old - sub;
    vm_unmap3(p->pagetable, next, PGROUNDUP(old));
  }
  p->heap = next;
  *previous = old;
  return 0;
}
static void kernel_map(pagetable_t pt, uint64 a, uint64 end, int flags) {
  for (; a < end; a += PGSIZE)
    if (map3(pt, a, a, flags) < 0)
      panic2("kernel mapping allocation");
}
void vm_init(void) {
  kinit3();
  pagetable_t pt = kalloc3();
  if (!pt)
    panic2("kernel root allocation");
  kernel_map(pt, KERNBASE, (uint64)trampoline + PGSIZE, PTE_R | PTE_X);
  kernel_map(pt, (uint64)trampoline + PGSIZE, PHYSTOP, PTE_R | PTE_W);
  kernel_map(pt, UART0, UART0 + PGSIZE, PTE_R | PTE_W);
  kernel_map(pt, CLINT_BASE, CLINT_BASE + 0x10000, PTE_R | PTE_W);
  kernel_map(pt, PLIC, PLIC + 0x400000, PTE_R | PTE_W);
  if (((uint64)trampoline & (PGSIZE - 1)) ||
      map3(pt, TRAMPOLINE, (uint64)trampoline, PTE_R | PTE_X) < 0)
    panic2("trampoline mapping");
  kernel_satp = MAKE_SATP(pt);
  w_satp(kernel_satp);
  sfence_vma();
}
