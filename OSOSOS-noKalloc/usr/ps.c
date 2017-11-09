//it seems that "ps" is the abbreviation of "Power shell". This file is mainly about the implementatin of shell.

#include "ps.h"
//call the corresponding kernel function
#include <driver/ps2.h>
#include <driver/sd.h>
#include <driver/vga.h>
#include <zjunix/bootmm.h>
#include <zjunix/buddy.h>
#include <zjunix/fs/fat.h>
#include <zjunix/slab.h>
#include <zjunix/time.h>
#include <zjunix/utils.h>
#include "../usr/ls.h"
#include "exec.h"
#include "myvi.h"

char ps_buffer[64];
int ps_buffer_index;

void test_proc() {
    unsigned int timestamp;
    unsigned int currTime;
    unsigned int data;
    asm volatile("mfc0 %0, $9, 6\n\t" : "=r"(timestamp));
    data = timestamp & 0xff;
    while (1) {
        asm volatile("mfc0 %0, $9, 6\n\t" : "=r"(currTime));
        if (currTime - timestamp > 100000000) {
            timestamp += 100000000;
            *((unsigned int *)0xbfc09018) = data;
        }
    }
}

int proc_demo_create() {
    int asid = pc_peek();
    if (asid < 0) {
        kernel_puts("Failed to allocate pid.\n", 0xfff, 0);
        return 1;
    }
    unsigned int init_gp;
    asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
    pc_create(asid, test_proc, (unsigned int)kmalloc(4096), init_gp, "test");
    return 0;
}

void ps() {
    kernel_printf("Press any key to enter shell.\n");
    
	/*
function: keep calling the subfunction until it gets a valid ascii code (rather than scan code)
Special Note: rolling? blocked? 
    do {
        key = kernel_scantoascii(kernel_getkey());
        sleep(1000);
    } while (key == -1);
*/
	kernel_getchar();    //the first key would be discarded
	
    char c;
    ps_buffer_index = 0;
    ps_buffer[0] = 0;
    kernel_clear_screen(31);
    kernel_puts("PowerShell\n", 0xfff, 0);   //the name 
    kernel_puts("PS>", 0xfff, 0);            //prompt
    while (1) {
        c = kernel_getchar();
        if (c == '\n') {
            ps_buffer[ps_buffer_index] = 0;  //insert '\0'
            if (kernel_strcmp(ps_buffer, "exit") == 0) {     //is exit?
                ps_buffer_index = 0;         //reset buffer
                ps_buffer[0] = 0;            //insert '\0' at the beginning
                kernel_printf("\nPowerShell exit.\n");
            } else
                parse_cmd();                 //parse and execute
            ps_buffer_index = 0;             //reset buffer
            kernel_puts("PS>", 0xfff, 0);    //new round begin: a new prompt
        } else if (c == 0x08) {              //ascii:0x08 back space
            if (ps_buffer_index) {           // if buffer is not empty
                ps_buffer_index--;           //backspace buffer ptr
                kernel_putchar_at(' ', 0xfff, 0, cursor_row, cursor_col - 1);//erase on the screen
                cursor_col--;                //cursor
                kernel_set_cursor();         //
            }
        } else {
            if (ps_buffer_index < 63) {      //prohibit overflow
                ps_buffer[ps_buffer_index++] = c;  //insert into the buffer
                kernel_putchar(c, 0xfff, 0);       //vga display
            }
        }
    }
}

void parse_cmd() {
    unsigned int result = 0;
    char dir[32];
    char c;
    kernel_putchar('\n', 0, 0);
    char sd_buffer[8192];
    int i = 0;
    char *param;
    for (i = 0; i < 63; i++) {
        if (ps_buffer[i] == ' ') {  //detect blank space
            ps_buffer[i] = 0;       //replace blank space with '\0' 
            break;                  //only the first blank space
        }
    }
    if (i == 63)
        param = ps_buffer;  //no blank space? reset?
    else //i!=63
        param = ps_buffer + i + 1;
		
		//e.g.
		//abc def ghi
		//^
		
		//abc\0def ghi
        //     ^
		
    if (ps_buffer[0] == 0) { //end of sequence
        return;
    } else if (kernel_strcmp(ps_buffer, "clear") == 0) {
        kernel_clear_screen(31);
    } else if (kernel_strcmp(ps_buffer, "echo") == 0) {
        kernel_printf("%s\n", param);
		//e.g.
		//echo abc def
		//abc def
		
    } else if (kernel_strcmp(ps_buffer, "gettime") == 0) {
        char buf[10];
        get_time(buf, sizeof(buf));
        kernel_printf("%s\n", buf);
    } else if (kernel_strcmp(ps_buffer, "sdwi") == 0) {  //write sd card 0,1,2,3,...,511->SD card addr 7 (1 block)
        for (i = 0; i < 512; i++)
            sd_buffer[i] = i;
        sd_write_block(sd_buffer, 7, 1);
		//u32 sd_write_block(unsigned char *buf, unsigned long addr, unsigned long count);
		//count: write single block or multiple blocks
		
        kernel_puts("sdwi\n", 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "sdr") == 0) {  //read SD card addr 7 (1 block) and display
        sd_read_block(sd_buffer, 7, 1);
		//u32 sd_read_block(unsigned char *buf, unsigned long addr, unsigned long count);
		
        for (i = 0; i < 512; i++) {
            kernel_printf("%d ", sd_buffer[i]);
        }
        kernel_putchar('\n', 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "sdwz") == 0) {  ////write sd card all zeros ->SD card addr 7 (1 block)
        for (i = 0; i < 512; i++) {
            sd_buffer[i] = 0;
        }
        sd_write_block(sd_buffer, 7, 1);
        kernel_puts("sdwz\n", 0xfff, 0);
    } else if (kernel_strcmp(ps_buffer, "mminfo") == 0) {
        bootmap_info("bootmm");
        buddy_info();
    } /*else if (kernel_strcmp(ps_buffer, "mmtest") == 0) {
        kernel_printf("kmalloc : %x, size = 1KB\n", kmalloc(1024));
    }*/ else if (kernel_strcmp(ps_buffer, "ps") == 0) {
        result = print_proc();
        kernel_printf("ps return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "kill") == 0) {
        int pid = param[0] - '0';
        kernel_printf("Killing process %d\n", pid);
        result = pc_kill(pid);
        kernel_printf("kill return with %d\n", result);
    } /*else if (kernel_strcmp(ps_buffer, "time") == 0) {
        unsigned int init_gp;
        asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
        pc_create(2, system_time_proc, (unsigned int)kmalloc(4096), init_gp, "time");
    }*/ else if (kernel_strcmp(ps_buffer, "proc") == 0) {
        result = proc_demo_create();
        kernel_printf("proc return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "cat") == 0) {
        result = fs_cat(param);
        kernel_printf("cat return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "ls") == 0) {
        result = ls(param);
        kernel_printf("ls return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "vi") == 0) {
        result = myvi(param);
        kernel_printf("vi return with %d\n", result);
    } else if (kernel_strcmp(ps_buffer, "exec") == 0) {
        result = exec(param);
        kernel_printf("exec return with %d\n", result);
    } else {
        kernel_puts(ps_buffer, 0xfff, 0);
        kernel_puts(": command not found\n", 0xfff, 0);
    }
}
