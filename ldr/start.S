! Cast Menu loader entry (position: 0x8CE00000). Mirrors GDMENU loader.bin's
! stub: disable cache via CCR, run from the P2 (uncached) alias, clear .bss.
    .section .text.start, "ax"
    .global _start
_start:
    mov.l   ccr_addr, r0
    mov.w   ccr_val, r1
    mov.l   r1, @r0            ! CCR = 0x0909: flush + enable (uncached run is safe)
    mov.l   main_p2, r0
    nop; nop; nop; nop; nop; nop
    jmp     @r0
    nop
    .align 2
ccr_addr:   .long 0xFF00001C
main_p2:    .long _stage2 + 0x20000000
ccr_val:    .word 0x0909
    .align 2
_stage2:
    mov.l   bss_start, r0
    mov.l   bss_end, r2
    sub     r0, r2
    shlr2   r2
    tst     r2, r2             ! empty .bss: skip (dt on 0 would wrap and wipe all of RAM)
    bt      2f
    mov     #0, r1
1:  mov.l   r1, @r0
    dt      r2
    bf/s    1b
    add     #4, r0
2:  mov.l   stack_top, r15
    mov.l   main_addr, r0
    jmp     @r0
    nop
    .align 2
bss_start:  .long __bss_start
bss_end:    .long _end
stack_top:  .long 0x8D000000
main_addr:  .long _loader_main

! bios_enter(r4 = media flag): identical register image to GDMENU's loader
! (sp, GBR, VBR, SR, DBR, FPSCR, PR) then jump to the BIOS boot routine.
    .global _bios_enter
_bios_enter:
    mov     r4, r8
    mov.l   be_sp, r15
    mov.l   be_gbr, r0
    ldc     r0, gbr
    mov.l   be_vbr, r1
    mov.l   be_sr, r4
    ldc     r1, vbr
    mov.l   be_target, r14
    mov.l   be_fpscr, r5
    lds     r5, fpscr
    mov #0, r0; mov #0, r1; mov #0, r2; mov #0, r3; mov #0, r5; mov #0, r6; mov #0, r7
    ldc     r4, sr
    mov.l   be_dbr, r3
    ldc     r3, dbr
    mov     #0, r0
    mov     r8, r1
    mov #0, r2; mov #0, r3; mov #0, r4; mov #0, r5; mov #0, r6; mov #0, r7; mov #0, r8
    mov.l   be_pr, r12
    mov #0, r9
    lds     r12, pr
    mov #0, r10; mov #0, r11; mov #0, r12; mov #0, r13
    clrmac
    clrt
    jmp     @r14
    mov     #0, r14
    .align 2
be_sp:      .long 0x8D000000
be_gbr:     .long 0x8C000000
be_vbr:     .long 0x8C000000
be_dbr:     .long 0x8C000010
be_sr:      .long 0x700000F0
be_target:  .long 0x8C00B500
be_fpscr:   .long 0x00040001
be_pr:      .long 0x8CE00000

! ---- cheat engine: hooked in front of the GD-ROM syscall vector (0x8C0000BC), which every game
! library calls regularly. Walks a table of (code, value) pairs and performs the constant writes
! (CodeBreaker types 0/1/2 = 8/16/32-bit), then jumps to the original syscall. Position
! independent: ldr.c copies these bytes to CHEAT_STUB and the table to CHEAT_TABLE.
    .align 2
    .global _cheat_hook_start, _cheat_hook_end
_cheat_hook_start:
    mov.l   r8, @-r15
    mov.l   r9, @-r15
    mov.l   r10, @-r15
    mov.l   r11, @-r15
    mov.l   r12, @-r15
    mov.l   r13, @-r15
    mov.l   ch_table, r8
    mov.l   ch_mask, r12
    mov.l   ch_base, r13
1:  mov.l   @r8+, r9            ! code word: type in the top nibble, address in the low 24 bits
    tst     r9, r9
    bt      9f                  ! 0 terminates the table
    mov.l   @r8+, r10           ! value
    mov     r9, r11
    shlr16  r11
    shlr8   r11
    shlr2   r11
    shlr2   r11                 ! r11 = type
    and     r12, r9
    or      r13, r9             ! r9 = 0x8C000000 | addr
    tst     r11, r11
    bt      5f
    dt      r11
    bt      6f
    dt      r11
    bt      7f
    bra     1b                  ! other code types: ignored
    nop
5:  mov.b   r10, @r9
    bra     1b
    nop
6:  mov.w   r10, @r9
    bra     1b
    nop
7:  mov.l   r10, @r9
    bra     1b
    nop
9:  mov.l   @r15+, r13
    mov.l   @r15+, r12
    mov.l   @r15+, r11
    mov.l   @r15+, r10
    mov.l   @r15+, r9
    mov.l   @r15+, r8
    mov.l   ch_orig, r0
    mov.l   @r0, r0
    jmp     @r0                 ! original GD syscall entry (saved by ldr.c)
    nop
    .align 2
ch_table:   .long 0x8C003D80
ch_mask:    .long 0x00FFFFFF
ch_base:    .long 0x8C000000
ch_orig:    .long 0x8C003D7C
_cheat_hook_end:
