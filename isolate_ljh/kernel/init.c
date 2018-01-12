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
#pragma GCC pop_options


void test() {
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
    cvt2Gmode();
    clear_black();
    unsigned int i, j, k, w, h, ii, jj, num;
    for (i=0; i<11; i++)
        for (j=0; j<11; j++) {
            num = get_tower_map(0, (j << 3) + (j << 2) - j + i);
            //num = j&3;
            w = 210 + (i << 5) - (i << 1);
            h = 90 + (j << 5) - (j << 1);
            for (ii = w; ii < w+30; ii++)
                for (jj = h; jj < h+30; jj++) 
                    print_pixel(ii, jj,  get_tower_pic(num, (ii-w) + ((jj-h) << 5) - ((jj-h) << 1)));
        }
    return;
}

void test_mem(){
    bootmm_message();
    unsigned int* k1, *k2, *k3, *k4, *k5;
    k1 = (unsigned int*)kmalloc(4096);
    buddy_info();
    k2 = (unsigned int*)kmalloc(4096);
    buddy_info();
    k3 = (unsigned int*)kmalloc(8192);
    buddy_info();
    k4 = (unsigned int*)kmalloc(32769);
    buddy_info();
    kernel_printf("  k1 = %x\n", k1);
    kernel_printf("  k2 = %x\n", k2);
    kernel_printf("  k3 = %x\n", k3);
    kernel_printf("  k4 = %x\n", k4);
    //buddy_info();
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
    test();
    while(1) ;
    // Interrupts
    log(LOG_START, "Enable Interrupts.");
    init_interrupts();
    log(LOG_END, "Enable Interrupts.");
    // Init finished
    machine_info();
    *GPIO_SEG = 0x11223344;
    // Enter shell
    cpu_idle();                 // run idle process
    while (1)
        ;
}
