#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H


/*
 * Modified by LIANG Weixin
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
//#define MAX_PROCESS                 8
#define MAX_PID                     8//(MAX_PROCESS * 2)
//unimplemented
typedef struct  {
	int unimplemented_in_pc;
} trapframe;
typedef struct {
	int unimplemented_in_pc;
} mm_struct;

typedef struct task_struct_definition task_struct;
struct task_struct_definition{
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
	volatile int need_resched;//boolean                 // bool value: need to be rescheduled to release CPU?
	
	/*
	 * parent ：用户进程的父进程（创建它的进程）。
	 * 在所有进程中，只有一个进程没有父进程，
	 * 就是内核创建的第一个内核线程 idleproc。*
	 * 内核根据这个父子关系建立进程的树形结构，
	 * 用于维护一些特殊的操作，
	 * 例如确定哪些进程是否可以对另外一些进程进行什么样的操作等等。
	 * 
	 */    
	task_struct *parent;                 // the parent process
	
	//mm ：内存管理的信息，包括内存映射列表、页表指针等。
	//mm 里有个很重要的项pgdir，记录的是该进程使用的一级页表的物理地址。
	mm_struct *mm;                       // Process's memory management field
	
	trapframe *tf;                       // Trap frame for current interrupt
	int    flags;                             // Process flag


};

//kernel state stack of the process
//the size is 1 page
//shares task_union with te task_struct
typedef union {
    task_struct task;                  //bottom: task_struct
    unsigned char kernel_stack[4096];  //high address: kernel state stack
} task_union;

#define PROC_DEFAULT_TIMESLOTS 6
//set zeros
void init_pc();

//timer interrupt handler
void pc_schedule(unsigned int status, unsigned int cause, context* pt_context);

//find a valid ASID 
int pc_peek();


void pc_create(int asid, void (*func)(), unsigned int init_sp, unsigned int init_gp, char* name);

//kill the current process, and reschedule 
void pc_kill_syscall(unsigned int status, unsigned int cause, context* pt_context);

//kill a specific process
int pc_kill(int proc);

task_struct* get_curr_pcb();
int print_proc();

//------  new function added below by LIANG weixin   -----------//
void cpu_idle(void) __attribute__((noreturn));

#endif  // !_ZJUNIX_PC_H
