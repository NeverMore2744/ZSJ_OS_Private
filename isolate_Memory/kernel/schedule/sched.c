
//include 原有debug文件失败，故新建  kernel/victordebug.h
#include "../victordebug.h"
#include <victor/sched.h>
#include <driver/vga.h>//kernel puts
#include <assert.h>
#include <auxillary/defs.h>
#include <driver/ps2.h>

#include "sched_impl.h"




#ifdef SCHED_MLFBQ_DEBUG   
    #include <intr.h>//enable/disable interrupt
#endif   
/*
 * 为了管理系统中所有的进程控制块， pc.c 维护了如下全局变量
 */
extern task_struct pcb[ ];//pcb[MAX_PID];
extern unsigned int init_gp;
 // current proc
extern int curr_proc;
extern void copy_context(context* src, context* dest);

static struct MLFQ_all_queues scheduler_MLFQ_queues;//main data structure 

/*
 *** sched_class_enqueue ***
封装，从而隐藏了内部结构MLFQ_all_queues
实现MLFQ的enqueue操作，本函数在创建新线程时调用，
用于将进程ASID载入到MLFQ的就绪队列中，以便参与到调度之中。

调用举例：
pc::pc::do_fork

pcb[availablePCB].counter = PROC_DEFAULT_TIMESLOTS;     
//Step 1. initilize all the PCBs
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE, "[pc::do_fork] wakeup_proc(proc)");
    proc->state = PROC_RUNNABLE;
    sched_class_enqueue(availablePCB);//enqueue

*/
void//public
sched_class_enqueue(int procASID) {
	MLFQ_enqueue_first_time( &scheduler_MLFQ_queues , procASID);
}


//dead function try to reschedule 
int  
pc_re_schedule_syscall(unsigned int status, unsigned int cause, context* pt_context)
{
	#ifdef SCHED_MLFBQ_DEBUG
    kernel_puts("[pc_re_schedule_syscall]@DEBUG: Begin Re Schedule.\n", 0x0f0, 0);	
	kernel_printf("[pc_re_schedule_syscall] curr_proc: %d    State:%d counter:%d. \n" , curr_proc, pcb[curr_proc].state, pcb[curr_proc].counter);
	#endif
	scheduler_schedule(status, cause, pt_context);
	
	return 0;
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
void 
scheduler_schedule(unsigned int status, unsigned int cause, context* pt_context) {
    
	assert( pcb[curr_proc].counter > 0 , "[scheduler_schedule] pcb[curr_proc].counter > 0.\n");
	
	
	
	
	if(pcb[curr_proc].counter > PROC_SECOND_TIME_DEFAULT_TIMESLOTS)
	kernel_printf("[scheduler_schedule] curr_proc: %d    State:%d counter:%d name:%s. \n" , curr_proc, pcb[curr_proc].state, pcb[curr_proc].counter, pcb[curr_proc].name );
	
	assert( pcb[curr_proc].counter <= PROC_SECOND_TIME_DEFAULT_TIMESLOTS, "[scheduler_schedule] pcb[curr_proc].counter <= PROC_SECOND_TIME_DEFAULT_TIMESLOTS.\n");
	pcb[curr_proc].counter--;
	
	//assert( curr_proc == 0 || curr_proc == 1 || curr_proc == 3 || curr_proc == 4  , "[scheduler_schedule] pcb[curr_proc].counter > 0.\n");
	
	
	
	
	
	//pcb[curr_proc].wait_state == -1;
	
	
	
	
	if(pcb[curr_proc].state != PROC_RUNNABLE)
	{
		kernel_printf("[scheduler_schedule] PROC_SLEEPING, curr_proc: %d    State:%d counter:%d\n." , curr_proc, pcb[curr_proc].state, pcb[curr_proc].counter);
		//assert( pcb[curr_proc].state == PROC_RUNNABLE , "[scheduler_schedule] pcb[curr_proc].state should be PROC_RUNNABLE;.\n");
		
	}

	//if( pcb[curr_proc].counter ==0 )
	while( pcb[curr_proc].counter ==0 || (pcb[curr_proc].state != PROC_RUNNABLE) )//time out
	{
		
		pcb[curr_proc].counter = PROC_SECOND_TIME_DEFAULT_TIMESLOTS;	
		//Step 1: Save context
		copy_context(pt_context, &(pcb[curr_proc].context));
		
		//-------------------------------   KEY PART - advanced algorithm and data structure would be put here in the future  ----------------------------------
		
		MLFQ_enqueue_second_time( &scheduler_MLFQ_queues, curr_proc); 
		
		int retASID;
		retASID = MLFQ_dequeue(&scheduler_MLFQ_queues);		
		
		
		/*
		// 该进程剩余的时间片，只对当前进程有效
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
		*/
		curr_proc = retASID;
		assert( retASID <= MAX_PID-1 &&  retASID >= 0 , "[scheduler_schedule] retASID <= size-1 &&  retASID >= 0  \n");
		
		//-----------------------------------------------------------------
		
		// Load context
		copy_context(&(pcb[curr_proc].context), pt_context);

		#ifdef SCHED_MLFBQ_DEBUG
		if(pcb[curr_proc].state != PROC_RUNNABLE)
		{
		    kernel_puts("[scheduler_schedule] (pcb[curr_proc].state != PROC_RUNNABLE)\n", 0x00f, 0);	
			kernel_printf("[scheduler_schedule]@DEBUG:while, [scheduler_schedule] curr_proc: %d    State:%d counter:%d. \n" , curr_proc, pcb[curr_proc].state, pcb[curr_proc].counter);	
		}
		#endif
		
	}
	
#ifdef SCHED_MLFBQ_DEBUG
	kernel_puts("[scheduler_schedule]@DEBUG: Finish Schedule.", 0x00f, 0);
	kernel_printf("PROC_RUNNABLE: %d    PROC_SLEEPING:%d \n" , PROC_RUNNABLE, PROC_SLEEPING);
	kernel_printf("[scheduler_schedule] curr_proc: %d    State:%d counter:%d name:%s. \n" , curr_proc, pcb[curr_proc].state, pcb[curr_proc].counter, pcb[curr_proc].name );
	//kernel_printf("[scheduler_schedule] Finish Schedule.\n");
	
	//enable_interrupts();//doesn't work
	{
		//kernel_getchar();    //this key would be discarded, step
		print_queues();//also step, since it is too long
	                   // there is a getchar() inside for debug
					   
	}
    //disable_interrupts();//doesn't work
	if(curr_proc == 0 || curr_proc == 1 || curr_proc == 3 || curr_proc == 4 )
	asm volatile("lui $t0, 0xF000\n\t"//Time Slice Define
	"mtc0 $t0, $9\n\t"
	);	


	else
	asm volatile("lui $t0, 0x1000\n\t"//Time Slice Define
	             "mtc0 $t0, $9\n\t"
				 );
#else
	asm volatile("mtc0 $zero, $9\n\t"); //move this asm sentence below and slow it down.
#endif

}



/*
多级反馈队列（Multi-Level Feedback Queue）调度算法：
	在采用多级反馈队列调度算法的执行逻辑流程如下：
设置多个就绪队列，并为各个队列赋予不同的优先级。
	第一个队列的优先级最高，第二队次之，其余队列优先级依次降低。
	仅当第1～i-1个队列均为空时，操作系统调度器才会调度第i个队列中的进程运行。
	赋予各个队列中进程执行时间片的大小也各不相同。在优先级越高的队列中，
	每个进程的执行时间片就越小或越大（Linux-2.4内核就是采用这种方式）。
当一个就绪进程需要链入就绪队列时，操作系统首先将它放入第一队列的末尾，
	按FCFS的原则排队等待调度。若轮到该进程执行且在一个时间片结束时尚未完成，
	则操作系统调度器便将该进程转入第二队列的末尾，再同样按先来先服务原则等待调度执行。
	如此下去，当一个长进程从第一队列降到最后一个队列后，
	在最后一个队列中，可使用FCFS或RR调度算法来运行处于此队列中的进程。
如果处理机正在第i（i>1）队列中为某进程服务时，
	又有新进程进入第k（k<i）的队列，则新进程将抢占正在运行进程的处理机，
	即由调度程序把正在执行进程放回第i队列末尾，重新将处理机分配给处于第k队列的新进程。
	从MLFQ调度算法可以看出长进程无法长期占用处理机，
	且系统的响应时间会缩短，吞吐量也不错（前提是没有频繁的短进程）。
	所以MLFQ调度算法是一种合适不同类型应用特征的综合进程调度算法。
*/


void MLFQ_queue_enque(struct MLFQ_queue *ptr_one_q, int procASID)
{
	//check full
	assert(ptr_one_q->head != ptr_one_q->tail + 1, "[MLFQ_queue_enque] The queue is full.\n");
	assert(ptr_one_q->tail <= MAX_PID-1 &&  ptr_one_q->tail >= 0 , "[MLFQ_queue_enque] ptr_one_q->tail <= MAX_PID-1 \n");
	assert(ptr_one_q->head <= MAX_PID-1 &&  ptr_one_q->head >= 0 , "[MLFQ_queue_enque] ptr_one_q->head <= MAX_PID-1 &&  ptr_one_q->head >= 0 \n");
	
	
	ptr_one_q->items[ptr_one_q->tail] = procASID;
	ptr_one_q->tail++;//move ptr
	if(ptr_one_q->tail == MAX_PID)//
	{
		//reaching the end of the queue
		ptr_one_q->tail = 0;//reset
	}


	return;
}

int MLFQ_queue_deque(struct MLFQ_queue *ptr_one_q )
{
	int procASID;//return value
	//check empty
	assert(ptr_one_q->head != ptr_one_q->tail, "[MLFQ_queue_enque] The queue is full.\n");
	assert(ptr_one_q->head <= MAX_PID-1 &&  ptr_one_q->head >= 0 , "[MLFQ_queue_deque] ptr_one_q->head <= MAX_PID-1 &&  ptr_one_q->head >= 0 \n");
	assert(ptr_one_q->tail <= MAX_PID-1 &&  ptr_one_q->tail >= 0 , "[MLFQ_queue_deque] ptr_one_q->tail <= MAX_PID-1 &&  ptr_one_q->tail >= 0  \n");
	
	procASID = ptr_one_q->items[ptr_one_q->head];
	
	if(ptr_one_q->head == MAX_PID-1)//the last valid item
	{
		//reaching the end of the queue
		ptr_one_q->head = 0;//reset
	}
	else
		ptr_one_q->head++;//move ptr
	
	return procASID;
	
}








//@Debug
static void print_queues()
{
	struct MLFQ_all_queues *ptr_queues = &scheduler_MLFQ_queues;
	
#ifndef SCHED_MLFBQ_DEBUG
    assert(0, "[sched::print_queues] This function should only be used during debugging.");
#endif

	int i;
	for(i=0; i<MLFQ_QUEUE_LEVEL; i++)
	{
		int j=ptr_queues->_queue[i].head;
		//int j_next = ( j==MAX_PID-1 )? 0 : j;//verilog style! Hahaha
		bool needPrintEnter = 0;
		int  debugPrintCount = 0;

		while( j!= ptr_queues->_queue[i].tail )
		{
			debugPrintCount++;
			assert( debugPrintCount <= MAX_PID-1, "[sched::print_queues]  debugPrintCount <= MAX_PID-1 ");
			
			int printProcessCursor = ptr_queues->_queue[i].items[j];

			kernel_printf("%d queue, %d element, ASID: %d, state:%d ;", i, j, ptr_queues->_queue[i].items[j]
										,pcb[printProcessCursor].state
									);
			//assert(sem->wait_queue[j_next] != -1, "[syncronization::wait_queue_del] sem->wait_queue[j_next] != -1 \n");
			assert( ptr_queues->_queue[i].items[j] != -1, "[sched::print_queues] ptr_queues->_queue[i].items[j] != -1 \n");
			//update index
			j++;
			j = ( j==MAX_PID )? 0 : j;//Error: j = ( j==MAX_PID-1 )? 0 : j;
			
			//j_next = ( j==MAX_PID-1 )? 0 : j;
			if(needPrintEnter == 1)
				kernel_printf("\n");
			else
				kernel_puts(" | ", 0x00f, 0);	
			needPrintEnter = !needPrintEnter;
		}
		
/*		
		int j;
		for(j=0; j<=MAX_PID-1; j++)//4 is small number
		{
			kernel_printf("%d queue, %d element, value: %d, \n", i, j, ptr_queues->_queue[i].items[j]);
		}
*/		
		kernel_printf("\nHead %d,Tail %d\n", ptr_queues->_queue[i].head, ptr_queues->_queue[i].tail);
        //kernel_getchar();    //this key would be discarded, step
	}
	
	
	
	
	
	kernel_printf("\n\n");

}


void MLFQ_all_queues_init(struct MLFQ_all_queues *ptr_queues)
{
	//struct MLFQ_all_queues scheduler_MLFQ_queues

	

	int i;
	for(i=0; i<MLFQ_QUEUE_LEVEL; i++)
	{
		//for each queue
		ptr_queues->_queue[i].enqueue = MLFQ_queue_enque;//function ptr
		ptr_queues->_queue[i].dequeue = MLFQ_queue_deque;//function ptr
		ptr_queues->_queue[i].head =  0;//the first valid item to read
		ptr_queues->_queue[i].tail =  0;//the first valid place to write.
		ptr_queues->_queue[i].size = MAX_PID;
		//memset
		kernel_memset( &(ptr_queues->_queue[i].items), -1, MAX_PID * sizeof(int) );
	}
    return;

}

void
sched_init(void) {
    MLFQ_all_queues_init( &scheduler_MLFQ_queues );
	
	/*
	list_init(&timer_list);

    sched_class = &MLFQ_sched_class;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
	*/
}

//如果判断进程 proc 的 rq 不为空且time_silce 为 0,则需要降 rq队列，
//从 rq[i]调整到 rq[i+1]，如果是最后一个 rq，即 rq[3]，
//则继续保持在 rq[3]中，然后调用 RR_enqueue 把 proc 插入到rq[i+1]中。
void
MLFQ_enqueue_first_time(struct MLFQ_all_queues *ptr_queues, int procASID) {
	
	assert(ptr_queues!=NULL, "[MLFQ_enqueue] ptr_queues should not be NULL.\n " );
	assert( procASID <= MAX_PID-1 &&  procASID >= 0 , "[MLFQ_enqueue] procASID <= MAX_PID-1 &&  procASID >= 0  \n");
	
	MLFQ_queue_enque( &(ptr_queues->_queue[0]) , procASID ); //ptr_queues->_queue[0].enque( &(ptr_queues->_queue[0]) , procASID );
	kernel_printf("[MLFQ_enqueue] Enque ASID :%d\n", procASID);
	//sched_class->enqueue(nrq, proc);//re-schedule
	return ;
}

/*
Function: MLFQ_enqueue_second_time
scheduler内部函数，用于已经在就绪队列的process再次入列。
本函数将会把ASID插入到第二级Queue中。
*/
void
MLFQ_enqueue_second_time(struct MLFQ_all_queues *ptr_queues, int procASID) {
	
	assert(ptr_queues!=NULL, "[MLFQ_enqueue] ptr_queues should not be NULL.\n " );
	assert( procASID <= MAX_PID-1 &&  procASID >= 0 , "[MLFQ_enqueue] procASID <= MAX_PID-1 &&  procASID >= 0  \n");
	
	MLFQ_queue_enque( &(ptr_queues->_queue[1]) , procASID );//ptr_queues->_queue[1].enque( &(ptr_queues->_queue[1]) , procASID );
	return ;
}


/*
Function: MLFQ_enqueue_second_time
scheduler内部函数，用于按照算法规则，从就绪队列中取出线程执行。
*/
int
MLFQ_dequeue(struct MLFQ_all_queues *ptr_queues) {
	
	assert(ptr_queues!=NULL, "[MLFQ_enqueue] ptr_queues should not be NULL.\n " );
	int procASID;
	
	struct MLFQ_queue *ptr_one_q = &(ptr_queues->_queue[0]);
	struct MLFQ_queue *ptr_two_q = &(ptr_queues->_queue[1]);
	if(ptr_one_q->head != ptr_one_q->tail )
		procASID = MLFQ_queue_deque( &(ptr_queues->_queue[0]) );//procASID = ptr_queues->_queue[0].deque( &(ptr_queues->_queue[0])  );
	else if (ptr_two_q->head != ptr_two_q->tail )
		procASID = MLFQ_queue_deque( &(ptr_queues->_queue[1]) );//procASID = ptr_queues->_queue[1].deque( &(ptr_queues->_queue[1])  );
	else
		assert(0, "[MLFQ_enqueue] Empty Queue.\n " );
	
	assert( procASID <= MAX_PID-1 &&  procASID >= 0 , "[MLFQ_dequeue] procASID <= MAX_PID-1 &&  procASID >= 0  \n");
	
	//kernel_printf("[MLFQ_dequeue] Deque ASID :%d\n", procASID);
	//sched_class->enqueue(nrq, proc);//re-schedule
	return procASID;
}
