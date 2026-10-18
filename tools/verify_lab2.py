#!/usr/bin/env python3
"""可在解压后的独立提交包运行；汇总真实命令/退出码，不依赖 Git。"""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import subprocess,hashlib,json,tempfile,sys
ROOT=Path(__file__).resolve().parents[1]
records=[]
def run(cmd,cwd=ROOT):
    p=subprocess.run(cmd,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    records.append({'command':cmd,'input':'current checkout / real QEMU serial','exit':p.returncode,'output':p.stdout})
    print(p.stdout,end='');return p.returncode
ok=True
for path,want in json.loads((ROOT/'support/PRESET_SHA256.json').read_text()).items():
    data=(ROOT/path).read_bytes()
    if isinstance(want,dict):
        # 已有 Git 基线对 CRLF 做规范化；归档后允许相同文本的 LF 字节。
        same=hashlib.sha256(data).hexdigest()==want['sha256'] or hashlib.sha256(data.replace(b'\r\n',b'\n')).hexdigest()==want['lf_sha256']
    else:same=hashlib.sha256(data).hexdigest()==want
    if not same:print('FAIL preset',path);ok=False
records.append({'command':['sha256','support/PRESET_SHA256.json'],'input':'course preset files','output':'PASS course presets unchanged (original baseline CRLF/LF accepted)' if ok else 'FAIL preset integrity','exit':0 if ok else 1})
print(records[-1]['output'])
for cmd in [['make','clean','clean-extra'],['make','-j4'],['python3','check_expect.py','2024302111377'],['python3','tests/test_lab1.py'],['python3','tests/test_lab2.py']]:
    if run(cmd):ok=False;break
if ok:
    with tempfile.TemporaryDirectory(prefix='lab2-printf-') as d:
        binary=str(Path(d)/'test-printf')
        if run(['cc','-fno-builtin','-Ikernel','tests/test_printf_host.c','kernel/printf.c','-o',binary]) or run([binary]):ok=False
summary={'date':datetime.now(ZoneInfo('Asia/Shanghai')).isoformat(),'passed':ok,'records':records}
out=ROOT/'tests/results';out.mkdir(exist_ok=True);(out/'verification.txt').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
print('PASS Lab2 verification' if ok else 'FAIL Lab2 verification')
sys.exit(0 if ok else 1)
