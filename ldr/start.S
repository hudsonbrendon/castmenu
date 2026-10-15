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
