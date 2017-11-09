#include "exc.h"

#include <driver/vga.h>
#include <zjunix/pc.h>

#pragma GCC push_options
#pragma GCC optimize("O0")

exc_fn exceptions[32];   //function ptr
                         //typedef void (*exc_fn)(unsigned int, unsigned int, context*);

void do_exceptions(unsigned int status, unsigned int cause, context* pt_context) {
    int index = cause >> 2;  //CPO reg 13(CAUSE) : CAUSE[2:6]  Exc_Code[4:0]
    index &= 0x1f;           //5 valid bits, from 1 to 32
    if (exceptions[index]) {
        exceptions[index](status, cause, pt_context);       //call kernel/syscall.c
    } else {
        task_struct* pcb;
        unsigned int badVaddr;
        asm volatile("mfc0 %0, $8\n\t" : "=r"(badVaddr));   // "=r": constrain
                                                            // the gcc can chose reg %0 arbitarily, as long as the badVaddr is also using the same reg.
        pcb = get_curr_pcb();
        kernel_printf("\nProcess %s exited due to exception cause=%x;\n", pcb->name, cause);
        kernel_printf("status=%x, EPC=%x, BadVaddr=%x\n", status, pcb->context.epc, badVaddr);
        pc_kill_syscall(status, cause, pt_context);
        while (1)
            ;
    }
}

void register_exception_handler(int index, exc_fn fn) {
    index &= 31;
    exceptions[index] = fn;
}

void init_exception() {
    // status 0000 0000 0000 0000 0000 0000 0000 0000
    // cause 0000 0000 1000 0000 0000 0000 0000 0000
    asm volatile(
        "mtc0 $zero, $12\n\t"     //REG 12: STATUS = 0x8000_0000; 
		                          //STATUS[22] = BEV  0, exceptuion base address = 0x8000_0000
        "li $t0, 0x800000\n\t"    //
        "mtc0 $t0, $13\n\t");     //REG 13: CAUSE = 0x0080_0000
                                  //CAUSE[23] = IV = 1, interrupt base address 0x8000_0200
}

#pragma GCC pop_options
