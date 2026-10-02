; ============================================================
; stage2.asm — loaded at 0x0000:0x8000 in 16-bit real mode.
;
; Milestone 2: load kernel, enable A20, build page tables,
; enter x86_64 long mode, copy kernel to 0x100000, jump to it.
;
; Floppy geometry: 80 cyl, 2 heads, 18 sectors/track.
; ============================================================

BITS 16
ORG 0x8000

KERNEL_STAGE_SEG   equ 0x1000
KERNEL_STAGE_OFF   equ 0x0000
KERNEL_LBA         equ 17
KERNEL_SECS        equ 64
KERNEL_FINAL       equ 0x100000     ; physical addr of kernel in long mode

SECTORS_PER_TRACK  equ 18
HEADS              equ 2

; Page table area (16 KiB, aligned)
PML4_ADDR          equ 0x200000
PDPT_ADDR          equ 0x201000
PD_ADDR            equ 0x202000

stage2_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; --- VGA marker ---
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0F53                 ; 'S'
    stosw

    xor ax, ax
    mov es, ax
    mov si, msg_vga
    call vga_print

    call serial_init
    mov si, msg_serial
    call serial_print

    ; --- Load kernel to 0x9000 ---
    call load_kernel
    jc  disk_error
    mov si, msg_kernel_loaded
    call serial_print

    ; --- Enable A20 via port 0x92 ---
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE                   ; don't reset the machine
    out 0x92, al
    mov si, msg_a20
    call serial_print

    ; --- Load GDT ---
    lgdt [gdt_descriptor]

    ; --- Enter protected mode (CR0.PE = 1) ---
    mov eax, cr0
    or  eax, 1
    mov cr0, eax

    ; --- Far jump to 32-bit protected mode entry ---
    jmp 0x08:pm_entry

disk_error:
    mov si, msg_disk_err
    call serial_print
    cli
    hlt

; ------------------------------------------------------------
; load_kernel — CHS track-loop, loads KERNEL_SECS sectors
; from KERNEL_LBA into 0x0000:0x9000.
; Returns CF=1 on error.
; ------------------------------------------------------------
load_kernel:
    pusha
    mov dword [cur_lba], KERNEL_LBA
    mov word  [remaining], KERNEL_SECS
    mov word  [dst_seg], KERNEL_STAGE_SEG
    mov word  [dst_off], KERNEL_STAGE_OFF
.loop:
    mov ax, [remaining]
    test ax, ax
    jz   .done

    mov ax, [cur_lba]
    xor dx, dx
    mov cx, SECTORS_PER_TRACK
    div cx                          ; AX=track, DX=sect_idx
    mov bl, dl
    inc bl                          ; BL=sector

    xor dx, dx
    mov cx, HEADS
    div cx                          ; AX=cyl, DX=head
    mov ch, al
    mov dh, dl
    mov cl, bl

    mov al, SECTORS_PER_TRACK
    sub al, bl
    inc al
    mov ah, [remaining]
    cmp al, ah
    jbe .have_count
    mov al, ah
.have_count:
    push ax
    mov ax, [dst_seg]
    mov es, ax
    mov bx, [dst_off]

    pop ax
    mov ah, 0x02
    mov dl, [boot_drive]
    int 0x13
    jc  .error

    movzx cx, al
    mov eax, [cur_lba]
    add eax, ecx
    mov [cur_lba], eax

    mov ax, [remaining]
    sub ax, cx
    mov [remaining], ax

    mov ax, cx
    shl ax, 9
    add [dst_off], ax
    jnc .loop
    mov ax, [dst_seg]
    add ax, 0x1000
    mov [dst_seg], ax
    jmp .loop

.error:
    popa
    stc
    ret
.done:
    popa
    clc
    ret

; ------------------------------------------------------------
; VGA string printer
; ------------------------------------------------------------
vga_print:
    pusha
    mov di, 0xB800 + (80 * 2 * 1)
    mov ah, 0x0F
.next:
    lodsb
    test al, al
    jz   .done
    stosw
    jmp  .next
.done:
    popa
    ret

; ------------------------------------------------------------
; Serial helpers
; ------------------------------------------------------------
serial_init:
    pusha
    mov dx, 0x3F8 + 1
    xor al, al
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8 + 0
    mov al, 0x03
    out dx, al
    mov dx, 0x3F8 + 1
    xor al, al
    out dx, al
    mov dx, 0x3F8 + 3
    mov al, 0x03
    out dx, al
    mov dx, 0x3F8 + 2
    mov al, 0xC7
    out dx, al
    mov dx, 0x3F8 + 4
    mov al, 0x0B
    out dx, al
    popa
    ret

serial_putc:
    pusha
    mov bl, al
.wait:
    mov dx, 0x3F8 + 5
    in  al, dx
    test al, 0x20
    jz  .wait
    mov dx, 0x3F8 + 0
    mov al, bl
    out dx, al
    popa
    ret

serial_print:
    pusha
.next:
    lodsb
    test al, al
    jz   .done
    call serial_putc
    jmp  .next
.done:
    popa
    ret

; ------------------------------------------------------------
; GDT — flat, ring 0. Code and data segments cover 4 GiB,
; with the 64-bit code segment (L bit) for long mode.
; ------------------------------------------------------------
align 8
gdt_start:
    dq 0                            ; null

gdt_code32:                         ; 0x08: 32-bit code (needed for PM transition)
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b                    ; present, ring0, code, exec/read
    db 11001111b                    ; G=1, D=1, limit high F
    db 0

gdt_data:                           ; 0x10: data (ring 0)
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b                    ; present, ring0, data, read/write
    db 11001111b
    db 0

gdt_code64:                         ; 0x18: 64-bit code
    dw 0
    dw 0
    db 0
    db 10011010b                    ; present, ring0, code, exec/read
    db 10101111b                    ; G=1, L=1 (long mode), D=0, limit high F
    db 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ------------------------------------------------------------
; Data (16-bit segment)
; ------------------------------------------------------------
boot_drive:         db 0
cur_lba:            dd 0
remaining:          dw 0
dst_seg:            dw 0
dst_off:            dw 0

msg_vga:            db "stage2: alive (VGA)", 0
msg_serial:         db "stage2: alive (serial)", 13, 10, 0
msg_kernel_loaded:  db "stage2: kernel staged at 0x10000", 13, 10, 0
msg_a20:            db "stage2: A20 enabled, entering PM", 13, 10, 0
msg_disk_err:       db "stage2: disk read failed", 13, 10, 0

; ------------------------------------------------------------
; 32-bit protected mode entry
; ------------------------------------------------------------
BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Print 'P' to VGA to prove we're in PM (write at 0xB8000 + 4 = row 0 col 2)
    mov word [0xB8000 + 4], 0x0F50     ; 'P' white on black

    ; --- Build PML4 / PDPT / PD ---
    ; PML4[0] -> PDPT
    mov edi, PML4_ADDR
    mov cr3, edi                        ; CR3 = PML4 base (must be set before LME/PG)
    xor eax, eax
    mov ecx, 4096 / 4                   ; clear 4 KiB
    rep stosd                           ; clears PML4

    mov edi, PML4_ADDR
    mov eax, PDPT_ADDR | 0x03           ; present + writable
    mov [edi], eax

    ; PDPT[0] -> PD
    mov edi, PDPT_ADDR
    xor eax, eax
    mov ecx, 4096 / 4
    rep stosd
    mov edi, PDPT_ADDR
    mov eax, PD_ADDR | 0x03
    mov [edi], eax

    ; PD: identity-map 2 MiB huge pages from 0 to 2 GiB (512 entries)
    mov edi, PD_ADDR
    mov eax, 0x83                       ; present + writable + PS (2 MiB page)
    mov ecx, 512
.pd_fill:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .pd_fill

    ; --- Enable PAE (CR4.PAE) ---
    mov eax, cr4
    or  eax, (1 << 5)
    mov cr4, eax

    ; --- Set EFER.LME ---
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1 << 8)                   ; LME
    wrmsr

    ; --- Enable paging (CR0.PG) ---
    mov eax, cr0
    or  eax, (1 << 31)
    mov cr0, eax

    ; --- Load 64-bit code segment and far-jump ---
    jmp 0x18:lm_entry

; ------------------------------------------------------------
; 64-bit long mode entry
; ------------------------------------------------------------
BITS 64
lm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x90000

    ; VGA write: 'L' at 0xB8000 + 6
    mov word [0xB8000 + 6], 0x0F4C

    ; --- Copy kernel from 0x9000 to 0x100000 ---
    mov rsi, KERNEL_STAGE_SEG * 16 + KERNEL_STAGE_OFF   ; 0x9000
    mov rdi, KERNEL_FINAL                                ; 0x100000
    mov ecx, KERNEL_SECS * 512 / 8                       ; qwords
    cld
    rep movsq

    ; VGA write: 'K' at 0xB8000 + 8
    mov word [0xB8000 + 8], 0x0F4B

    ; --- Jump to kernel ---
    mov rax, KERNEL_FINAL
    jmp rax

.hang:
    cli
    hlt
    jmp .hang

; --- Pad to 16 sectors ---
times (16 * 512) - ($ - $$) db 0
