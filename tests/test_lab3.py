#!/usr/bin/env python3
"""真实 QEMU：Lab3 官方测试、动态配额、页泄漏/回滚、dump ABI。"""
import re
from test_lab2 import VM

def test_lab3():
    v=VM()
    def used():
        start=len(v.buf);v.send(b'\x10');v.expect('[vm3 used=');v.expect(']\n')
        m=re.findall(rb'\[vm3 used=(\d+) free=(\d+) limit=(\d+)\]',v.buf[start:])
        assert m,m
        u,f,limit=map(int,m[-1]);assert u+f==limit
        return u
    try:
        v.expect('sh> ')
        # 启动页表观测：四位权限、地址对齐、用户代码不可写、2页guard无U位。
        block=v.buf.split(b'DUMP-PAGETABLE begin\n')[1].split(b'DUMP-PAGETABLE end')[0]
        lines=block.splitlines()
        assert all(re.fullmatch(rb'(L2|L1|LEAF) va=0x[0-9a-f]+ pa=0x[0-9a-f]+ perm=[r-][w-][x-][u-]',s) for s in lines),block
        leaves={int(a,16):(int(p,16),flags) for a,p,flags in re.findall(rb'LEAF va=0x([0-9a-f]+) pa=0x([0-9a-f]+) perm=([rwxu-]+)',block)}
        assert leaves[0][1]==b'r-xu'
        assert sum(1 for va,(_,perm) in leaves.items() if 0x3fffc000<=va<0x3fffe000 and perm==b'rw--')==2
        assert all(va%4096==0 and pa%4096==0 for va,(pa,_) in leaves.items())
        baseline=used()
        v.send('badaccess\n');v.expect('TEST-3a PASS');v.expect('TEST-3b PASS');v.expect('sh> ')
        assert used()==baseline,'badaccess leaked pages'
        for _ in range(3):
            start=len(v.buf);v.send('memcap\n');v.expect('TEST-4 PASS',timeout=40);v.expect('sh> ')
            chunks=re.search(rb'after (\d+) chunks',v.buf[start:])
            assert chunks and int(chunks[1])>1000,'did not exercise physical memory quota'
            assert used()==baseline,'OOM cleanup leaked pages'
        for _ in range(2):
            v.send('vmprobe\n');v.expect('VM copyout ready');v.send('ABCDEFG');v.expect('VM-PROBE PASS',timeout=40);v.expect('sh> ')
            assert used()==baseline,'rollback/guard/copy test leaked pages'
        for _ in range(32):
            v.send('hi\n');v.expect('hi: user program running');v.expect('sh> ')
            assert used()==baseline,'fork-exec-exit leaked pages'
        v.send('dump\n');v.expect('DUMP-PAGETABLE begin');v.expect('DUMP-PAGETABLE end');v.expect('sh> ')
        assert used()==baseline
        assert b'FAIL' not in v.buf and b'PANIC' not in v.buf,v.buf[-2000:]
        print('PASS Lab3: dump ABI, 2 guard pages, 3 physical OOM runs, atomic rollback, cross-page copies, 32 fork-exec cycles, zero net page leaks')
        return v.buf
    finally:v.close()
if __name__=='__main__':test_lab3()
