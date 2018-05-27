#ifndef __VICTOR_SCHED_H__
#define __VICTOR_SCHED_H__

/* 进程调度算法：多级反馈队列算法（preemptive）
   ***   Principle of work: a brief introduction to multilevel feedback queue   ***

The multilevel feedback queue scheduling algorithm, in contrast, allows
a process to move between queues. The idea is to separate processes 
according to the characteristics of their CPU bursts. 
If a process uses too much CPU time, it will be moved 
to a lower-priority queue. This scheme leaves I/O-bound and 
interactive processes in the higher-priority queues. 
In addition, a process that waits too long in a lower-priority queue 
may be moved to a higher-priority
queue. This form of aging prevents starvation.
 
The deﬁnition of a multilevel feedback queue scheduler makes it the most
general CPU-scheduling algorithm. It can be conﬁgured to match a speciﬁc
system under design. Unfortunately, it is also the most complex algorithm,
since defining the best scheduler requires some means by which to select
values for all the parameters.

   ****   算法剖析   ****
   （preemptive）
多级反馈队列（Multi-Level Feedback Queue）调度算法：在采用多级反馈队列
调度算法的执行逻辑流程如下：
•	设置多个就绪队列，并为各个队列赋予不同的优先级。第一个队列的优先级最高，
第二队次之，其余队列优先级依次降低。仅当第 1～i-1 个队列均为空 时，
操作系统调度器才会调度第 i 个队列中的进程运行。赋予各个队列中进
程执行时间片的大小也各不相同。在优先级越高的队列中，每个进程的执行时间片
就越小或越大（Linux-2.4 内核就是采用这种方式）。
•	当一个就绪进程需要链入就绪队列时，操作系统首先将它放入第一队列的末尾，
按 FCFS 的原则排队等待调度。若轮到该进程执行且在一个时间片结束 时尚未完成，
则操作系统调度器便将该进程转入第二队列的末尾，再同样按先来先服务原则等待调度执行。
如此下去，当一个长进程从第一队列降到最 后一个队列后，在最后一个队列中，
可使用 FCFS 或 RR 调度算法来运行处于 此队列中的进程。
•	如果处理机正在第 （i>1）队列中为某进程服务时，又有新进程进入第 k（k<i）的队列，
则新进程将抢占正在运行进程的处理机，即由调度程序把正在执行进程放回第 i 队列末尾，
重新将处理机分配给处于第 k 队列的新进程。
从 MLFQ 调度算法可以看出长进程无法长期占用处理机，
且系统的响应时间会缩 短，吞吐量也不错（前提是没有频繁的短进程）。
所以 MLFQ 调度算法是一种合 适不同类型应用特征的综合进程调度算法。

Note: all the inner data structures are 
hidden in kernel/schedule/sched_impl.h
*/


//#include <proc.h>
#include <zjunix/pc.h>
#define PROC_DEFAULT_TIMESLOTS 1//2//time in the first level queue
#define PROC_SECOND_TIME_DEFAULT_TIMESLOTS 1//4//time in the second level queue

void sched_init(void);
//void scheduler_schedule(void);
void scheduler_schedule(unsigned int status, unsigned int cause, context* pt_context);

//dead function try to reschedule 
int pc_re_schedule_syscall(unsigned int status, unsigned int cause, context* pt_context);
//void wakeup_proc(struct proc_struct *proc);

//interface
void sched_class_enqueue(int procASID);


#endif /* !__VICTOR_SCHED_H__ */

/*

    sched_class_enqueue(idleproc);//enqueue
	pcb[idleproc].counter = PROC_DEFAULT_TIMESLOTS;     //Step 1. initilize all the PCBs

*/