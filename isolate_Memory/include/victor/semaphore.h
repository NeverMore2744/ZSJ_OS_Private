#ifndef __VICTOR_SYNC_SEM_H__
#define __VICTOR_SYNC_SEM_H__

//#include <defs.h>
#include <auxillary/defs.h>
#include <zjunix/pc.h>
//#include <atomic.h>
//#include <wait.h>

/*
数据结构
To implement semaphores under this deﬁnition, we deﬁne a semaphore as
follows:

typedef struct {
 int value;
 struct process *list;
} semaphore;

仿照课本，按照如下方法实现信号量的数据结构。
Each semaphore has an integer value and a list of processes list.
*/
#define SEMAPHORE_MAX_NAME_LEN 32
typedef struct {
    int value;//信号量的值
    //wait_queue_t wait_queue;
	int wait_queue[MAX_PID];//等待队列
	int head;//the next to read, read ptr
	int tail;//the next to write, write ptr
	char name[SEMAPHORE_MAX_NAME_LEN];//信号量的名字
	
	//debug information
	bool private_is_initialized;//私有变量，用于确认信号量是否被初始化过。
	//因为信号量将阻塞进程，威力极其强大，所以需要十分谨慎。
} semaphore;
/*
	value>0，表示共享资源的空闲数
	vlaue<0，表示该信号量的等待队列里的进程数
	value=0，表示等待队列为空
 */

void sem_init(semaphore *sem, int value, char* name);
void up(semaphore *sem);
void down(semaphore *sem);
//bool try_down(semaphore *sem);//!unimplemented
void sem_print_wait_queues(semaphore *sem);

#endif /* !__VICTOR_SYNC_SEM_H_ */