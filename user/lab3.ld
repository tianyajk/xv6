/* Lab3 自有布局；课程 user.ld 原样保留。按页隔开代码、只读数据、可写数据。 */
OUTPUT_ARCH(riscv)
ENTRY(main)
SECTIONS {
  . = 0;
  .text : { *(.text .text.*) }
  . = ALIGN(4096);
  .rodata : { *(.rodata .rodata.* .srodata .srodata.*) }
  . = ALIGN(4096);
  .data : { *(.data .data.* .sdata .sdata.*) }
  .bss : { *(.bss .bss.* .sbss .sbss.*) *(COMMON) }
  end = .;
}
