OUTPUT_FORMAT("elf64-powerpc")
OUTPUT_ARCH(powerpc:common64)
ENTRY(module_info)
PHDRS { code PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); prxparam 0x60000002 FLAGS(0); }
SECTIONS {
 . = 0x10000;
 .module_info : { KEEP(*(.module_info)) } :code
 .exports : { KEEP(*(.exports)) } :code
 .lib.stub : { KEEP(*(.lib.stub)) } :code
 .prx_param : { KEEP(*(.prx_param)) } :code :prxparam
 .text : { *(.text .text.*) } :code
 . = ALIGN(0x10000);
 .data : { *(.data .data.*) *(.rodata .rodata.*) } :data
 .opd : { *(.opd) } :data
 .toc : ALIGN(256) { *(.toc .got) } :data
 __toc_base = ADDR(.toc) + 0x8000;
 .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } :data
 /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
