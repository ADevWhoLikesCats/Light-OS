; userspace.asm — a tiny ring-3 program.
; We expose:
;   user_blob_start, user_blob_end
;   user_msg   (the string)
;
; The blob is position-independent-ish; it uses absolute addresses
; computed by the linker relative to user_blob_start.

BITS 64
section .rodata
global user_blob_start
global user_blob_end
global user_msg
global user_msg_len

user_blob_start:
    ; write(1, msg, len)
    mov rax, 1                  ; SYS_write
    mov rdi, 1                  ; stdout
    lea rsi, [rel user_msg]
    mov rdx, 18                 ; "hello from ring 3\n" = 18 bytes
    syscall

    ; exit(0)
    mov rax, 60
    xor rdi, rdi
    syscall

    ; If syscall returns (it won't), halt.
.hang:
    jmp .hang

user_msg:
    db "hello from ring 3", 10
user_msg_len equ $ - user_msg

user_blob_end:
