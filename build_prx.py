#!/usr/bin/env python3
"""Convert an ELFv1 link with retained relocations to SCE PRX relocations.
Ref: RPCS3 PPUModule.cpp ppu_load_prx relocation format. Fails on unsupported
relocations instead of silently producing an incomplete module.
"""
import struct,sys,json
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

def build(src,dst):
 with open(src,'rb') as f:
  e=ELFFile(f);segs=[s for s in e.iter_segments() if s['p_type']=='PT_LOAD']
  assert len(segs)==2
  param=e.get_section_by_name('.prx_param')
  assert param is not None and param['sh_size']==0x28
  assert any(s['p_type']==0x60000002 for s in e.iter_segments())
  rels=[];counts={}
  def loc(addr):
   for i,s in enumerate(segs):
    if s['p_vaddr']<=addr<s['p_vaddr']+s['p_memsz']:return i,addr-s['p_vaddr']
   raise ValueError(f'Address outside load segments: {addr:x}')
  toc=e.get_section_by_name('.toc')['sh_addr']+0x8000
  for sec in e.iter_sections():
   if not isinstance(sec,RelocationSection):continue
   target=e.get_section(sec['sh_info'])
   if not target['sh_flags']&2:continue
   syms=e.get_section(sec['sh_link'])
   for r in sec.iter_relocations():
    t=r['r_info_type'];sym=syms.get_symbol(r['r_info_sym']);v=sym['st_value']+r['r_addend'];a=r['r_offset']
    ai,ao=loc(a)
    if t==51: # TOC base in ELFv1 function descriptors
     t=38;v=toc
    if t in (47,48,49,50,63,64):
     # Linker already resolved TOC-relative offsets. They remain valid only
     # when target and TOC belong to the same relocatable segment.
     assert loc(v)[0]==loc(toc)[0],(t,hex(v));continue
    if t in (10,11):
     # ELFv1 branch symbols may name OPDs. Read the linker's resolved branch,
     # not the descriptor's address, to obtain the actual code destination.
     ss=segs[ai];insn=struct.unpack_from('>I',ss.data(),ao)[0]
     mask,bits=(0x3fffffc,26) if t==10 else (0xfffc,16)
     delta=insn&mask
     if delta&(1<<(bits-1)):delta-=1<<bits
     assert not insn&2,'Absolute branch is not expected'
     v=a+delta
    if t in (10,11,26,44):
     if loc(v)[0]==ai:continue
     if t==26:raise ValueError('intersegment REL32 is unsupported')
    if t==0:continue
    if t not in (1,4,5,6,10,11,38,44,57):raise ValueError(f'Unsupported relocation {t} at {a:x}')
    try: vi,vo=loc(v)
    except ValueError:
     # e.g. &object-1: place it by the symbol's segment, offset may be negative
     vi,_=loc(sym['st_value']);vo=(v-segs[vi]['p_vaddr'])&0xffffffffffffffff
     if t not in (1,38):raise
    rels.append(struct.pack('>QHBBIQ',ao,0,vi,ai,t,vo));counts[t]=counts.get(t,0)+1
  # File offsets retain ELF alignment. PRX segment vaddrs are normalized to 0.
  out=bytearray(0x1000);headers=[]
  for i,s in enumerate(segs):
   off=len(out);data=s.data();out+=data
   info=off+(e.header['e_entry']-s['p_vaddr']) if i==0 else 0
   # Segment vaddrs follow each other (no overlap); PS3 access bits as in
   # webMAN's webftp_server.sprx (0x400005 code, 0x600006 data).
   va=0 if i==0 else (segs[0]['p_memsz']+0xfff)//0x1000*0x1000
   flags=s['p_flags']|(0x400000 if i==0 else 0x600000)
   headers.append((1,flags,off,va,info,len(data),s['p_memsz'],0x1000))
   out+=bytes((-len(out))%0x1000)
  param_segment, param_offset=loc(param['sh_addr'])
  assert param_segment==0
  headers.append((0x60000002,0,headers[0][2]+param_offset,0,0,param['sh_size'],param['sh_size'],4))
  off=len(out);data=b''.join(rels);out+=data
  headers.append((0x700000a4,0,off,0,0,len(data),0,8))
  ident=b'\x7fELF\x02\x02\x01\x66'+bytes(8)
  out[:64]=struct.pack('>16sHHIQQQIHHHHHH',ident,0xffa4,21,1,0,64,0,0x01000000,64,56,len(headers),0,0,0)
  for i,h in enumerate(headers):struct.pack_into('>II6Q',out,64+56*i,*h)
  Path(dst).write_bytes(out)
  print(json.dumps({'bytes':len(out),'relocations':counts,'segments':[(s['p_filesz'],s['p_memsz']) for s in segs]}))
if __name__=='__main__':build(*sys.argv[1:])
