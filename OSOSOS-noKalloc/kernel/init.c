#include <arch.h>
#include <driver/ps2.h>
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <page.h>
//#include <zjunix/bootmm.h>//delte memory management component 
//#include <zjunix/buddy.h>//delte memory management component
#include <zjunix/fs/fat.h>
#include <zjunix/log.h>
#include <zjunix/pc.h>
//#include <zjunix/slab.h>//delte memory management component
#include <zjunix/syscall.h>
#include <zjunix/time.h>
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
// gp: "Global pointer", the base address of static data, 静态数据，共64k
static char dummy_memory_remove_kmalloc_for_PearlShell[32768];
static char dummy_memory_remove_kmalloc_for_time[4096];
void create_startup_process() {
    unsigned int init_gp;
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    /* 
	 * Note:
	 * 0: idle(should come from bootloader)
	 * 1: init 
	 * 2��kthreadd  
	 */ 
    //pc_create(3, ps, (unsigned int)kmalloc(4096) + 4096, init_gp, "PearlShell");
	//here you need to be extremely careful!!!!
	//PearlShell requires a huge amount of stack. Even 4000 is not enough. 
    pc_create(3, ps, (unsigned int)dummy_memory_remove_kmalloc_for_PearlShell + 16384, init_gp, "PearlShell");
	log(LOG_OK, "Shell init");
    //pc_create(4, system_time_proc, (unsigned int)kmalloc(4096) + 4096, init_gp, "time");
	pc_create(4, system_time_proc, (unsigned int)dummy_memory_remove_kmalloc_for_time + 4096, init_gp, "time");
    log(LOG_OK, "Timer init");
}
#pragma GCC pop_options

void init_kernel() {
    kernel_clear_screen(31);
    // Exception
    init_exception();
    // Page table
    init_pgtable();
    // Drivers
    init_vga();
    init_ps2();
    // Memory management
    log(LOG_START, "Memory Modules.");
    //init_bootmm();
    log(LOG_OK, "Bootmem.");
    //init_buddy();
    log(LOG_OK, "Buddy.");
    //init_slab();
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
