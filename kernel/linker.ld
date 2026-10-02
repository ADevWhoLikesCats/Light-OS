ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : ALIGN(4096) {
        KEEP(*(.text._start))
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(4096);
        __kernel_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
