//private implementation

#include "pc.h"

#include <driver/vga.h>
#include <intr.h>
#include <zjunix/syscall.h>
#include <zjunix/utils.h>
#include <zjunix/slab.h>
#include <assert.h>
//new include dir
#include <victor/sched.h>

//need to allocate memory
//#include <zjunix/slab.h> //problem
#include <auxillary/defs.h>
#include <auxillary/error.h>
#include <auxillary/unistd.h>

/*
 * 为了管理系统中所有的进程控制块， 维护了如下全局变量
 */
task_struct pcb[MAX_PID];

/* current proc
 * 当前占用 CPU，处于“运行”状态进程控制块指针。
 * 通常这个变量是只读的，只有在进程切换的时候才进行修改，
 * 并且整个切换和修改过程需要保证操作的原子性，至少需要屏蔽中断，
 */
unsigned int init_gp = 0;
 // current proc
int curr_proc = -1;


static int nr_process = 0;//计数，已经创建PCB的数目 

// idle proc
int idleproc = -1;//NULL;//or use type int ?

/* init proc
 * static struct proc *initproc;  //指向第一个用户态进程（proj10 以后）
 */
int initproc = -1;//NULL;


void copy_context(context* src, context* dest) {
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

/* 
 * alloc_proc函数（位于kern/pc/pc.c中）负责分配一个新的task_struct结构，
 * 用于存储新建立的内核线程的管理信息。此函数需要对这个结构进行最基本的初始化。
 * 本函数的主要工作是对未经初始化的pcb (Process block) 进行必要的初始化。
 * 但需要注意，本函数并没有完成所有的初始化工作。函数调用者（如，do_fork 函数）
 * 需要根据自身需求进一步初始化，赋值。
 */
// alloc_proc - alloc a task_struct and init all fields of task_struct
static int
alloc_proc(void) {
    //task_struct *proc = kmalloc(sizeof( task_struct ));//struct task_struct *, sizeof(struct task_struct)
    
	int availablePCB = pc_peek();
	if(availablePCB == -1 || availablePCB>MAX_PID ){
		assert(0,"[alloc_proc] allocation of PCB fails.");
		while(1);
	}
	

	task_struct *proc = &(pcb[availablePCB]);

	
	
	if (proc != NULL) {    
		//void* kernel_memset(void* dest, int b, int len);
        kernel_memset(&(proc->context), 0, sizeof(context));//struct context
		proc->state = PROC_UNINIT;
        proc->ASID = -1;//proc->pid = -1;
        proc->runs = 0;
        proc->context.sp = 0;//init_sp;// proc->kstack = 0;
		proc->context.gp = 0;
        proc->need_resched = 0;
        proc->parent = -1;//NULL;//int
        proc->mm = NULL;//not used
        proc->tf = NULL;//not used
        //proc->cr3 = boot_cr3;
        proc->flags = 0;
        kernel_memset(proc->name, 0, PROC_NAME_LEN);
		//unique
		proc->counter = 0;//PROC_DEFAULT_TIMESLOTS
		proc->start_time = 0;
		proc->wait_state = WT_NOT_WAITING;//-1;// not waiting at all
    }
	else{
		assert(0, "[alloc_proc]: kmalloc failed. It returned NULL. ");  //assertion must fail
	}
		
    return availablePCB;//return proc;//ptr
}



void initializePCB(){
	int i;
	char uninitialized_name[] = "Not_initialized_yet";
    for (i = 0; i < MAX_PID; i++)
	{
		pcb[i].ASID = -1;//set invalid
		//pcb[i].name = 
		
		kernel_strcpy(pcb[i].name, uninitialized_name);
		pcb[i].state = PROC_UNINIT;
		pcb[i].need_resched = 0;
		pcb[i].runs = 0;
		pcb[i].need_resched = 0;
		pcb[i].parent = -1;//NULL;
	}
}
// init_main - the second kernel thread used to create user_main kernel threads
#pragma GCC push_options
#pragma GCC optimize("O0")
static int
init_main(void *arg) {
	
    kernel_printf("this initproc, pid = %d, name = %s \n", pcb[curr_proc].ASID, pcb[curr_proc].name);
    kernel_printf("To You: \"%s\".\n", (const char *)arg);

    //-----------------------   check syncronization -----------------------//
//	extern void check_sync(void);//syncronization checking
//  check_sync();                // check philosopher sync problem
	 //-----------------------   check syncronization end -----------------------//
    
     
    //-----------------------   check syscall 4: print string -----------------------//
	char try_syscall_4_szString[] = "[init_main] try_syscall_4_szString\n";
	
	asm volatile(                                //try system call
		"li $a0, 0\n\t"
		"add $a0, %0,$a0\n\t"// $a0: 传入参数, mov $a0, %0
		
        :      //output
		: "r"( try_syscall_4_szString )		//input
        );
		
	const int syscallCode = SYS_printstring;//#define SYS_printstring 
	asm volatile(                                //try system call
        "add $v0, $zero, %0\n\t"   // $v0: system call code
		"syscall\n\t"
		
		:
		: "r"( syscallCode )		//input
        );	
    //-----------------------   check syscall 4: print string end -----------------------//
    

//-----------------------   check semaphore blocked -----------------------//



//-----------------------   check semaphore blocked end-----------------------//

    //-----------------------   wait for a while  -----------------------//
	int i = 0;
    while(++i!=8388480);//waiting for a while
    //-----------------------   wait for a while  end -----------------------//


	
	kernel_printf("Wishing you prosperity;\n");
	kernel_printf("Kung Hei Fat Choy;"); 
    kernel_printf("To You: en.., Bye, Bye. :)\n");

    //Kidding :) , just testing the power of this function.
    //disable_interrupts();//local_intr_save(intr_flag);//先关中断  //!!!!Crital ERROR FATAL  WANRING 
    
    
    return 0;//future: syscall SYS_EXIT
}
#pragma GCC pop_options	


/*
 特例：创建第0个内核线程 idle
在 init.c::init_kernel()函数调用了 pc.c:: init_pc 函数。
pc.c:: init_pc函数启动了创建内核线程的步骤。 
首先当前的执行上下文（从 init_kernel启动至今）就可以看成是一个
内核线程的上下文(其堆栈初始值为0x8100_0000)，见arch::mips::start.s

start:
	lui $sp, 0x8100     #ghost number, the initial stack
	la $gp, _gp 		# _gp is defined in kernel.ld
	j init_kernel
	nop

为此我们可以通过给当前执行的上下文分配一个进程控制块以及对它
进行相应初始化而将其打造成第 0 个内核线程 -- idleproc。具体步骤如下：
首先调用 alloc_proc 函数来获得 task_struct 结构的一块内存
，这就是第 0 个进程控制块了。

 */
void 
init_pc(unsigned int parameter_init_gp) {

    sched_init();
	
	init_gp = parameter_init_gp;//initialize gp.
	
	initializePCB();//all the PCB blocks    
	
	//--------- initialize the first process --------------//
	//should be performed by alloc_proc 
	idleproc = alloc_proc();
	if(idleproc != 0){
		kernel_printf("idleproc.ASID: %d", idleproc);
		assert(idleproc == 0,"[init_pc] idleproc.ASID must be zero.\n");
	}
    

	
	pcb[idleproc].state = PROC_UNINIT; //设置进程为“初始”态
	pcb[idleproc].ASID = -1;           //进程的 pid还没设置好
	/*
	 * 语句给了 idleproc 合法的身份证号--0，
	 * 这名正言顺地表明了 idleproc 是第 0 个内核线程。
	 * “0”是第一个的表示方法是计算机领域所特有的，
	 * 比如 C语言定义的第一个数组元素的小标也是“0”。
	 */
	 
    pcb[idleproc].ASID = 0;
	pcb[idleproc].wait_state = -1;
    //sched_class_enqueue(idleproc);//Don't enqueue, since it is the current process.
	pcb[idleproc].counter = PROC_DEFAULT_TIMESLOTS;     //Step 1. initilize all the PCBs
    kernel_strcpy(pcb[idleproc].name, "idle");          //Step 2. construct the idle process
    curr_proc = 0;                               //        which process is running currently
    
	//idleproc->pid = 0;//ASID
	/*
	 * 第二条语句改变了 idleproc 的状态，
	 * 使得它从 “出生”转到了“准备工作”，就差调度它执行了。
	 */
    pcb[idleproc].state = PROC_RUNNABLE;
	//nr_process++;
	

    
	//idleproc->kstack = (uintptr_t)bootstack;
    
	//very dangerous
	//pcb[idleproc].need_resched = 1;//boolean
	
    /*
    一般方案(举例)：创建第 1 个内核线程 init
    调用kernel_thread函数创建线程，进行错误检查，
    同时设置名字。注意，若未给进程设置名字，进程运行时不受影响的。
    名字的设置只是使得它易于被观察，调试，同时也符合用户的预期。
    */

	/*
	 * 第 0 个内核线程主要工作是完成内核中各个子系统的初始化，
	 * 然后就通过执行 cpu_idle函数开始过退休生活了。
	 * 但接下来还需创建其他进程来完成各种工作，但 idleproc 自己不想做，
	 * 于是就通过调用 kernel_thread 函数创建了一个内核线程 init_main。
	 */
	//create init
	
	const int input_for_init_main = 888;//not used
    int pid = kernel_thread(init_main, 
        (void *)"Hello world!!" , 0);//"Hello world!!" would not be put in the stack.
	
	if(pid!=1)
	{
		kernel_printf("[init_pc] init_main.ASID: %d  ", pid);
		print_proc();
		assert(pid==1, "[init_pc] create init_main failed.\n");
	}
    


    initproc = pid;//initproc = find_proc(pid);
    char initproc_name[] = "init";//set_proc_name(initproc, "init");
	
	//char* kernel_strcpy(char* dest, const char* src);
	kernel_strcpy(pcb[initproc].name, initproc_name); 

    assert(idleproc ==0 && pcb[idleproc].ASID == 0,"idleproc must be 0.");
    assert(initproc ==1 && pcb[initproc].ASID == 1,"initproc must be 1.");
	
	//#define SYS_kill            12// since we have new method to register syscall, see syscall.c
	//register_syscall(10, pc_kill_syscall);       //Step 3. add the "kill" syscall, index = 10
    register_interrupt_handler(7, pc_schedule);  //Step 4. register the timer interrupt. 
	                                             //        function protocal: void register_interrupt_handler(int index, intr_fn fn)
												 //        Once interrupt, it would call pc_schedule(a function ptr)

    asm volatile(                                //Step 5.  sett the size of time slice
                                // Volume 2: The Compare register acts in conjunction with 
                                //          the Count register to implement a timer and 
                                //          timer interrupt function.
        "li $v0, 1000000\n\t"   // $11(Compare)与$9(Counter)共同完成计时与定时中断功能, 10^6
        "mtc0 $v0, $11\n\t"     // $11固定，存储当前时间片的大小
        "mtc0 $zero, $9");
		
	//DEBUG	
        //stall
	//init_main("Hello world!!");	
	print_proc();
	//while(1);
}

/*
 *** 时钟中断调用关系梳理 ***
完成了时钟中断初始化，以及中断注册函数的注册。
为了和历史版本兼容，此处注册函数仍为pc::pc_schedule 。
即每次为调度设置的时钟中断都由pc::pc_schedule负责相应。
In kernel/pc/pc.c::init_pc 

但是为了实现功能的模块化，pc::pc_schedule在本实现中只是一个壳子，
它直接执行schedule::sched::scheduler_schedule 。
仍然注册pc::pc_schedule作为时钟中断响应函数的原因是为了和历史版本兼容。
*/
//usage: register_interrupt_handler(7, pc_schedule);
//it seeems that it would handle the timer interrupt.
//dummy call, actually it calls scheduler class.
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context) {
	scheduler_schedule(status, cause, pt_context);//dispatch
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
	
	kernel_printf("OK_create!\n");  // 这三句printf如果删掉会出问题，原因不明
	pcb[asid].state = PROC_UNINIT;//begin allocation
	
	pcb[asid].context.epc = (unsigned int)func;
    pcb[asid].context.sp = init_sp;
    pcb[asid].context.gp = init_gp;	
    kernel_strcpy(pcb[asid].name, name);
    pcb[asid].ASID = asid;
	
	pcb[asid].runs = 0;
	kernel_printf("OK_create 1!\n");
	pcb[asid].state = PROC_RUNNABLE;//end allocation
	sched_class_enqueue(asid);//enqueue
	pcb[asid].counter = PROC_DEFAULT_TIMESLOTS;     //Step 1. initilize all the PCBs
	pcb[asid].wait_state = -1;
	
	kernel_printf("OK_create 2!\n");
}


// 杀死进程的实质就是废掉一个PCB
// 杀死的是当前正在运行的进程，把ASID置为无效(-1)
int pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context) {
    if (curr_proc != 0) {
        pcb[curr_proc].ASID = -1;
        pc_schedule(status, cause, pt_context);
    }
	return 0;
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

/*
注解：垂死状态
运行态到就退出态的变化过程：当用户进程执行完毕（或者被要求强行退出）后，
将执行do_exit函数完成对自身所占部分资源的回收，并执行进程调度切换。
*/
//new function: note that the function has different return type.
// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to 
//       proc->tf in do_fork-->copy_thread function
void 
deadLoop_for_returned_process(int process_return_value)
{
	kernel_printf("[deadLoop_for_returned_process] A process returned,with value: %d\n", process_return_value);
    while(1);
    // current->state = PROC_ZOMBIE;//in the future, 
    // current->exit_code = error_code;  //in the future, 
    //syscall XXX do_exit. Write it in the future
}

/*
 * kernel_thread 函数传入参数为线程执行的函数指针，函数参数，以及内核进程的标志位信息。
 * 本函数是创建内核线程的最顶层的封装。
 * 调用kernel_thread 函数将会完成所有创建内核线程创建的工作。
 * 概括地来讲，创建内核线程需要分配和设置各类资源。
 * 从kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。
 */
int
kernel_thread(int (*fn)(void *), void *arg, int clone_flags) {
    /*
	struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;!!
    */

    
    /*
     * kernel_thread 在本函数体内主要完成内核线程context初始化的工作，
     * 包括：寄存器置零，设置执行起点（fn函数），设置返回点
     * （用于接收执行完毕的进程，完成进程退出的相关工作）。
     */

	context no_father_context;
    kernel_memset( &no_father_context, 0, sizeof(context) );
	no_father_context.epc = (unsigned int)fn;
    //no_father_context.sp = init_sp; calling do_fork,with ???init_sp=0???
	//I was fucked hard. The subfunction always try to give a stack.
	
    no_father_context.gp = init_gp;	
	no_father_context.a0 = (unsigned int)arg;
	//return??
	no_father_context.ra = (unsigned int)deadLoop_for_returned_process;

    return do_fork(clone_flags | CLONE_VM, 0, &no_father_context);
	//return 0;
}

static int
setup_kstack(task_struct *proc);
static int
copy_mm(uint32_t clone_flags, task_struct *proc);
static void
copy_thread(int proc_num, uintptr_t esp, context *ctxt) ;

/*
do_fork 函数会调用alloc_proc函数来分配并初始化一个进程控制块，
但alloc_proc只是找到了一小块内存用以记录进程的必要信息，
并没有实际分配这些资源。我们通过do_fork实际创建新的内核线程。
do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，
但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。
*/

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int
do_fork(int clone_flags, int stack, context *ctxt) {
    int ret = -E_NO_FREE_PROC;
    
    task_struct *proc;

    /*
    if (nr_process >= MAX_PID) {//or MAX_PROCESS??
        goto fork_out;
    }
    ret = -E_NO_MEM;
    */
	
    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakeup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

	//调用alloc_proc，首先获得一块用户信息块。
	int availablePCB = alloc_proc();
	assert( availablePCB >= 0 && availablePCB<=MAX_PID-1, "[do_fork] availablePCB >= 0 && availablePCB<=MAX_PID-1.");
	proc = &(pcb[availablePCB]);
    if ( proc == NULL) {
        goto fork_out;
    }

    proc->parent = curr_proc;//&(pcb[curr_proc]);//current;
    //assert(current->wait_state == 0);//no wait_state

	//为进程分配一个内核栈。
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
	unsigned int get_stack_addr = proc->context.sp;
	assert(get_stack_addr == pcb[availablePCB].context.sp, "[do_fork] Stack initialization not Equal.");
	/*
    if (copy_fs(clone_flags, proc) != 0) { //for LAB8
        goto bad_fork_cleanup_kstack;
    }
	*/
	
	//复制原进程的内存管理信息到新进程（但内核线程不必做此事）
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_fs;
    }
	
	//复制原进程上下文到新进程
	
    //copy_thread(proc, stack, tf);//unimplemented
	copy_thread(availablePCB, stack, ctxt);
	
	
	proc->ASID = availablePCB;//pc_peek();//get_pid();
/*
	//将新进程添加到进程列表
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);

    }
    local_intr_restore(intr_flag);
*/    
	//唤醒新进程//wakeup_proc(proc);
	
	pcb[availablePCB].counter = PROC_DEFAULT_TIMESLOTS;     //Step 1. initilize all the PCBs
	assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE, "[pc::do_fork] wakeup_proc(proc)");
    proc->state = PROC_RUNNABLE;
    sched_class_enqueue(availablePCB);//enqueue
	
    ret = proc->ASID;//proc->pid;
fork_out:
    return ret;//返回新进程号

	//Release, other wise memory would leak.
bad_fork_cleanup_fs:  //for LAB8
    //put_fs(proc);
bad_fork_cleanup_kstack:
    //put_kstack(proc);
bad_fork_cleanup_proc:
    //kfree(proc);
    goto fork_out;
	
	return 0;
}




// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
static int
copy_mm(uint32_t clone_flags, task_struct *proc) {
    //assert(current->mm == NULL);
	
    /* do nothing in this project */
    return 0;
}

//stack
//static char dummy_setup_kstack_area[65536];//size of stack: 4096//4096*16 = 65536 //MAX_PID
//static unsigned int dummy_setup_kstack_area_ptr = (unsigned int)dummy_setup_kstack_area;
//static int dummy_setup_kstack_count = 0;
// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static char dont_know_why_kernel_stack_is_zero[4096];
static void
copy_thread(int proc_num, uintptr_t esp, context *ctxt) {
	
	unsigned int stack_that_already_initialize = pcb[proc_num].context.sp;

	
	
    //pcb[proc_num].context = ???;current??
	assert(proc_num < MAX_PID && proc_num>=0, "[copy_thread]: invalid father ASID.");  // Ensure that asid is not too big
	
	//pcb[proc_num].context = *ctxt;//this sentence would call memcpy, which can't be resolved.
	kernel_memcpy( &(pcb[proc_num].context), ctxt, sizeof(context) );
	
	pcb[proc_num].context.sp = stack_that_already_initialize ;
	
	if(stack_that_already_initialize == 0)//making sure
	{
		pcb[proc_num].context.sp = (unsigned int)dont_know_why_kernel_stack_is_zero + 4096;//old: not adding 4096
		kernel_printf("[copy_thread] You are giving empty stack!!\n");
	}
		
	//else
		//pcb[proc_num].context.sp = esp;//NEVER USE ESP
    /*
	if(pcb[proc_num].context.sp != dummy_setup_kstack_area_ptr)
	{
	    kernel_printf("[copy_thread] Stack Not Equal!! \n");
	}*/
	
	/*
	proc->tf = (trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
	*/
}


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(task_struct *proc) {
    
    proc->context.sp = (unsigned int)kmalloc(4096);
    return 0;
    /*
	int stack_start = (unsigned int)kmalloc(65536);
	int stack_end   = stack_start + 65536;//4096*16 = 65536 //MAX_PID
	
	
	if(dummy_setup_kstack_count<MAX_PID-1)
	{
		dummy_setup_kstack_area_ptr += 4096;
		unsigned int stack_top = dummy_setup_kstack_area_ptr;
		
		dummy_setup_kstack_count++;
		
		proc->context.sp = stack_top;
		//unsigned int stack_top = (unsigned int)&(dummy_setup_kstack_area[dummy_setup_kstack_count][4090]);
        
        kernel_printf("[setup_kstack] (minus 0x80000000)Stack buffer area from :%d, to :%d, allocating addr: %d\n",
                                             stack_start - 0x80000000,
                                             stack_end  - 0x80000000,
                                             stack_top - 0x80000000);
                                            
		//proc->context.sp = stack_top;
		
		return 0;		
	}
	else
		kernel_printf("[setup_kstack]Stack buffer area is Full.");

	 * Memory management issue
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL) {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }

    return -E_NO_MEM; */
}
