#!/usr/bin/env python3
"""在指定目录生成内嵌程序表，保持课程的表项/映像交错 ABI。"""
from pathlib import Path
import argparse,re
p=argparse.ArgumentParser()
p.add_argument('--output',required=True)
p.add_argument('--bin-dir',required=True)
p.add_argument('programs',nargs='+')
a=p.parse_args()
lines=['.section .rodata','.balign 8','.global _uprog_table','_uprog_table:']
for name in a.programs:
    if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',name):p.error('invalid program name')
    binary=Path(a.bin_dir,name+'.bin')
    if not binary.is_file():p.error('missing binary '+str(binary))
    lines += [f'.quad _uprog_{name}_start',f'.quad _uprog_{name}_end',f'.asciz "{name}"','.balign 8',
              f'.global _uprog_{name}_start',f'_uprog_{name}_start:',f'.incbin "{binary.as_posix()}"',
              f'.global _uprog_{name}_end',f'_uprog_{name}_end:','.balign 8']
lines += ['.quad 0','.quad 0']
output=Path(a.output);output.parent.mkdir(parents=True,exist_ok=True)
tmp=output.with_suffix(output.suffix+'.tmp');tmp.write_text('\n'.join(lines)+'\n');tmp.replace(output)
