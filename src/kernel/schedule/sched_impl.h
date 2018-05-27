#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

//#include <defs.h>

///


#define MLFQ_QUEUE_LEVEL 2
struct MLFQ_queue
{
	int items[MAX_PID];
	int head;//the first valid item, -1
	int tail;//the first place to write, 0
	int size;//MAX_PID
	
	// 将进程 p 插入队列 rq
	void (*enqueue)(struct MLFQ_queue *ptr_one_q, int procASID);
	//dequeue, return an int: procASID
	int (*dequeue)(struct MLFQ_queue *ptr_one_q);
};

/*
下面的两个函数是服务
struct MLFQ_queue
的单个等待队列的基本操作函数，实现了队列的入列，出列基本操作。
*/
//struct MLFQ_queue;
void MLFQ_queue_enque(struct MLFQ_queue *ptr_one_q, int procASID);
int MLFQ_queue_deque(struct MLFQ_queue *ptr_one_q );

/*
 ***  Top level: The Multi-level feedback queue ****
当前MLFQ调度算法其实是对RR算法的进一步扩展，即把一个rq队列扩展为了n个rq队列。多
个rq队列确保了长时间运行的进程优先级会随着执行时间的增加而降低，而短时间运行的进程
会优于长时间运行的进程被先调度执行。
*/
//struct MLFQ_all_queues;
struct MLFQ_all_queues
{
	struct MLFQ_queue _queue[MLFQ_QUEUE_LEVEL];
	int queue_count;//MLFQ_QUEUE_LEVEL
};


/* 
  *** MLFQ_enqueue_first_time ***
本函数实现MLFQ的enqueue操作，本函数在创建新线程时调用，
用于将进程ASID载入到MLFQ的就绪队列中，以便参与到调度之中。
注意此函数被封装为如下形式，从而隐藏了内部结构MLFQ_all_queues：
void//public
sched_class_enqueue(int procASID) {
    MLFQ_enqueue_first_time( &scheduler_MLFQ_queues , procASID);
}

调用举例：
<in pc::pc::do_fork>

pcb[availablePCB].counter = PROC_DEFAULT_TIMESLOTS;     
//Step 1. initilize all the PCBs
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE, "[pc::do_fork] wakeup_proc(proc)");
    proc->state = PROC_RUNNABLE;
    sched_class_enqueue(availablePCB);//enqueue

*/
void MLFQ_enqueue_first_time(struct MLFQ_all_queues *ptr_queues, int procASID);

/*
Function: MLFQ_enqueue_second_time
scheduler内部函数，用于已经在就绪队列的process再次入列。
本函数将会把ASID插入到第二级Queue中。
*/
void MLFQ_enqueue_second_time(struct MLFQ_all_queues *ptr_queues, int procASID); 

/*
Function: MLFQ_enqueue_second_time
scheduler内部函数，用于按照算法规则，从就绪队列中取出线程执行。
*/
int MLFQ_dequeue(struct MLFQ_all_queues *ptr_queues);

//debug
static void print_queues();
#endif /* !__KERN_SCHEDULE_SCHED_H__ */

