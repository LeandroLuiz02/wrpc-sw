OUTPUT_FORMAT("elf32-littleriscv")
ENTRY(_start)

MEMORY
{
    ram :
	ORIGIN = 0x00000000,
	LENGTH = CONFIG_RAMSIZE - CONFIG_STACKSIZE - CONFIG_DATASIZE
    datacopy :
        ORIGIN = CONFIG_RAMSIZE - CONFIG_STACKSIZE - CONFIG_DATASIZE
	LENGTH = CONFIG_DATASIZE
    stack :
	ORIGIN = CONFIG_RAMSIZE - CONFIG_STACKSIZE
	LENGTH = CONFIG_STACKSIZE
}

SECTIONS
{
    /*--------------------------------------------------------------------*/
    /* Code and read-only segment                                         */
    /*--------------------------------------------------------------------*/

    /* text: Program code section */
    .text :
    {
	*(.boot)
	*(.text)
	*(.text.*)
	*(.gnu.linkonce.t.*)
    } > ram

    /* rodata: Read-only data */
    .rodata :
    {
	*(.rdata)
	*(.rodata)
	*(.rodata.*)
	*(.srodata.*)
	*(.gnu.linkonce.r.*)
    } > ram

    /*--------------------------------------------------------------------*/
    /* Initialized data segment                                           */
    /*--------------------------------------------------------------------*/

    /* Start of initialized data segment */
    . = ALIGN(4);
     _fdata = .;

    /* data: Writable data */
    .data :
#if CONFIG_DATASIZE != 0
	AT (ORIGIN(datacopy))
#endif
    {
	*(.data)
	*(.data.*)
	*(.gnu.linkonce.d.*)
    } > ram

    /* Have _gp point to middle of sdata/sbss to maximize displacement range */
    . = ALIGN(4);
    _gp = . + 0x800;

    /* Writable small data segment */
    .sdata :
    {
	*(.sdata)
	*(.sdata.*)
	*(.gnu.linkonce.s.*)
    } > ram

    . = ALIGN(4);
    _edata = .;

    /*--------------------------------------------------------------------*/
    /* Uninitialized data segment                                         */
    /*--------------------------------------------------------------------*/

    /* Start of uninitialized data segment */
    . = ALIGN(4);
    _fbss = .;

    /* Writable uninitialized small data segment */
    .sbss :
    {
	*(.sbss)
	*(.sbss.*)
	*(.gnu.linkonce.sb.*)
    } > ram

    /* bss: Uninitialized writeable data section */
    _bss_start = .;
    .bss :
    {
	*(.bss)
	*(.bss.*)
	*(.gnu.linkonce.b.*)
	*(COMMON)
    } > ram

#if CONFIG_DATASIZE != 0
    _datacopy = ORIGIN(datacopy);
#endif
	
    /* End of RAM for checking stack overflows */
    PROVIDE(_endram = ORIGIN(stack));

    /* First location in stack is highest address in STACK */
    PROVIDE(_fstack = ORIGIN(stack) + LENGTH(stack));
    PROVIDE(_end = .);
}
