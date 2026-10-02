; syscall_entry - entered via SYSCALL instruction.
;
; On entry:
;   RCX = user RIP (saved by CPU)
;   R11 = user RFLAGS (saved by CPU)
;   RSP = user stack (still!)
;
; We switch to a kernel stack, save everything we need,
; call syscall_handler, then sysretq back.

BITS 64
section .text

extern syscall_handler

global syscall_entry

; Kernel stack for syscall handling.
section .bss
align 16
syscall_kernel_stack:
    resb 16384
syscall_kernel_stack_top:

section .text
syscall_entry:
    ; Save user RSP and switch to kernel stack.
    mov [rel saved_user_rsp], rsp
    lea rsp, [rel syscall_kernel_stack_top]

    ; Save the user's registers we care about.
    ; We don't need to save rcx/r11 (CPU handles them).
    ; Push in an order that lets us pass a struct to C.
    ; struct syscall_regs fields (first 15 are GPRs we care about):
    ;   r15 r14 r13 r12 r11 r10 r9 r8 rdi rsi rbp rbx rdx rcx rax
    ;   rip rflags rsp ss
    ;
    ; We'll pass a pointer to this stack frame as the first argument.
    ; For simplicity, we save what we need and pass a partial struct.
    ;
    ; Push in reverse order of the struct so memory layout matches.
    ; Fields we actually fill: rax, rdi, rsi, rdx, rbx, rbp,
    ;                          r8..r15, rip, rflags, rsp, ss
    push qword 0x23             ; ss (user data, RPL=3)
    push qword [rel saved_user_rsp]  ; rsp
    push r11                    ; rflags
    push rcx                    ; rip
    push rax
    push qword 0                ; rcx placeholder (unused; CPU clobbered it)
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11                    ; r11 placeholder (rflags saved above)
    push r12
    push r13
    push r14
    push r15

    ; RDI = pointer to the register frame
    mov rdi, rsp
    call syscall_handler

    ; Return value in RAX; store into the saved rax slot (offset 14*8).
    mov [rsp + 14*8], rax

    ; Pop back
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    ; Pop rip, rflags, rsp from the frame.
    pop rcx                     ; user rip
    pop r11                     ; user rflags
    pop rsp                     ; user stack
    ; The remaining 'ss' slot is left on the user stack; sysret
    ; derives SS from STAR, not from the stack.

    o64 sysret

section .data
saved_user_rsp: dq 0
