#!/usr/bin/env python3
"""真实 QEMU 单核串口端到端验收；不依赖文件系统或第三方库。"""
from pathlib import Path
import subprocess, selectors, os, time, sys
ROOT = Path(__file__).resolve().parents[1]
class VM:
    def __init__(self):
        self.p = subprocess.Popen(['qemu-system-riscv64','-machine','virt','-bios','none','-kernel',str(ROOT/'kernel/kernel'),'-nographic','-smp','1'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
        self.sel=selectors.DefaultSelector(); self.sel.register(self.p.stdout,selectors.EVENT_READ)
        self.buf=b''; self.mark=0
    def pump(self,sec=.05):
        for key,_ in self.sel.select(sec):
            b=os.read(key.fd,65536)
            if b:self.buf+=b
    def expect(self,s,timeout=6):
        s=s.encode(); end=time.monotonic()+timeout
        while time.monotonic()<end:
            if s in self.buf[self.mark:]:
                self.mark=self.buf.index(s,self.mark)+len(s); return
            self.pump()
        raise AssertionError(f'missing {s!r}: {self.buf[-1800:]!r}')
    def send(self,b):
        if isinstance(b,str):b=b.encode()
        self.p.stdin.write(b);self.p.stdin.flush()
    def close(self):
        self.p.terminate()
        try:self.p.wait(timeout=2)
        except subprocess.TimeoutExpired:self.p.kill();self.p.wait()
        self.sel.close()

def test_shell():
    v=VM()
    try:
        v.expect('OSLAB1 sid=2024302111377 mod97=0x46\n'); v.expect('sh> ')
        for _ in range(3):
            v.send('hi\n');v.expect('hi: user program running, pid=');v.expect('sh> ')
        v.send('badecall\n');v.expect('TEST-1 PASS');v.expect('sh> ')
        v.send('not-found\n');v.expect('exec not-found\n failed');v.expect('sh> ')
        v.send('bufstorm\n');time.sleep(.15)
        # 官方 bufstorm 在字符流模式下每次 read 算一轮；整批输入并精确校验。
        for s in ['one\n','two\n','three\n','four\n']:
            v.send(s);v.expect(s)
        v.expect('BUFSTORM lines=4 bytes=19');v.expect('sh> ')
        v.send('lab2probe\n');v.expect('PROBE core PASS');v.expect('PROBE char ready')
        assert v.buf.count(b'PROBE orphan child PASS\n')==8
        assert v.buf.count(b'PROBE nested orphan PASS\n')==8
        v.send('Z');v.expect('PROBE char PASS');v.expect('PROBE stream ready')
        for offset in range(0,1024,32):
            v.send(bytes(ord('a')+(offset+i)%26 for i in range(32)))
            v.expect(f'PROBE stream bytes={offset+32}\n')
        v.expect('PROBE stream PASS bytes=1024');v.expect('PROBE overflow ready')
        # 逐字节注入避免主机一次突发受硬件 FIFO 容量影响。
        for _ in range(96):
            v.send('x');time.sleep(.004)
        v.expect('PROBE overflow PASS capacity=64');v.expect('PROBE all PASS');v.expect('PROBE interruptible pause ready')
        v.send(b'\x03');v.expect('sh> ')
        v.send(b'\x10');v.expect('[lab2 ticks=')
        import re
        v.expect(']\n')
        stats=re.findall(rb'\[lab2 ticks=(\d+) rx=(\d+) dropped=(\d+) wakeups=(\d+)\]',v.buf)
        assert stats and int(stats[-1][2])==32,stats
        v.send('spin\n');v.expect('spin 0');time.sleep(.1);v.send(b'\x10');v.expect('[lab2 ticks=')
        time.sleep(.1);v.send(b'\x10');v.expect('[lab2 ticks=');v.expect(']\n')
        stats=re.findall(rb'\[lab2 ticks=(\d+) rx=(\d+) dropped=(\d+) wakeups=(\d+)\]',v.buf)
        assert len(stats)>=3 and int(stats[-1][0])>int(stats[-2][0]),stats
        v.send(b'\x03');v.expect('sh> ')
        v.send('hi\n');v.expect('hi: user program running, pid=');v.expect('sh> ')
        v.send('bufstorm\n');time.sleep(.05);v.send(b'\x03');v.expect('sh> ')
        v.send('bufstorm\n');time.sleep(.05);v.send(b'\x04');v.expect('BUFSTORM lines=0 bytes=0');v.expect('sh> ')
        for _ in range(32):
            v.send('hi\n');v.expect('hi: user program running, pid=');v.expect('sh> ')
        assert b'PANIC' not in v.buf and b'PROBE FAIL' not in v.buf,v.buf[-2000:]
        return v.buf
    finally:v.close()

if __name__=='__main__':
    data=test_shell();print('PASS shell: banner, repeated hi, bad syscall, exec failure, bufstorm, spin Ctrl-C, recovery; pointer checks, isolation, slots, timer, stream, overflow=32')
    out=ROOT/'tests/results';out.mkdir(exist_ok=True);(out/'shell.log').write_bytes(data)
