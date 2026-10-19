#!/usr/bin/env python3
"""生成真实 ELF 入口、段范围和页权限；保留预置内嵌表 ABI。"""
from pathlib import Path
import struct,sys
out=['/* 自动生成的用户 ELF 元数据。 */',
     'struct image_section3 {uint64 va,size; int flags;};']
rows=[]
for name in sys.argv[1:]:
    b=Path('user-flat',name+'.elf').read_bytes()
    assert b[:6]==b'\x7fELF\x02\x01'
    entry=struct.unpack_from('<Q',b,24)[0]
    shoff=struct.unpack_from('<Q',b,40)[0]
    entsize,count=struct.unpack_from('<HH',b,58)
    end=0;sections=[]
    for i in range(count):
        _,_,flags,addr,_,size=struct.unpack_from('<IIQQQQ',b,shoff+i*entsize)
        if flags&2 and size:
            end=max(end,addr+size)
            permissions=2|16|(4 if flags&1 else 0)|(8 if flags&4 else 0)
            assert permissions&12!=12,'W+X section'
            sections.append((addr,size,permissions))
    assert entry<end<128*1024,(name,entry,end)
    out.append(f'static const struct image_section3 sections_{name}[] = {{')
    out.extend(f'  {{{a}UL,{s}UL,{p}}},' for a,s,p in sections)
    out.append('};')
    rows.append(f'  {{"{name}",{entry}UL,{end}UL,sections_{name},{len(sections)}}},')
out.append('static const struct {const char *name; uint64 entry,end; const struct image_section3 *sections; uint64 nsections;} user_entries[] = {')
out+=rows+['};']
Path('kernel/userentry.h').write_text('\n'.join(out)+'\n')
