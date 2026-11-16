"""Checks every TOC-relative access in build/plugin.elf against the exported __toc_base.
Usage: python3 tests/tocchk.py build/plugin.elf"""
import sys,struct,subprocess
f=sys.argv[1]
nm=subprocess.run(['powerpc64-linux-gnu-nm',f],capture_output=True,text=True).stdout
tb=[int(l.split()[0],16) for l in nm.splitlines() if l.endswith(' __toc_base')][0]
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection
e=ELFFile(open(f,'rb'));bad=ok=0
text=e.get_section_by_name('.text');td=text.data();ta=text['sh_addr']
for sec in e.iter_sections():
    if not isinstance(sec,RelocationSection) or sec.name!='.rela.text':continue
    syms=e.get_section(sec['sh_link'])
    for r in sec.iter_relocations():
        t=r['r_info_type']
        if t not in (47,48,49,50,63,64):continue
        S=syms.get_symbol(r['r_info_sym'])['st_value']+r['r_addend']
        o=r['r_offset']-ta;h=struct.unpack('>h',td[o:o+2])[0]
        if t in (63,64):h&=~3
        if t in (47,63,64): got=tb+h
        elif t==48: got=None
        else: continue
        if got is None:continue
        ok+= got==S; bad+= got!=S
print(f,'__toc_base',hex(tb),'TOC16 ok',ok,'bad',bad)
