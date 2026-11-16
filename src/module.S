.section .module_info,"a"
.balign 4
.global module_info
module_info:
 .short 0
 .byte 1,0
 .asciz "ps2_disc_cache"
 .zero 13
 .long __toc_base
 .long exports_start
 .long exports_end
 .long imports_start
 .long imports_end
.section .exports,"a"
exports_start:
 .byte 28,0
 .short 0x0101,0x8000,2,0,0
 .long 0,0,nids,export_opds
exports_end:
nids: .long 0xab779874,0xbc9a0086
export_opds: .long stop_opd,start_opd
stop_opd: .long entry_stop,__toc_base
start_opd: .long entry_start,__toc_base
.section .prx_param,"a"
.balign 4
.global prx_param_start
prx_param_start:
 .long 0x28,0x1b434cec,2,0
 .long exports_start,exports_end,imports_start,imports_end
 .short 0x0101,0
 .long 0

/* PRX imports, PSL1GHT layout (ppu/sprx/common/libexport.c): one 0x2c entry per
 * library; the loader writes each function's 32-bit OPD address into its stub slot. */
.section .lib.stub,"a"
.balign 4
imports_start:
 .long 0x2c000001
 .short 0x0009,1
 .long 0,0
 .long libname0,fnids0,stubs0
 .long 0,0,0,0
 .long 0x2c000001
 .short 0x0009,1
 .long 0,0
 .long libname1,fnids1,stubs1
 .long 0,0,0,0
 .long 0x2c000001
 .short 0x0009,1
 .long 0,0
 .long libname2,fnids2,stubs2
 .long 0,0,0,0
 .long 0x2c000001
 .short 0x0009,1
 .long 0,0
 .long libname4,fnids4,stubs4
 .long 0,0,0,0
 .long 0x2c000001
 .short 0x0009,4
 .long 0,0
 .long libname5,fnids5,stubs5
 .long 0,0,0,0
 .long 0x2c000001
 .short 0x0009,1
 .long 0,0
 .long libname6,fnids6,stubs6
 .long 0,0,0,0
imports_end:
.section .rodata.sceFNID,"a"
.balign 4
fnids0:
 .long 0x24a1ea07
.section .rodata.sceResident,"a"
libname0: .asciz "sysPrxForUser"
.section .data.sceFStub,"aw"
.balign 4
stubs0:
slot_sys_thread_create_ex: .long 0
.section .rodata.sceFNID,"a"
.balign 4
fnids1:
 .long 0xa02d46e7
.section .rodata.sceResident,"a"
libname1: .asciz "vshtask"
.section .data.sceFStub,"aw"
.balign 4
stubs1:
slot_imp_vshtask_notify: .long 0
.section .rodata.sceFNID,"a"
.balign 4
fnids2:
 .long 0xeb757101
.section .rodata.sceResident,"a"
libname2: .asciz "vshmain"
.section .data.sceFStub,"aw"
.balign 4
stubs2:
slot_imp_vshmain_mode: .long 0
.section .rodata.sceFNID,"a"
.balign 4
fnids4:
 .long 0xaa3b4bcd
.section .rodata.sceResident,"a"
libname4: .asciz "sys_fs"
.section .data.sceFStub,"aw"
.balign 4
stubs4:
slot_imp_fs_free_size: .long 0
.section .rodata.sceFNID,"a"
.balign 4
fnids5:
 .long 0x9c056962
 .long 0x64f66d35
 .long 0xdc751b40
 .long 0x6db6e8cd
.section .rodata.sceResident,"a"
libname5: .asciz "sys_net"
.section .data.sceFStub,"aw"
.balign 4
stubs5:
slot_imp_socket: .long 0
slot_imp_connect: .long 0
slot_imp_send: .long 0
slot_imp_socketclose: .long 0
.section .opd,"aw"
.balign 8
.globl sys_thread_create_ex
sys_thread_create_ex: .quad .sys_thread_create_ex,.TOC.@tocbase,0
.text
.balign 4
.sys_thread_create_ex:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_sys_thread_create_ex@ha
 lwz 12,slot_sys_thread_create_ex@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_vshtask_notify
imp_vshtask_notify: .quad .imp_vshtask_notify,.TOC.@tocbase,0
.text
.balign 4
.imp_vshtask_notify:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_vshtask_notify@ha
 lwz 12,slot_imp_vshtask_notify@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_vshmain_mode
imp_vshmain_mode: .quad .imp_vshmain_mode,.TOC.@tocbase,0
.text
.balign 4
.imp_vshmain_mode:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_vshmain_mode@ha
 lwz 12,slot_imp_vshmain_mode@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_fs_free_size
imp_fs_free_size: .quad .imp_fs_free_size,.TOC.@tocbase,0
.text
.balign 4
.imp_fs_free_size:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_fs_free_size@ha
 lwz 12,slot_imp_fs_free_size@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_socket
imp_socket: .quad .imp_socket,.TOC.@tocbase,0
.text
.balign 4
.imp_socket:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_socket@ha
 lwz 12,slot_imp_socket@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_connect
imp_connect: .quad .imp_connect,.TOC.@tocbase,0
.text
.balign 4
.imp_connect:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_connect@ha
 lwz 12,slot_imp_connect@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_send
imp_send: .quad .imp_send,.TOC.@tocbase,0
.text
.balign 4
.imp_send:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_send@ha
 lwz 12,slot_imp_send@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .opd,"aw"
.balign 8
.globl imp_socketclose
imp_socketclose: .quad .imp_socketclose,.TOC.@tocbase,0
.text
.balign 4
.imp_socketclose:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_socketclose@ha
 lwz 12,slot_imp_socketclose@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
.section .rodata.sceFNID,"a"
.balign 4
fnids6:
 .long 0x8b72cda1
.section .rodata.sceResident,"a"
libname6: .asciz "sys_io"
.section .data.sceFStub,"aw"
.balign 4
stubs6:
slot_imp_pad_get_data: .long 0
.section .opd,"aw"
.balign 8
.globl imp_pad_get_data
imp_pad_get_data: .quad .imp_pad_get_data,.TOC.@tocbase,0
.text
.balign 4
.imp_pad_get_data:
 mflr 0
 std 0,16(1)
 std 2,40(1)
 stdu 1,-128(1)
 lis 12,slot_imp_pad_get_data@ha
 lwz 12,slot_imp_pad_get_data@l(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 addi 1,1,128
 ld 2,40(1)
 ld 0,16(1)
 mtlr 0
 blr
