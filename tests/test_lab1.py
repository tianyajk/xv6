#!/usr/bin/env python3
"""Lab2 中的 Lab1 回归：banner 保持精确前缀，随后进入 Shell。"""
from pathlib import Path
import subprocess,tempfile,re
ROOT=Path(__file__).resolve().parents[1]
expected=(ROOT/'expect_banner.txt').read_bytes()
for run in (1,2):
    with tempfile.TemporaryDirectory(prefix='lab2-boot-') as d:
        log=Path(d)/'int.log'
        p=subprocess.Popen(['qemu-system-riscv64','-machine','virt','-bios','none','-kernel',str(ROOT/'kernel/kernel'),'-nographic','-smp','1','-d','int','-D',str(log)],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:out,err=p.communicate(timeout=.4)
        except subprocess.TimeoutExpired:p.kill();out,err=p.communicate()
        traps=log.read_text()
        assert out.startswith(expected) and out.count(expected)==1,(run,out)
        assert b'sh> ' in out and err==b'',(run,out,err)
        causes=re.findall(r'async:([01]), cause:([0-9a-f]+)',traps)
        assert all((int(a),int(c,16)) in ((0,8),(1,1),(1,7),(1,9)) for a,c in causes),traps[-2000:]
        print(f'PASS boot run={run} banner={expected!r} shell=yes unexpected_traps=0')
