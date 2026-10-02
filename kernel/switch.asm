; context_switch(uint64_t *old_rsp, uint64_t new_rsp)
;   RDI = pointer to store current RSP
;   RSI = new RSP to load
;
; Saves callee-saved registers (r15, r14, r13, r12, rbp, rbx),
; saves RSP into *old_rsp, loads new_rsp, restores, returns.

BITS 64
section .text

global context_switch
context_switch:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp          ; *old_rsp = current RSP
    mov rsp, rsi            ; RSP = new_rsp

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret

; Entry point for a brand-new thread.
; Called (via context_switch) with a fresh stack.
; It reads the current thread's fn and calls it.
global thread_start_trampoline
extern thread_runner
thread_start_trampoline:
    sti                     ; enable interrupts for this fresh thread
    call thread_runner
.hang:
    cli
    hlt
    jmp .hang
