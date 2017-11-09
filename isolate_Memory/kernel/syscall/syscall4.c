//legacy

#include <arch.h>
#include <zjunix/syscall.h>

#include <driver/vga.h>

int syscall4(unsigned int status, unsigned int cause, context* pt_context) {
    kernel_puts((unsigned char*)pt_context->a0,0xf00,0);
	//kernel_printf("[syscall4] System Call 4\n\n");
	return 0;
}