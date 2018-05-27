#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H


/*
 * Modified by LIANG Weixin
 */ 

/*
  ***   进程状态管理概述   ***
用户进程有不同的状态（也可理解为“生命”的不同阶段），
当操作系统把程序的放到内存中后，这个进程就“诞生”了，
不过还没有开始执行，但已经消耗了内存资源，处于“创建”状态；当进程准备好各种资源，
就等能够使用CPU时，进程处于“就绪”状态；当进程终于占用CPU，程序的指令被CPU
一条一条执行的时候，这个进程就进入了“工作”状态，也称“运行”状态，
这时除了进一步占用内存资源外，还占用了CPU资源；当这个进程等待某个资源而无法继续执行时，
进程可放弃CPU使用，即释放CPU资源，进入“等待”状态；当程序指令执行完毕，
进程进入了“死亡”状态。这些状态的转换时机需要操作系统管理起来，
而且进程的创建和清除等工作必须由操作系统提供，而且从“运行”态与“就绪”态/“等待”态之间的转换，
涉及到保存和恢复进程的“执行现场”，也成为进程上下文，这是确保进程即使“断断续续”地执行，
也能正确完成工作的必要保证。

  *** 注解：等待状态 ***
为了支持用户进程完成特定事件的等待和唤醒操作，
我们设计了等待队列，从而使得用户进程可以方便地实现由于某事件没有完成而睡眠，
并且在事件完成后被唤醒的整个操作过程。
其基本设计思想是：当一个进程由于某个事件没有产生而需要在某个睡眠等待时，
设置自身运行状态为 PROC_SLEEPING，等待原因为某事件，
然后将自己的进程控制块指针和等待标记挂载入等待队列中
（如某一个信号量的等待队列），再执行 schedule 函数完成调度切换；
当某些事件发生后，另一个任务（进程）会唤醒等待队列上的某个或者所有进程，
唤醒操作就是将等待队列中的等待项中的进程运行状态设置为可调度的状态，
并且把等待项从等待队列中删除。

*/
// process's state in his life cycle
enum proc_state {
    PROC_UNINIT = 0,  // uninitialized
    PROC_SLEEPING,    // sleeping
    PROC_RUNNABLE,    // runnable(maybe running)
    PROC_ZOMBIE,      // almost dead, and wait parent proc to reclaim his resource
};

// Saved registers for kernel context switches.
// Save all the regular registers so we don't need to care
// which are caller save.
// The layout of context must match code in start.s. 
typedef struct {
    //the order is important
    //32 regs
	unsigned int epc;
    unsigned int at;
    unsigned int v0, v1;
    unsigned int a0, a1, a2, a3;
    unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
    unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
    unsigned int t8, t9;
    unsigned int hi, lo;
    unsigned int gp;
    unsigned int sp;
    unsigned int fp;
    unsigned int ra;
} context;

#define PROC_NAME_LEN               32
#define MAX_PROCESS                 8
#define MAX_PID                     (MAX_PROCESS * 2)
//unimplemented
typedef struct  {
	int unimplemented_in_pc;
} trapframe;
typedef struct {
	int unimplemented_in_pc;
} mm_struct;

typedef struct task_struct_definition task_struct;
struct task_struct_definition{
	// 该进程剩余的时间片，只对当前进程有效
	unsigned int counter;     //time slice left
	unsigned long start_time; //the time of its creation
    int ASID;                 //ASID: process num
	/*
	 * context：进程的上下文，用于进程切换（参见汇编代码的保存方法）。
	 * 在内核中，所有的进程在内核中也是相对独立的
	 * （例如独立的内核堆栈以及上下文等等）。
	 * 使用 context保存寄存器的目的就在于在内核态中能够进行上下文之间的切换。
	 */  	
    context context;          //32 regs
    char name[PROC_NAME_LEN+5];            //the name of the process
	//new field added by LIANG weixin:
	//state：进程所处的状态。
    enum proc_state state;                      // Process state
	//problematic? may overflow
    int runs;                                   // the running times of Proces	
	//for idle, need_resched should be 1
	// 该进程是否需要调度，只对当前进程有效
	volatile int need_resched;//boolean                 
	// bool value: need to be rescheduled to release CPU?
	/*
	 * parent ：用户进程的父进程（创建它的进程）。
	 * 在所有进程中，只有一个进程没有父进程，
	 * 就是内核创建的第一个内核线程 idleproc。*
	 * 内核根据这个父子关系建立进程的树形结构，
	 * 用于维护一些特殊的操作，
	 * 例如确定哪些进程是否可以对另外一些进程进行什么样的操作等等。
	 * 
	 */    
	int parent;                 // the parent process
	
	//mm ：内存管理的信息，包括内存映射列表、页表指针等。
	//mm 里有个很重要的项pgdir，记录的是该进程使用的一级页表的物理地址。
	mm_struct *mm;                       // Process's memory management field
	
	trapframe *tf;                       // Trap frame for current interrupt
	int    flags;                             // Process flag
	
	int    wait_state;//for what reason it is waiting, not waiting: -1, WT_NOT_WAITING


};

#define PF_EXITING                  0x00000001      // getting shutdown

#define WT_INTERRUPTED               0x80000000                    // the wait state could be interrupted
#define WT_CHILD                    (0x00000001 | WT_INTERRUPTED)  // wait child process
#define WT_KSEM                      0x00000100                    // wait kernel semaphore
#define WT_TIMER                    (0x00000002 | WT_INTERRUPTED)  // wait timer
#define WT_KBD                      (0x00000004 | WT_INTERRUPTED)  // wait the input of keyboard
//victor dialect
#define WT_NOT_WAITING                  -1

//kernel state stack of the process
//the size is 1 page
//shares task_union with te task_struct
typedef union {
    task_struct task;                  //bottom: task_struct
    unsigned char kernel_stack[4096];  //high address: kernel state stack
} task_union;


//set zeros
void init_pc(unsigned int init_gp);

//timer interrupt handler
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);

//find a valid ASID 
int pc_peek();


void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name);

//kill the current process, and reschedule 
int pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);

//kill a specific process
int pc_kill(int proc);

task_struct* get_curr_pcb();
int print_proc();

//------ share global variable ---//

extern task_struct pcb[ ];//pcb[MAX_PID];
extern unsigned int init_gp;
 // current proc
extern int curr_proc;

//------  new function added below by LIANG weixin   -----------//
void cpu_idle(void) __attribute__((noreturn));
int kernel_thread(int (*fn)(void *), void *arg, int clone_flags);
int do_fork(int clone_flags, int stack, context *ctxt);
#endif  // !_ZJUNIX_PC_H
