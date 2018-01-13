#include <arch.h>
#include <driver/ps2.h>
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <page.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/mmu/Tower.h>
#include <zjunix/mmu/mmu.h>
#include <zjunix/fs/fat.h>
#include <zjunix/log.h>
#include <zjunix/pc.h>
#include <zjunix/slab.h>
#include <zjunix/syscall.h>
#include <zjunix/time.h>
#include<zjunix/mmu/vga_g.h>
#include "../usr/ps.h"

// Print machine information
void machine_info() {
    int row;
    int col;
    kernel_printf("\n%s\n", "ZJUNIX V1.0");
    row = cursor_row;
    col = cursor_col;
    cursor_row = 29;
    kernel_printf("%s", "Created by System Interest Group, Zhejiang University.");
    cursor_row = row;
    cursor_col = col;
    kernel_set_cursor();
}

#pragma GCC push_options
#pragma GCC optimize("O0")

// Create two processes:
// 1. The shell process, "powershell"
// 2. The time process, "time"
// gp: "Global pointer", the base address of static data
void create_startup_process() {
    unsigned int init_gp;
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    /* 
	 * Note:
	 * 0: idle(should come from bootloader)
	 * 1: init   
	 * 2: kthread
	 */ 
    pc_create(3, ps, (unsigned int)kmalloc(4096) + 4096, init_gp, "PearlShell");
    log(LOG_OK, "Shell init");
    pc_create(4, system_time_proc, (unsigned int)kmalloc(4096) + 4096, init_gp, "time");
    log(LOG_OK, "Timer init");
}


void test_slab_and_buddy() {
    unsigned int* k1, *k2, *k3, *k4, *k5;
    unsigned int i=8, j=4;
    for (i=8; i<=4096; i+=j) {
        j+=4;
        k1 = (unsigned int*)kmalloc(i);   kernel_printf("\n  a2-k1=%x, i=%x\n", k1, i);
        kfree(k1);   kernel_printf("\n  f2-k1=%x, i=%x\n", k1, i);
        k1 = (unsigned int*)kmalloc(i+4);   kernel_printf("\n  a2-k1=%x, i=%x\n", k1, i);
        k2 = (unsigned int*)kmalloc(i+8);
        k3 = (unsigned int*)kmalloc(i+12);
        k4 = (unsigned int*)kmalloc(i+16);  kernel_printf("\n  a-k4=%x, i=%x\n", k4, i);
        k5 = (unsigned int*)kmalloc(i+20);  kernel_printf("\n  a-k5=%x, i=%x\n", k5, i);
        kfree(k1);   kernel_printf("\n  f-k1=%x, i=%x\n", k1, i);
        kfree(k2);   kernel_printf("\n  f-k2=%x, i=%x\n", k2, i);
        kfree(k4);   kernel_printf("\n  f-k4=%x, i=%x\n", k4, i);
        k2 = (unsigned int*)kmalloc(i+24);  kernel_printf("\n  a-k2=%x, i=%x\n", k2, i);
        k4 = (unsigned int*)kmalloc(i+28);  kernel_printf("\n  a-k4=%x, i=%x\n", k4, i);
        kfree(k2);   kernel_printf("\n  f-k2=%x, i=%x\n", k2, i);
        kfree(k3);   kernel_printf("\n  f-k3=%x, i=%x\n", k3, i);
        kfree(k4);   kernel_printf("\n  f-k4=%x, i=%x\n", k4, i);
        kfree(k5);   kernel_printf("\n  f-k5=%x, i=%x\n", k5, i);
    }
    return;

    //k2 = (unsigned int*)kmalloc(88);
    //k3 = (unsigned int*)kmalloc(84);
    return;
}

void test_pgtable() {
    pgtable_info();
    kernel_printf("After create a PGT:\n");
    create_PGT(127);
    pgtable_info();
    kernel_printf("After create a PDT:\n");
    create_PDT(127, 0);
    pgtable_info();
    create_PDT_entry(127, 0, 0);
    create_PDT_entry(127, 3, 0);
    kernel_printf("After create 2 entries:\n");
    pgtable_info();
}

void test_g() {
    unsigned int regs_buff[10], regs2_buff[10];
    unsigned int* regs_buff_ptr, *regs2_buff_ptr;
    unsigned int* pic;
    regs_buff_ptr = regs_buff;
    regs2_buff_ptr = regs2_buff;
    regs_buff[0] = 3;
    //regs_buff[1] = (unsigned int)PIC_MONSTER;
    regs_buff[1] = 0xf0f;
    regs_buff[2] = (30 << 16) + 40;
    regs_buff[3] = (40 << 16) + 50;

    cvt2Gmode();
    clear_black();
    pic = get_mario_pic_ptr();
    print_32b_bmp((unsigned char*)pic, 12054, -1, -1);
    return;

    regs_buff[0] = 3;
    //regs_buff[1] = (unsigned int)PIC_MONSTER;
    regs_buff[1] = 0xf0f;
    regs_buff[2] = (30 << 16) + 40;
    regs_buff[3] = (40 << 16) + 50;
    kernel_printf("\n\n   ");
    asm volatile(
        "addu $sp, $sp, -20 \n\t"
        "sw   $v0, 0($sp) \n\t"
        "sw   $a0, 4($sp) \n\t"
        "sw   $a1, 8($sp) \n\t"
        "sw   $a2, 12($sp) \n\t"
        "sw   $a3, 16($sp) \n\t"
        "ori  $v0, $zero, 160 \n\t"
        "lw   $a0, 0(%0) \n\t"
        "lw   $a1, 4(%0) \n\t"
        "lw   $a2, 8(%0) \n\t"
        "lw   $a3, 12(%0) \n\t"
        "syscall \n\t"
        "nop \n\t"
        "nop \n\t"
        "lw   $v0, 0($sp) \n\t"
        "lw   $a0, 4($sp) \n\t"
        "lw   $a1, 8($sp) \n\t"
        "lw   $a2, 12($sp) \n\t"
        "lw   $a3, 16($sp) \n\t"
        "addu $sp, $sp, 20 \n\t"
        : "=r"(regs_buff_ptr)
    );
    kernel_printf("\n\n   ");
    return;
}

#pragma GCC pop_options

void test_buddy_mem(){
    bootmm_message();
    unsigned int* k1, *k2, *k3, *k4, *k5, *k6, *k7, *k8, *k9;
    k1 = (unsigned int*)kmalloc(4096);
    buddy_info();
    k2 = (unsigned int*)kmalloc(4096);
    buddy_info();
    k3 = (unsigned int*)kmalloc(8192);
    buddy_info();
    k4 = (unsigned int*)kmalloc(32769);
    buddy_info();
    k5 = (unsigned int*)kmalloc(70000);
    buddy_info();
    k6 = (unsigned int*)kmalloc(140000);
    buddy_info();
    k7 = (unsigned int*)kmalloc(280000);
    buddy_info();
    k8 = (unsigned int*)kmalloc(560000);
    buddy_info();
    k9 = (unsigned int*)kmalloc(1200000);
    buddy_info();
    return;
    kernel_printf("  After free k1:\n");
    kfree(k1);
    buddy_info();
    kfree(k2);
    kernel_printf("  After free k2:\n");
    buddy_info();
    kernel_printf("  %x\n", k1);
    kernel_printf("  test OK~");
    return;
    k2 = (unsigned int*)kmalloc(8192);
    k3 = (unsigned int*)kmalloc(96);
    kernel_printf("%x\n", k3);
    k4 = (unsigned int*)kmalloc(192);
    kfree(k3);
    kernel_printf("%x\n", k2);
    kernel_printf("%x\n", k4);
    kernel_printf("%x\n", k3);
    k3 = (unsigned int*)kmalloc(96);
    kernel_printf("%x\n", k3);
}

void init_kernel() {
    kernel_clear_screen(31);
    // Exception
    init_exception();
    // TLB
    log(LOG_START, "TLB modules.");
    init_TLB();
    log(LOG_OK, "TLB modules.");
    // Drivers
    init_vga();
    init_ps2();
    // Memory management
    log(LOG_START, "Memory Modules.");
    init_bootmm();
    log(LOG_OK, "Bootmem.");
    init_buddy();
    log(LOG_OK, "Buddy.");
    init_slab();
    log(LOG_OK, "Slab.");
    log(LOG_END, "Memory Modules.");
    // File system
    log(LOG_START, "File System.");
    init_fs();
    log(LOG_END, "File System.");
    // System call
    log(LOG_START, "System Calls.");
    init_syscall();
    log(LOG_END, "System Calls.");
    // Process control
    log(LOG_START, "Process Control Module.");
    init_pc();
    create_startup_process();
    log(LOG_END, "Process Control Module.");
    // MMU modules
    log(LOG_START, "MMU modules.");
    init_MMU();
    log(LOG_END, "MMU modules.");
    // Interrupts
    log(LOG_START, "Enable Interrupts.");
    init_interrupts();
    log(LOG_END, "Enable Interrupts.");
    // Init finished
    machine_info();
    *GPIO_SEG = 0x11223344;
    test_g();
    // Enter shell
    cpu_idle();                 // run idle process
    while (1)
        ;
}
