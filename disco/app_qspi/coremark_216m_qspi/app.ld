/* qspi_map stage-2 app linker script - runs entirely from the QSPI-mapped
 * space (QUADSPI memory-mapped mode).
 *
 * Memory map:
 *   QSPI (rx) : 8 MB @ 0x90000000   (vector table + code + rodata + .data LMA)
 *   DTCM (xrw): 128 KB @ 0x20000000 (stack + RW/ZI + .dcm_heap reserve)
 *   SDRAM (rwx): 16 MB @ 0xC0000000 (heap + opt-in .sdram buffers)
 *
 * The bootloader puts the QUADSPI in memory-mapped mode and jumps to the reset
 * vector at 0x90000000. .data/.bss/stack stay in the on-chip DTCM; the SDRAM
 * carries the `.sdram` buffers and the malloc() heap (newlib's pre-main
 * allocations use the small .dcm_heap DTCM reserve first - see syscalls.c).
 * This app deliberately does NOT own the RCC/PLL2 clocks (the bootloader
 * does) - see system_app.c.
 */

ENTRY(Reset_Handler)

MEMORY
{
  QSPI (rx)  : ORIGIN = 0x90000000, LENGTH = 8M
  DTCM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  SDRAM (rwx): ORIGIN = 0xC0000000, LENGTH = 16M
}

_estack        = ORIGIN(DTCM) + LENGTH(DTCM);  /* top of DTCM, stack grows down */
_Min_Heap_Size = 0x400;
_Min_Stack_Size = 0x1000;

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >QSPI

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >QSPI

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >QSPI

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >QSPI

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >QSPI

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >QSPI

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >QSPI

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >QSPI

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    __data_start__ = _sdata;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
    __data_end__ = _edata;
  } >DTCM AT> QSPI

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >DTCM

  /* Small on-chip heap RESERVE for newlib's pre-main _sbrk() calls (they
   * happen before the SDRAM is usable - see syscalls.c / stm32f769ni.ld). */
  .dcm_heap :
  {
    . = ALIGN(8);
    _dcm_heap_start = .;
    . = . + 0x2000;   /* 8 KB */
    _dcm_heap_end = .;
    . = ALIGN(8);
  } >DTCM

  /* On-board SDRAM (16 MB @ 0xC0000000). NOBITS - zeroed by board.c's
   * SDRAM_Init() after the FMC/SDRAM is up, not by the startup. */
  .sdram (NOLOAD) :
  {
    . = ALIGN(4);
    _sdram_start = .;
    *(.sdram)
    *(.sdram.*)
    . = ALIGN(4);
    _sdram_end = .;
  } >SDRAM

  /* Heap in the SDRAM, after the .sdram buffers (capped at 0xC1000000 by
   * syscalls.c). newlib's pre-main _sbrk() calls use the .dcm_heap DTCM
   * reserve instead, then _sbrk switches here once Board_Init()/SDRAM_Init()
   * has brought the SDRAM up - see syscalls.c and stm32f769ni.ld. */
  ._user_heap :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = ALIGN(8);
  } >SDRAM

  /* Stabs debugging sections. */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment       0 : { *(.comment) }
  /* DWARF debug sections. */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  .debug_macro    0 : { *(.debug_macro) }
  .ARM.attributes 0 : { KEEP (*(.ARM.attributes)) }
  .note.gnu.arm.ident 0 : { KEEP (*(.note.gnu.arm.ident)) }
}
