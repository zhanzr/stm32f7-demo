/* GNU ARM GCC linker script for STM32F769NIH6 (STM32F769I-Discovery).
 *
 * Memory map (F769NI):
 *   FLASH      :   2 MB @ 0x08000000   (execution)
 *   DTCM       : 128 KB @ 0x20000000   (stack + RW/ZI, core-private)
 *   AXI SRAM1-3: 400 KB @ 0x20020000   (RESERVED - not used by the projects)
 *   SDRAM      :  16 MB @ 0xC0000000   (on-board FMC SDRAM - heap + `.sdram`)
 *
 * The SDRAM is divided into two linker-managed regions (no raw addresses):
 *   `.sdram`   - opt-in zero-init buffers (cleared by board.c's SDRAM_Init):
 *                uint8_t buf[4096] __attribute__((section(".sdram")));
 *   `._user_heap` - the malloc() heap, placed AFTER `.sdram` and capped at
 *                the SDRAM top (16 MB @ 0xC0000000 -> 0xC1000000) by syscalls.c.
 *
 * The SDRAM is brought up by SystemInit() at 216 MHz BEFORE the startup .data
 * copy and __libc_init_array (which calls _sbrk/malloc a few times), so the
 * heap works from the very first allocation. .data/.bss/stack stay in DTCM:
 * the Cortex-M7 loads the initial SP from the vector table before SystemInit
 * runs, and the bootstrap .data copy must not touch the external SDRAM.
 *
 * Symbols consumed by startup_stm32f769xx.s:
 *   _estack, _sidata, _sdata, _edata, _sbss, _ebss
 * Symbols consumed by syscalls.c (_sbrk): end / _end
 * Symbols consumed by board.c (SDRAM_Init): _sdram_start / _sdram_end
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 2M
  DTCM (xrw)   : ORIGIN = 0x20000000, LENGTH = 128K
  SDRAM (rwx)  : ORIGIN = 0xC0000000, LENGTH = 16M
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
  } >FLASH

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
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

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
  } >DTCM AT> FLASH

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

  /* Small on-chip heap RESERVE: newlib calls _sbrk()/malloc() a few times
   * during __libc_init_array (before main), when accesses to the external
   * SDRAM fault. _sbrk (syscalls.c) serves those pre-main allocations from
   * here, then switches to the SDRAM heap once Board_Init() is done. */
  .dcm_heap :
  {
    . = ALIGN(8);
    _dcm_heap_start = .;
    . = . + 0x2000;   /* 8 KB */
    _dcm_heap_end = .;
    . = ALIGN(8);
  } >DTCM

  /* On-board SDRAM, opt-in zero-init section (cleared by board.c's
   * SDRAM_Init() after the SDRAM is up, not by the startup):
   *     uint8_t buf[4096] __attribute__((section(".sdram"))); */
  .sdram (NOLOAD) :
  {
    . = ALIGN(4);
    _sdram_start = .;
    *(.sdram)
    *(.sdram.*)
    . = ALIGN(4);
    _sdram_end = .;
  } >SDRAM

  /* Heap in the on-board SDRAM, AFTER the .sdram buffers so _sbrk
   * (syscalls.c, capped at 0xC1000000) can never grow into them. The SDRAM is
   * brought up by SystemInit() before the C runtime, so newlib's early
   * _sbrk()/malloc() calls during __libc_init_array already use it. */
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
