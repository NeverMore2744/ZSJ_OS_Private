#include <exc.h>
#include <zjunix/syscall.h>
#include <zjunix/mmu/usr_syscalls.h>
#include <driver/vga.h>
#include "syscall4.h"

sys_fn syscalls[256];

// Exception number 8/32: system call, in the "exceptions" array(function pointer array)
// System call number 4/255: system call number 4
void init_syscall() {
    register_exception_handler(8, syscall);

    // register all syscalls here
    register_syscall(4, syscall4);
    init_usr_syscall();  // 140, 160
}


//Note: syscall is a special kind of exception
//exception num: No.8
void syscall(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int code;
    code = pt_context->v0;    //The code number is stored at $v0
    kernel_printf("%x:\n  ", code);
    pt_context->epc += 4;
    if (syscalls[code]) {
        syscalls[code](status, cause, pt_context);   // call: syscall4 
		                                             // kernel/syscalls.c
    }
}

void register_syscall(int index, sys_fn fn) {
    index &= 255;
    syscalls[index] = fn;
}
