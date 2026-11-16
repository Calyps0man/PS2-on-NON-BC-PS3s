#!/usr/bin/env python3
"""Builds build/ps2_disc_cache.prx (unsigned) with a powerpc64 cross toolchain
(powerpc64-linux-gnu-gcc/ld) and PSL1GHT headers. Set PSL1GHT to your checkout."""
import subprocess, sys, os
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_prx import build

root = Path(__file__).resolve().parent
out = root / 'build'
out.mkdir(exist_ok=True)
psl = Path(os.environ.get('PSL1GHT', root.parent / 'PSL1GHT'))
cc = 'powerpc64-linux-gnu-gcc'
ld = 'powerpc64-linux-gnu-ld'
gi = subprocess.run([cc, '-print-file-name=include'], capture_output=True, text=True).stdout.strip()
flags = ['-nostdinc', '-isystem', gi, '-I' + str(root / 'shim'), '-I' + str(psl / 'ppu/include'),
         '-O2', '-mcpu=cell', '-m64', '-mabi=elfv1', '-mcmodel=small', '-fPIC', '-ffreestanding', '-fno-builtin',
         '-fno-stack-protector', '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra', '-Werror']
subprocess.run([cc, *flags, '-S', str(root / 'src/plugin.c'), '-o', str(out / 'plugin.s')], check=True)
s = (out / 'plugin.s').read_text()
for name in ('start', 'stop'):
    label = f'.L.plugin_{name}:'
    assert s.count(label) == 1, label
    s = s.replace(label, f'.globl entry_{name}\nentry_{name}:\n' + label)
(out / 'plugin.s').write_text(s)
for src, dst in [(out / 'plugin.s', out / 'plugin.o'), (root / 'src/module.S', out / 'module.o')]:
    subprocess.run([cc, '-m64', '-mabi=elfv1', '-c', str(src), '-o', str(dst)], check=True)
subprocess.run([ld, '-T', str(root / 'src/module.ld'), '--emit-relocs', '-Map=' + str(out / 'plugin.map'),
                '-o', str(out / 'plugin.elf'), str(out / 'module.o'), str(out / 'plugin.o')], check=True)
build(out / 'plugin.elf', out / 'ps2_disc_cache.prx')
