; ------------------------------------------------------------
; exit_ctx.asm
;
; Provides two functions:
;   void exit_ctx_save(uint64_t *ctx);   -- saves RSP, RBP, RBX, R12-R15
;   void exit_ctx_restore(uint64_t *ctx); -- restores and returns
;
; Context layout in memory (8 qwords):
;   [0] RSP
;   [1] RBP
;   [2] RBX
;   [3] R12
;   [4] R13
;   [5] R14
;   [6] R15
;   [7] RIP (return address to resume at)
; ------------------------------------------------------------

BITS 64
section .text

global exit_ctx_save
global exit_ctx_restore

exit_ctx_save:
    ; RDI = pointer to context array
    mov [rdi + 0], rsp
    mov [rdi + 8], rbp
    mov [rdi + 16], rbx
    mov [rdi + 24], r12
    mov [rdi + 32], r13
    mov [rdi + 40], r14
    mov [rdi + 48], r15
    lea rax, [rel .resume]
    mov [rdi + 56], rax
    xor eax, eax            ; return 0 (save path)
    ret
.resume:
    mov eax, 1              ; return 1 (restore path)
    ret

exit_ctx_restore:
    ; RDI = pointer to context array
    mov rsp, [rdi + 0]
    mov rbp, [rdi + 8]
    mov rbx, [rdi + 16]
    mov r12, [rdi + 24]
    mov r13, [rdi + 32]
    mov r14, [rdi + 40]
    mov r15, [rdi + 48]
    mov rax, [rdi + 56]
    jmp rax
