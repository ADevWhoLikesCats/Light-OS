; ============================================================
; isr.asm — exception stubs for vectors 0..31
;
; For each vector:
;   - If the CPU did NOT push an error code, we push a dummy 0.
;   - We push the vector number.
;   - We jump to isr_common.
;
; isr_common:
;   - Saves all general-purpose registers in the order that
;     matches struct regs in idt.h.
;   - Calls isr_handler(struct regs *) with RDI = RSP.
;   - Restores registers and iretq.
;
; Stack layout at the moment isr_handler is called
; (lowest address at the bottom, RSP points at r15):
;
;   offset  +0   : r15
;   offset  +8   : r14
;   offset +16   : r13
;   offset +24   : r12
;   offset +32   : r11
;   offset +40   : r10
;   offset +48   : r9
;   offset +56   : r8
;   offset +64   : rbp
;   offset +72   : rdi
;   offset +80   : rsi
;   offset +88   : rdx
;   offset +96   : rcx
;   offset +104  : rbx
;   offset +112  : rax
;   offset +120  : vector
;   offset +128  : error_code
;   offset +136  : rip        (pushed by CPU)
;   offset +144  : cs         (pushed by CPU)
;   offset +152  : rflags     (pushed by CPU)
;   offset +160  : rsp        (pushed by CPU, if CPL change)
;   offset +168  : ss         (pushed by CPU, if CPL change)
;
; In ring 0 -> ring 0, the CPU does NOT push rsp/ss. But we
; always enter from ring 0 for now, so rsp/ss are not present.
; We still declare them in struct regs; do not read them yet.
; ============================================================

BITS 64

section .text

extern isr_handler

; ---- Macro: stub without error code ----
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0            ; dummy error code
    push qword %1           ; vector
    jmp isr_common
%endmacro

; ---- Macro: stub with error code ----
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1           ; vector (error already on stack)
    jmp isr_common
%endmacro

; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ---- Common entry ----
isr_common:
    ; Save all GPRs. Push order is reverse of struct regs order
    ; so that after all pushes, the lowest address holds r15.
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; RDI = pointer to struct regs (= current RSP)
    mov rdi, rsp

    ; Align stack to 16 bytes before calling C.
    ; SysV ABI requires RSP % 16 == 0 at the call instruction.
    ; Current RSP is already 8 mod 16 after the call to isr_handler
    ; will push the return address. We need to compensate.
    ; But isr_handler never returns for now, so alignment doesn't
    ; strictly matter. Keep it simple and skip alignment.

    call isr_handler

    ; If isr_handler ever returns, restore and iretq.
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16             ; pop vector + error code
    iretq

; ---- Stub pointer table for idt.c ----
section .rodata
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 32
    dq isr %+ i
%assign i i+1
%endrep
