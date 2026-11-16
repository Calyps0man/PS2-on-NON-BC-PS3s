#!/usr/bin/env python3
"""Structural/load-relocation checks for the built PRX. Does not execute PS3 firmware.
Simulates the PRX loader's relocations at 20 load layouts, then checks module
info, exports, the sysPrxForUser import, the trampoline's slot address, and the
program-header fields matched to webMAN's webftp_server.sprx."""
import struct, sys
from pathlib import Path
from elftools.elf.elffile import ELFFile

root = Path(__file__).resolve().parents[1]
p = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build/ps2_disc_cache.prx'
raw = p.read_bytes()
with p.open('rb') as f:
    e = ELFFile(f)
    assert e.header.e_type == 0xffa4 and e.header.e_flags == 0x01000000
    seg = list(e.iter_segments())
    assert len(seg) == 4 and seg[2]['p_type'] == 0x60000002 and seg[3]['p_type'] == 0x700000a4
    assert seg[0]['p_flags'] == 0x400005 and seg[1]['p_flags'] == 0x600006
    assert seg[0]['p_vaddr'] == 0 and seg[1]['p_vaddr'] >= seg[0]['p_memsz'], 'segments overlap'
    assert seg[3]['p_memsz'] == 0
    rel = seg[3].data()
    for trial in range(20):
        bases = [0x10000000 + trial * 0x100000, 0x30000000 + trial * 0x100000]
        mem = [bytearray(s.data()) + bytearray(s['p_memsz'] - s['p_filesz']) for s in seg[:2]]
        seen = set()
        for off, zero, vi, ai, typ, val in struct.iter_unpack('>QHBBIQ', rel):
            assert not zero and typ in (1, 4, 6, 38) and vi < 2 and ai < 2, typ
            assert (ai, off) not in seen
            seen.add((ai, off))
            assert val < seg[vi]['p_memsz']
            v = bases[vi] + val
            if typ == 1:
                struct.pack_into('>I', mem[ai], off, v)
            elif typ == 38:
                struct.pack_into('>Q', mem[ai], off, v)
            elif typ == 4:
                struct.pack_into('>H', mem[ai], off, v & 0xffff)
            elif typ == 6:
                struct.pack_into('>H', mem[ai], off, ((v + 0x8000) >> 16) & 0xffff)
        rd = lambda a, fmt: struct.unpack_from(fmt, mem[0] if a < bases[1] else mem[1], a - (bases[0] if a < bases[1] else bases[1]))
        info = seg[0]['p_paddr'] - seg[0]['p_offset']
        attr, maj, mnr, name, toc, exp, end, imp, iend = struct.unpack_from('>HBB28sIIIII', mem[0], info)
        assert name.rstrip(b'\0') == b'ps2_disc_cache' and (maj, mnr) == (1, 0)
        assert bases[1] <= toc < bases[1] + len(mem[1]) and end - exp == 28
        assert (iend - imp) % 0x2c == 0 and iend > imp
        prx = seg[2]['p_offset'] - seg[0]['p_offset']
        size, magic, ver, unk, es, ee, ss, se = struct.unpack_from('>IIIIIIII', mem[0], prx)
        assert (size, magic, ver, unk) == (0x28, 0x1b434cec, 2, 0) and (es, ee, ss, se) == (exp, end, imp, iend)
        # exports
        x = exp - bases[0]
        assert mem[0][x] == 28 and struct.unpack_from('>HH', mem[0], x + 4) == (0x8000, 2)
        nids, opds = struct.unpack_from('>II', mem[0], x + 20)
        assert rd(nids, '>II') == (0xab779874, 0xbc9a0086)
        for opd in rd(opds, '>II'):
            pc, rtoc = rd(opd, '>II')
            assert rtoc == toc and bases[0] <= pc < bases[0] + len(mem[0])
        # import entries (PSL1GHT layout)
        def cstr(a):
            m_, b_ = (mem[1], bases[1]) if a >= bases[1] else (mem[0], bases[0])
            return bytes(m_[a - b_:]).split(b'\0')[0]
        slots = {}
        for ie in range(imp, iend, 0x2c):
            h1, h2, count, z1, z2, nm, fn, st, *rest = rd(ie, '>IHHIIIII4I')
            assert (h1, h2, z1, z2) == (0x2c000001, 0x0009, 0, 0) and rest == [0, 0, 0, 0] and count
            assert bases[1] <= st and st + 4 * count <= bases[1] + seg[1]['p_filesz']
            for k in range(count):
                slots[st + 4 * k] = (cstr(nm), rd(fn + 4 * k, '>I')[0])
    # every slot is loaded by exactly one trampoline (lis r12,HA / lwz r12,LO(r12))
    code = mem[0]
    hits = {}
    for o in range(0, len(code) - 8, 4):
        a, b = struct.unpack_from('>II', code, o)
        if a >> 16 == 0x3d80 and b >> 16 == 0x818c:
            lo = b & 0xffff
            lo = lo - 0x10000 if lo & 0x8000 else lo
            ad = (((a & 0xffff) << 16) + lo) & 0xffffffff
            hits[ad] = hits.get(ad, 0) + 1
    assert set(hits) == set(slots) and all(c == 1 for c in hits.values()), (hits, slots)
    want = {(b'sysPrxForUser', 0x24a1ea07), (b'vshtask', 0xa02d46e7), (b'vshmain', 0xeb757101),
(b'sys_fs', 0xaa3b4bcd), (b'sys_net', 0x9c056962),
            (b'sys_net', 0x64f66d35), (b'sys_net', 0xdc751b40), (b'sys_net', 0x6db6e8cd), (b'sys_io', 0x8b72cda1)}
    assert set(slots.values()) == want
print(f'PASS: headers match webftp layout, {len(rel)//24} relocations at 20 load layouts, '
      f'exports, {len(slots)} imported functions in {(iend-imp)//0x2c} libraries, one trampoline per slot')
