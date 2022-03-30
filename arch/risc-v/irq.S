/*
  RISC-V / RV32 interrupt support code
  Based on the RISC-V proxy kernel
*/

.section .text

.global trap_entry
trap_entry:
    addi   sp,sp,-320
    sw     ra,4(sp)
    sw     gp,12(sp)
    sw     tp,16(sp)
    sw     t0,20(sp)
    sw     t1,24(sp)
    sw     t2,28(sp)
    sw     s0,32(sp)
    sw     s1,36(sp)
    sw     a0,40(sp)
    sw     a1,44(sp)
    sw     a2,48(sp)
    sw     a3,52(sp)
    sw     a4,56(sp)
    sw     a5,60(sp)
    sw     a6,64(sp)
    sw     a7,68(sp)
    sw     s2,72(sp)
    sw     s3,76(sp)
    sw     s4,80(sp)
    sw     s5,84(sp)
    sw     s6,88(sp)
    sw     s7,92(sp)
    sw     s8,96(sp)
    sw     s9,100(sp)
    sw     s10,104(sp)
    sw     s11,108(sp)
    sw     t3,112(sp)
    sw     t4,116(sp)
    sw     t5,120(sp)
    sw     t6,124(sp)
    csrr   t0,mscratch
    csrr   s0,mstatus
    csrr   t1,mepc
    csrr   t2,mbadaddr
    csrr   t3,mcause
    sw     t0,8(sp)
    sw     s0,128(sp)
    sw     t1,132(sp)
    sw     t2,136(sp)
    sw     t3,140(sp)
    li     t0,-1
    sw     t0,144(sp)
    mv     a0,sp

    la     t0, jump_table
    sll    t3, t3, 2
    add    t0, t0, t3
    lw     t0, 0(t0)
    la     ra, jump_table_return
    jr     t0

jump_table:
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word spll_irq_entry
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler
    .word undefined_handler

jump_table_return:

    lw     t1,128(sp)
    lw     t2,132(sp)
    csrw   mepc,t2
    lw     ra,4(sp)
    lw     sp,8(sp)
    lw     gp,12(sp)
    lw     tp,16(sp)
    lw     t0,20(sp)
    lw     t1,24(sp)
    lw     t2,28(sp)
    lw     s0,32(sp)
    lw     s1,36(sp)
    lw     a1,44(sp)
    lw     a2,48(sp)
    lw     a3,52(sp)
    lw     a4,56(sp)
    lw     a5,60(sp)
    lw     a6,64(sp)
    lw     a7,68(sp)
    lw     s2,72(sp)
    lw     s3,76(sp)
    lw     s4,80(sp)
    lw     s5,84(sp)
    lw     s6,88(sp)
    lw     s7,92(sp)
    lw     s8,96(sp)
    lw     s9,100(sp)
    lw     s10,104(sp)
    lw     s11,108(sp)
    lw     t3,112(sp)
    lw     t4,116(sp)
    lw     t5,120(sp)
    lw     t6,124(sp)
    lw     a0,40(sp)
    addi   sp,sp,320
    mret

.weak undefined_handler
undefined_handler:
    j      undefined_handler

