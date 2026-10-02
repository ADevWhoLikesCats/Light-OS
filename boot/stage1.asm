; ============================================================
; stage1.asm — MBR
;
; Loaded by BIOS at 0x0000:0x7C00 in 16-bit real mode.
; Job: load stage2 (16 sectors starting at LBA 1) into
;      0x0000:0x8000, then far-jump to it.
;
; Constraints:
;   - Must fit in 512 bytes, ending with 0x55 0xAA.
;   - Cannot assume DS/ES/SS have sane values.
;   - INT 13h uses CHS on the wire, so we compute it by hand.
; ============================================================

BITS 16
ORG 0x7C00

STAGE2_SEG  equ 0x0000
STAGE2_OFF  equ 0x8000
STAGE2_LBA  equ 1
STAGE2_SECS equ 16          ; must match Makefile's STAGE2_SECTORS

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; stack grows down from just below us
    sti

    ; BIOS passes boot drive in DL. Save it — INT 13h resets may clobber it.
    mov [boot_drive], dl

    ; --- Reset disk controller ---
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    jc  disk_error

    ; --- Read stage2 via CHS ---
    ; LBA 1 on a 1.44 MB floppy: C=0, H=0, S=2 (sectors are 1-based on the wire).
    ; 16 sectors fit within one track (18 sectors/track), so no track crossing.
    mov ax, STAGE2_SEG
    mov es, ax              ; ES:BX = 0x0000:0x8000
    mov bx, STAGE2_OFF

    mov ah, 0x02            ; BIOS: read sectors
    mov al, STAGE2_SECS
    mov ch, 0               ; cylinder 0
    mov cl, STAGE2_LBA + 1  ; sector (1-based)
    mov dh, 0               ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc  disk_error

    ; --- Stage1 verified: we ran, we read the disk ---
    mov si, msg_stage1_ok
    call print

    ; --- Hand off to stage2 ---
    ; Far jump = set CS:IP atomically. `jmp seg:off` in NASM emits this.
    jmp STAGE2_SEG:STAGE2_OFF

; --- Error path ---
disk_error:
    mov si, msg_disk_err
    call print
    cli
    hlt

; --- print: SI = null-terminated string, BIOS teletype (INT 10h AH=0Eh) ---
print:
    pusha
.next:
    lodsb                   ; AL = [DS:SI], SI++
    test al, al
    jz   .done
    mov  ah, 0x0E
    mov  bx, 0x0007         ; page 0, light gray
    int  0x10
    jmp  .next
.done:
    popa
    ret

; --- Data ---
boot_drive:    db 0
msg_stage1_ok: db "stage1: loaded stage2, jumping", 13, 10, 0
msg_disk_err:  db "stage1: disk read failed", 13, 10, 0

; --- Boot signature ---
times 510 - ($ - $$) db 0
dw 0xAA55
