#!/usr/bin/env python3
"""从 ELF 头提取入口与内存范围；平铺二进制本身不含这些信息。"""
from pathlib import Path
import struct,sys
rows=[]
for name in sys.argv[1:]:
    b=Path('user-flat',name+'.elf').read_bytes()
    assert b[:6]==b'\x7fELF\x02\x01'
    entry=struct.unpack_from('<Q',b,24)[0]
    shoff=struct.unpack_from('<Q',b,40)[0]
    entsize,count=struct.unpack_from('<HH',b,58)
    end=0
    for i in range(count):
        _,_,flags,addr,_,size=struct.unpack_from('<IIQQQQ',b,shoff+i*entsize)
        if flags&2:end=max(end,addr+size)
    assert entry<end<=128*1024-8192,(name,entry,end)
    rows.append(f'  {{"{name}", {entry}UL, {end}UL}},')
Path('kernel/userentry.h').write_text('/* 自动生成：不要手改；包含 ELF 的真实入口与 BSS 末端。 */\nstatic const struct { const char *name; uint64 entry, end; } user_entries[] = {\n'+'\n'.join(rows)+'\n};\n')
