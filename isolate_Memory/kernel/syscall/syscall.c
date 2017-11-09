#include <exc.h>
#include <zjunix/syscall.h>
#include "syscall4.h"
#include <driver/vga.h>

#include <auxillary/unistd.h>

//for registering syscall
#include <zjunix/pc.h>
#include <victor/sched.h>

/*
  *** 致命：MIPS编译器的错误 ***
以下办法会产生错误，原因是内嵌汇编导致$v0绑定错误。

	Fatal Warnnig: $a0 is not correct
	It seems that only li is reliable.
	
	asm volatile(                                //try system call
        "li $v0, 4\n\t"   // $v0: system call code
		"li $a0, 0\n\t"
		"add $a0, %0,$a0\n\t"// $a0: 传入参数, mov $a0, %0
		"syscall\n\t"
        :      //output
		: "r"(iAddressOfString)		//input
        );

希望获得正确结果的解决办法应该是：
		
	To get right answer, you should write:
	unsigned int iAddressOfString = 100;
	asm volatile(                                //try system call
		"li $a0, 0\n\t"
		"add $a0, %0,$a0\n\t"// $a0: 传入参数, mov $a0, %0
		
        :      //output
		: "r"( iAddressOfString )		//input
        );
	asm volatile(                                //try system call
        "li $v0, 4\n\t"   // $v0: system call code
		"syscall\n\t"
        );		
*/

/*
 **** 系统调用的重构:系统调用的静态注册 ****
办法：直接初始化静态函数指针数组

以4号中断为例
旧办法为
register_syscall(4, syscall4);
其中4为系统调用号，syscall4为处理函数的函数指针。
存在以下缺点：
•	使得程序冗长，添加系统调用的负担重
•	注册时机十分重要，稍不注意就出错
•	可读性差
改进后的做法为
直接初始化静态函数指针数组

//  [宏定义：系统调用号]    sys_函数指针,
[SYS_printstring]       syscall4,
其中syscall4为处理函数的函数指针，SYS_printstring 为一个宏定义
#define SYS_printstring      50
宏定义在unistd.h中
存在以下优点：
•	程序简洁，编程方便
•	由编译器保证注册时机
•	可读性好，以表格形式呈现系统调用
*/
//typedef void (*sys_fn)(unsigned int status, unsigned int cause, context* pt_context);
//sys_fn syscalls[256];
static int (*syscalls[])(unsigned int status, unsigned int cause, context* pt_context) = {
//	[宏定义：系统调用号]    sys_函数指针,
	
	[SYS_printstring]       syscall4,//see unistd.h
    [SYS_kill]       pc_kill_syscall,//see unistd.h, legacy, <zjunix/pc.h>
    [SYS_RE_schedule]       pc_re_schedule_syscall,//see unistd.h, legacy, <zjunix/pc.h>
/*
    [SYS_exit]              sys_exit,//see unistd.h
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
    [SYS_gettime]           sys_gettime,
    [SYS_lab6_set_priority] sys_lab6_set_priority,
    [SYS_sleep]             sys_sleep,
    [SYS_open]              sys_open,
    [SYS_close]             sys_close,
    [SYS_read]              sys_read,
    [SYS_write]             sys_write,
    [SYS_seek]              sys_seek,
    [SYS_fstat]             sys_fstat,
    [SYS_fsync]             sys_fsync,
    [SYS_getcwd]            sys_getcwd,
    [SYS_getdirentry]       sys_getdirentry,
    [SYS_dup]               sys_dup,
*/	
};

// Exception number 8/32: system call, in the "exceptions" array(function pointer array)
// System call number 4/255: system call number 4
void init_syscall() {
    register_exception_handler(8, syscall);

    // register all syscalls here
    // register_syscall(4, syscall4);
}


//Note: syscall is a special kind of exception
//exception num: No.8
void syscall(unsigned int status, unsigned int cause, context* pt_context) {
    unsigned int code;
    code = pt_context->v0;    //??????????ú?? ???????????? $V0 
    pt_context->epc += 4;
	//kernel_printf("[syscall]: $v0:%d. \n",pt_context->v0);
	//kernel_printf("[syscall]: $a0:%d. \n",pt_context->a0);
    if (syscalls[code]) {
        syscalls[code](status, cause, pt_context);   // call: syscall4 
		                                             // kernel/syscalls.c
    }
}
/*
void register_syscall(int index, sys_fn fn) {
    index &= 255;
    syscalls[index] = fn;
}
*/