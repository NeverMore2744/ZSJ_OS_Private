
#ifndef _ZSJOS_SYSCALL_H
#define _ZSJOS_SYSCALL_H

#include<zjunix/syscall.h>

void init_syscall();
void syscall(unsigned int status, unsigned int cause, context* pt_context);
void register_syscall(int index, sys_fn fn);

// 128: Change the interrupt vector
void syscall_change_interrupt_vector(unsigned int status, unsigned int cause, context* pt_context);

// 129: Kill the process itself
void syscall_kill_process(unsigned int status, unsigned int cause, context* pt_context);

// 130: Change the foreground and background

// 131: 
void syscall_print_string(unsigned int status, unsigned int cause, context* pt_context);

#endif