/*
******************************************************************************
**  链接脚本：STM32F103C8T6
**  Flash 64KB @ 0x08000000，SRAM 20KB @ 0x20000000
**
**  三个 lab 用的是同一颗芯片、同一份内存布局，共用这一个脚本。
**  （向量表、.data 装载地址、堆栈检查这些符号名必须和
**    gcc/startup_stm32f103xb.s 里引用的一致，改名前先对一下。）
******************************************************************************
*/

ENTRY(Reset_Handler)

/* 栈顶 = SRAM 末尾（栈从高地址往低地址生长） */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 链接期检查：RAM 里至少留出这么多堆和栈，不够直接报错 */
_Min_Heap_Size  = 0x200;   /* 512 B  */
_Min_Stack_Size = 0x400;   /* 1 KB   */

MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
}

SECTIONS
{
  /* ---- 中断向量表必须放在 Flash 最开头 ---- */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ---- 代码 ---- */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)         /* ARM 与 Thumb 互调胶合代码 */
    *(.glue_7t)
    *(.eh_frame)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* ---- 常量 ---- */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH

  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /* ---- 已初始化变量：存放在 Flash，运行时由启动代码搬到 RAM ---- */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* ---- 未初始化变量 ---- */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* ---- 堆栈区：只用于链接期占用检查，不产生实际数据 ---- */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
    libc.a  ( * )
    libm.a  ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
