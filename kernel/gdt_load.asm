; gdt_load(struct gdt_ptr *p)
;   RDI = pointer to {u16 limit, u64 base}

BITS 64
section .text
global gdt_load

gdt_load:
    lgdt [rdi]

    ; Reload CS: far return to a 64-bit kernel code label
    push qword 0x08
    lea rax, [rel .reload]
    push rax
    retfq

.reload:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    ret
