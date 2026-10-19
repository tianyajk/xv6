#!/usr/bin/env python3
"""检查生成文件集中存放，且保持课程内核路径兼容。"""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def verify_layout():
    subprocess.run(['make','-j4'],cwd=ROOT,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    assert (ROOT/'build/kernel/kernel').is_file(),'kernel must be built under build/'
    assert (ROOT/'kernel/kernel').is_symlink(),'legacy kernel path must be a compatibility link'
    assert (ROOT/'kernel/kernel').resolve()==(ROOT/'build/kernel/kernel').resolve()
    for dirname in ['kernel','user']:
        assert not list((ROOT/dirname).glob('*.o')),dirname+' contains object files'
    for name in ['user-flat','user/usys.S','kernel/userimg.S','kernel/userentry.h']:
        assert not (ROOT/name).exists(),'legacy generated path remains: '+name
    for name in ['usys.S','userimg.S','userentry.h']:
        assert (ROOT/'build/generated'/name).is_file(),name
    assert len(list((ROOT/'build/kernel').glob('*.o')))>=10
    elf={p.stem for p in (ROOT/'build/user').glob('*.elf')}
    bins={p.stem for p in (ROOT/'build/user').glob('*.bin')}
    assert elf==bins and {'sh','hi','spin'}.issubset(bins)
    if (ROOT/'.git').exists():
        subprocess.run(['git','check-ignore','--quiet','build/kernel/kernel'],cwd=ROOT,check=True)
    else:
        assert '/build/' in (ROOT/'.gitignore').read_text()
    print('PASS build layout: objects, ELF, binaries and generated source under build/; legacy QEMU path linked')
def verify_legacy_header():
    old=ROOT/'kernel/userentry.h'
    assert not old.exists()
    try:
        old.write_text('#error stale legacy header must not be included\n')
        subprocess.run(['make','-B','build/kernel/proc2.o'],cwd=ROOT,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        subprocess.run(['make','clean-extra'],cwd=ROOT,check=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        assert not old.exists() and not (ROOT/'build').exists()
    finally:
        old.unlink(missing_ok=True)
    verify_layout()
    print('PASS legacy metadata: stale source header ignored; clean-extra removes it; rebuild succeeds')
if __name__=='__main__':
    verify_layout()
    verify_legacy_header()
