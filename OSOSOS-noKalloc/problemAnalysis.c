//this is a note book
     //void do_interrupts(unsigned int status, unsigned int cause, context* pt_context) 
     //start.s , k0,k1
    move $k1, $sp
	la  $k0, kernel_sp	
	j exception_save_context
	lw  $sp, 0($k0)
	
	 
	 //pc.h
	 copy_context(pt_context, &(pcb[curr_proc].context));
     
    if (i == 8) {                           //why?
        kernel_puts("Error: PCB[0] is invalid!\n", 0xfff, 0);
        while (1) //commite suicide 
            ;
    }
    
    copy_context(&(pcb[curr_proc].context), pt_context);
    
