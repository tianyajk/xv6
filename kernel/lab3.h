#ifndef LAB3_H
#define LAB3_H
/* 用户/内核共享的诊断接口；不修改课程系统调用编号表。 */
#define SYS_vmstats3 23
#define SYS_vmtest3 24
#define SYS_dump3 25
#define SYS_vmfail3 26
#define USTACK_TOP 0x40000000UL
#define USTACK_PAGES 2
#define UGUARD_BEGIN (USTACK_TOP - (USTACK_PAGES + LAB3_GUARD_PAGES) * 4096UL)
struct vmstats3 {
  uint64 total, limit, used, free, allocs, frees;
  uint64 guard_begin, guard_end, heap, heap_base;
};
#ifndef __USER_VM3__
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
void kinit3(void);
void *kalloc3(void);
void kfree3(void *);
void kfail3(long);
void kstats3(struct vmstats3 *);
uint64 kused3(void);
pte_t *walk3(pagetable_t, uint64, int);
int map3(pagetable_t, uint64, uint64, int);
pagetable_t vm_create3(struct trapframe *);
void vm_destroy3(pagetable_t);
int vm_alloc3(pagetable_t, uint64, uint64, int);
void vm_unmap3(pagetable_t, uint64, uint64);
int vm_clone3(pagetable_t, pagetable_t);
int vm_check3(pagetable_t, uint64, uint64, int);
int vm_copy3(pagetable_t, void *, uint64, uint64, int);
int vm_load3(pagetable_t, uint64, const void *, uint64);
int vm_sbrk3(struct process *, int, uint64 *);
int vm_selftest3(void);
void dump_pagetable(pagetable_t);
#endif
#endif
