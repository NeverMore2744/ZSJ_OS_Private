//private implementation

#include "pc.h"

#include <driver/vga.h>
#include <intr.h>
#include <zjunix/syscall.h>
#include <zjunix/utils.h>
#include <assert.h>

//need to allocate memory
#include <zjunix/slab.h>
#include <auxillary/defs.h>
#include <auxillary/error.h>

/*
 * 为了管理系统中所有的进程控制块， 维护了如下全局变量
 */
task_struct pcb[MAX_PID];

/* current proc
 * 当前占用 CPU，处于“运行”状态进程控制块指针。
 * 通常这个变量是只读的，只有在进程切换的时候才进行修改，
 * 并且整个切换和修改过程需要保证操作的原子性，至少需要屏蔽中断，
 */
int curr_proc = -1;

 

// idle proc
int idleproc = -1;//NULL;//or use type int ?

/* init proc
 * static struct proc *initproc;  //指向第一个用户态进程（proj10 以后）
 */
int initproc = -1;//NULL;


static void copy_context(context* src, context* dest) {
    dest->epc = src->epc;
    dest->at = src->at;
    dest->v0 = src->v0;
    dest->v1 = src->v1;
    dest->a0 = src->a0;
    dest->a1 = src->a1;
    dest->a2 = src->a2;
    dest->a3 = src->a3;
    dest->t0 = src->t0;
    dest->t1 = src->t1;
    dest->t2 = src->t2;
    dest->t3 = src->t3;
    dest->t4 = src->t4;
    dest->t5 = src->t5;
    dest->t6 = src->t6;
    dest->t7 = src->t7;
    dest->s0 = src->s0;
    dest->s1 = src->s1;
    dest->s2 = src->s2;
    dest->s3 = src->s3;
    dest->s4 = src->s4;
    dest->s5 = src->s5;
    dest->s6 = src->s6;
    dest->s7 = src->s7;
    dest->t8 = src->t8;
    dest->t9 = src->t9;
    dest->hi = src->hi;
    dest->lo = src->lo;
    dest->gp = src->gp;
    dest->sp = src->sp;
    dest->fp = src->fp;
    dest->ra = src->ra;
}

/*
// alloc_proc - alloc a proc_struct and init all fields of proc_struct
int 
alloc_proc(void) {
	int proc_struct_asid = -1;//assume it is invalid at first.
	proc_struct_asid  = pc_peek();//find a valid ASID
	assert(proc_struct_asid < MAX_PID, "[alloc_proc]: Trying to create asid>=MAX_PID");  // Ensure that asid is not too big
	
	pcb[proc_struct_asid].ASID = -1;//set invalid
	pcb[proc_struct_asid].state = PROC_UNINIT;
	pcb[proc_struct_asid].need_resched = 0;
	pcb[proc_struct_asid].runs = 0;
	pcb[proc_struct_asid].need_resched = 0;
	pcb[proc_struct_asid].parent = 0;//NULL;
	
	return proc_struct_asid;
}
*/

// alloc_proc - alloc a task_struct and init all fields of task_struct
static task_struct *
alloc_proc(void) {
    task_struct *proc = kmalloc(sizeof( task_struct ));//struct task_struct *, sizeof(struct task_struct)
    if (proc != NULL) {
    //STEP 1
    /*
     * below fields in task_struct need to be initialized
     *       enum proc_state state;                      // Process state
     *       int pid;                                    // Process ID
     *       int runs;                                   // the running times of Proces
     *       uintptr_t kstack;                           // Process kernel stack
     *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
     *       struct task_struct *parent;                 // the parent process
     *       struct mm_struct *mm;                       // Process's memory management field
     *       struct context context;                     // Switch here to run process
     *       struct trapframe *tf;                       // Trap frame for current interrupt
     *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
     *       uint32_t flags;                             // Process flag
     *       char name[PROC_NAME_LEN + 1];               // Process name
     */
	    
		//void* kernel_memset(void* dest, int b, int len);
        kernel_memset(&(proc->context), 0, sizeof(context));//struct context
		proc->state = PROC_UNINIT;
        proc->ASID = -1;//proc->pid = -1;
        proc->runs = 0;
        proc->context.sp = 0;//init_sp;// proc->kstack = 0;
		proc->context.gp = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;//not used
        proc->tf = NULL;//not used
        //proc->cr3 = boot_cr3;
        proc->flags = 0;
        kernel_memset(proc->name, 0, PROC_NAME_LEN);
		//unique
		proc->counter = 0;//PROC_DEFAULT_TIMESLOTS
		proc->start_time = 0;
		
    }
	else{
		assert(0, "[alloc_proc]: kmalloc failed. It returned NULL. ");  //assertion must fail
	}
		
    return proc;
}




void init_pc() {
    int i;
    for (i = 1; i < MAX_PID; i++)
	{
		pcb[i].ASID = -1;//set invalid
		pcb[i].state = PROC_UNINIT;
		pcb[i].need_resched = 0;
		pcb[i].runs = 0;
		pcb[i].need_resched = 0;
		pcb[i].parent = 0;//NULL;
	}
        
	
	//--------- initialize the first process --------------//
	//should be performed by alloc_proc 
	pcb[0].state = PROC_UNINIT; //设置进程为“初始”态
	pcb[0].ASID = -1;           //进程的 pid还没设置好
	/*
	 * 语句给了 idleproc 合法的身份证号--0，
	 * 这名正言顺地表明了 idleproc 是第 0 个内核线程。
	 * “0”是第一个的表示方法是计算机领域所特有的，
	 * 比如 C语言定义的第一个数组元素的小标也是“0”。
	 */
	 
    pcb[0].ASID = 0;
    
	pcb[0].counter = PROC_DEFAULT_TIMESLOTS;     //Step 1. initilize all the PCBs
    kernel_strcpy(pcb[0].name, "idle");          //Step 2. construct the idle process
    curr_proc = 0;                               //        which process is running currently
    
	//idleproc->pid = 0;//ASID
	/*
	 * 第二条语句改变了 idleproc 的状态，
	 * 使得它从 “出生”转到了“准备工作”，就差调度它执行了。
	 */
    pcb[0].state = PROC_RUNNABLE;
    //idleproc->kstack = (uintptr_t)bootstack;
    
	//very dangerous
	//pcb[0].need_resched = 1;//boolean
	
	idleproc = 0;//idle asid = 0
	/*
	 * 第 0 个内核线程主要工作是完成内核中各个子系统的初始化，
	 * 然后就通过执行 cpu_idle函数开始过退休生活了。
	 * 但接下来还需创建其他进程来完成各种工作，但 idleproc 自己不想做，
	 * 于是就通过调用 kernel_thread 函数创建了一个内核线程 init_main。
	 */
	//create init
	//int pid = kernel_thread(init_main, NULL, 0);
	//static int
    //init_main(void *arg) {
	
	
	register_syscall(10, pc_kill_syscall);       //Step 3. add the "kill" syscall, index = 10
    register_interrupt_handler(7, pc_schedule);  //Step 4. register the timer interrupt. 
	                                             //        function protocal: void register_interrupt_handler(int index, intr_fn fn)
												 //        Once interrupt, it would call pc_schedule(a function ptr)

    asm volatile(                                //Step 5.  sett the size of time slice
                                // Volume 2: The Compare register acts in conjunction with 
                                //          the Count register to implement a timer and 
                                //          timer interrupt function.
        "li $v0, 1000000\n\t"   // $11(Compare)与$9(Counter)共同完成计时与定时中断功能
        "mtc0 $v0, $11\n\t"     // $11固定，存储当前时间片的大小
        "mtc0 $zero, $9");

}

//usage: register_interrupt_handler(7, pc_schedule);
//it seeems that it would handle the timer interrupt.
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
    //Step 1: Save context
    copy_context(pt_context, &(pcb[curr_proc].context));
	
	//-------------------------------   KEY PART - advanced algorithm and data structure would be put here in the future  ----------------------------------
	
    int i;
    for (i = 0; i < MAX_PID; i++) {
        curr_proc = (curr_proc + 1) & 7;
        if (pcb[curr_proc].ASID >= 0)
            break;
    }
    if (i == MAX_PID) {
        kernel_puts("Error: PCB[0] is invalid!\n", 0xfff, 0);
        while (1)
            ;
    }
	
	//-----------------------------------------------------------------
	
    // Load context
    copy_context(&(pcb[curr_proc].context), pt_context);
    asm volatile("mtc0 $zero, $9\n\t");
}

int pc_peek() {
    int i = 0;
    for (i = 0; i < MAX_PID; i++)
        if (pcb[i].ASID < 0)
            break;
    if (i == MAX_PID)
	{
		assert(0, "[pc_peek]: No available ASID");  //assertion must fail
		return -1;
	}
        
    return i;
}

//old function:
// 创建进程的实质就是初始化一个PCB
// 只初始化epc, sp, gp, 进程名称, ASID?
void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name) {
    //assert: valid asid 
	assert(asid < MAX_PID, "[pc_create]: Trying to create asid>=MAX_PID");  // Ensure that asid is not too big
    assert(pcb[asid].ASID < 0, "[pc_create]: ASID already created.");  //
	
	pcb[asid].context.epc = (unsigned int)func;
    pcb[asid].context.sp = init_sp;
    pcb[asid].context.gp = init_gp;	
    kernel_strcpy(pcb[asid].name, name);
    pcb[asid].ASID = asid;
	
	pcb[asid].runs = 0;
	
}


// 杀死进程的实质就是废掉一个PCB
// 杀死的是当前正在运行的进程，把ASID置为无效(-1)
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    if (curr_proc != 0) {
        pcb[curr_proc].ASID = -1;
        pc_schedule(status, cause, pt_context);
    }
}

int pc_kill(int proc) {
    
	//we need assert
	assert(proc < MAX_PID, "[pc_kill]: Trying to kill proc>=MAX_PID");  // Ensure that the buffer is more than 8 bytes
    
	proc &= (MAX_PID - 1);//unsafe at all
	if (proc != 0 && pcb[proc].ASID >= 0) {
        pcb[proc].ASID = -1;
        return 0;
    } else if (proc == 0)
        return 1;
    else
        return 2;
}

// 当前正在运行的进程的PCB指针
task_struct* get_curr_pcb() {
    return &pcb[curr_proc];
}

// 打印进程信息
int print_proc() {
    int i;
    kernel_puts("PID name\n", 0xfff, 0);
    for (i = 0; i < MAX_PID; i++) {
        if (pcb[i].ASID >= 0)
            kernel_printf(" %x  %s\n", pcb[i].ASID, pcb[i].name);
    }
    return 0;
}

//------  new function added below by LIANG weixin   -----------//
// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
// idle process 的执行主体是 cpu_idle 函数
void
cpu_idle(void) {
    while (1) {
        //if (current->need_resched) {
            //schedule();//would not run idle process.
        //}
    }
}

//new function: note that the function has different return type.
// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
int
kernel_thread(int (*fn)(void *), void *arg, /*uint32_t*/int clone_flags) {
    /*
	struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;
	*/
	
    //return do_fork(clone_flags | CLONE_VM, 0, &tf);
	return 0;
}


/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */

int
do_fork(uint32_t clone_flags, uintptr_t stack, trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    /*
	struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
	*/
	
	
    //LAB4:EXERCISE2 YOUR CODE
    //LAB8:EXERCISE2 YOUR CODE  HINT:how to copy the fs in parent's proc_struct?
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

	//LAB5 YOUR CODE : (update LAB4 steps)
   /* Some Functions
    *    set_links:  set the relation links of process.  ALSO SEE: remove_links:  lean the relation links of process 
    *    -------------------
	*    update step 1: set child proc's parent to current process, make sure current process's wait_state is 0
	*    update step 5: insert proc_struct into hash_list && proc_list, set the relation links of process
    */
	
	/*
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }

    proc->parent = current;
    assert(current->wait_state == 0);

    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    if (copy_fs(clone_flags, proc) != 0) { //for LAB8
        goto bad_fork_cleanup_kstack;
    }
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_fs;
    }
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);

    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_fs:  //for LAB8
    put_fs(proc);
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
	*/
	return 0;
}