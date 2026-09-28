#!/usr/bin/env python3
"""Run the lab1 kernel twice and check exact UART output and trap log."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = (ROOT / "expect_banner.txt").read_bytes()
CMD = [
    "qemu-system-riscv64", "-machine", "virt", "-bios", "none",
    "-kernel", str(ROOT / "kernel/kernel"), "-nographic",
]

for run in (1, 2):
    with tempfile.TemporaryDirectory(prefix="lab1-qemu-") as tmp:
        trap_log = Path(tmp) / "int.log"
        proc = subprocess.Popen(
            CMD + ["-d", "int", "-D", str(trap_log)],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=ROOT,
        )
        try:
            stdout, stderr = proc.communicate(timeout=1)
        except subprocess.TimeoutExpired:
            proc.kill()
            stdout, stderr = proc.communicate()
        trap_data = trap_log.read_bytes() if trap_log.exists() else b""
        assert stdout == EXPECTED, (run, stdout, EXPECTED)
        assert stderr == b"", (run, stderr)
        assert trap_data == b"", (run, trap_data[:200])
        print(f"PASS QEMU run={run} uart={stdout!r} trap_bytes=0 exit={proc.returncode}")
