//include 原有debug文件失败，故新建  kernel/victordebug.h
#include "../victordebug.h"

//option 
//#define BUSY_WAITING_IN_SEMAPHORE

#include <victor/semaphore.h>
#include <zjunix/pc.h>
#include <zjunix/utils.h>

#include <intr.h>//enable/disable interrupt
#include <assert.h>

#include <driver/vga.h>
#include <auxillary/unistd.h>


extern task_struct pcb[ ];//pcb[MAX_PID];
extern unsigned int init_gp;
 // current proc
extern int curr_proc;

void//Delete current
wait_queue_del(semaphore *sem,  int curr_proc);

void wait_current_set(semaphore *sem, uint32_t wait_state);

void//delete the current 
wait_current_del(semaphore *sem, int curr_proc);

//return the process ASID that is deleted
int wait_queue_first_del(semaphore *sem, uint32_t wait_state);
//set the first process in the queue with the state runnable, but would not delete it in the wait queue 
void wakeup_wait(semaphore *sem, uint32_t wakeup_flags); 

static void semaphore_wait_queue_enque(semaphore *ptr_one_q, int procASID);

void
sem_init(semaphore *sem, int value, char* name) {
    assert(value >=0, "[sem_init]: semaphore value should >= 0");
    kernel_memset( sem, 0, sizeof(semaphore));//struct semaphore

	
	kernel_memset( sem->wait_queue, -1, MAX_PID * sizeof(int) );//wait_queue
	
	sem->value = value;//initialize value
	sem->head = 0;
	sem->tail = 0;
	kernel_strcpy( sem->name, name);
	
	sem->private_is_initialized = 1;

	#ifdef SEMAPHORE_DEBUG
	kernel_puts("[semaphore::sem_init]@DEBUG: initilizing: ", 0x00f, 0);
	kernel_printf("[semaphore::sem_init] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
	#endif //!SEMAPHORE_DEBUG	

    return;
}

/* 
 * ● __up(semaphore *sem, uint32_t wait_state)：具体实现信号量的V操作，首先关中断，如
	果信号量对应的wait queue中没有进程在等待，直接把信号量的value加一，然后开中断返
	回；如果有进程在等待且进程等待的原因是semophore设置的，则调用wakeup_wait函数将
	waitqueue中等待的第一个wait删除，且把此wait关联的进程唤醒，最后开中断返回。
 */
 

void
up(semaphore *sem) {// same as signal()

	#ifdef SEMAPHORE_DEBUG
	//用于debug，每次执行都输出debug信息，包括信号量的值，名字，当前进程ASID等等
	kernel_puts("[semaphore::up]@DEBUG: up(semaphore *sem)[before up] .\n", 0x00f, 0);
	kernel_printf("[semaphore::sem_init] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
	#endif //!SEMAPHORE_DEBUG	

//以下是busy waiting 版本的实现。BUSY_WAITING_IN_SEMAPHORE宏定义在victordebug.h中
#ifdef BUSY_WAITING_IN_SEMAPHORE
/*
 *** pseudo code ***
signal(S) {
  S++;
}
*/
	//断言本信号量已经被初始化
    assert( sem->private_is_initialized == 1 , "[syncronization::up] sem->private_is_initialized == 1 \n");
	
	disable_interrupts();//关中断
	sem->value++;//other guys are busy waiting

	#ifdef SEMAPHORE_DEBUG
	//用于debug，每次执行都输出debug信息，包括信号量的值，名字，当前进程ASID等等
	kernel_puts("[semaphore::up]@DEBUG: up(semaphore *sem)[after up] .\n", 0x00f, 0);
	kernel_printf("[semaphore::sem_init] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
	#endif //!SEMAPHORE_DEBUG	

	enable_interrupts();///开中断返回；
	return;

/*
signal(semaphore *S) {
	S->value++;
	if (S->value <= 0) {
		remove a process P from S->list; wakeup(P);
	}
}
*/
//以下是进程阻塞等待版本的实现。BUSY_WAITING_IN_SEMAPHORE宏定义在victordebug.h中	
#else	//! #ifdef BUSY_WAITING_IN_SEMAPHORE	
    bool intr_flag;
	uint32_t wait_state = WT_KSEM;
    
	assert( sem->private_is_initialized == 1 , "[syncronization::up] sem->private_is_initialized == 1 \n");
	
	disable_interrupts();//关中断
    {
        //if wait queue is empty
		if( sem->head == sem->tail )//对应的wait queue中没有进程在等待
		{
			sem->value ++;//直接把信号量的value加一
		}
		else
		{
			sem->value ++;//直接把信号量的value加一//应该也要加1啊？！
			assert( pcb[sem->wait_queue[sem->head] ].wait_state == wait_state, "[up(semaphore *sem)]::pcb[sem->wait_queue[sem->head] ].wait_state == wait_state");//有进程在等待，且（首个）进程等待的原因是semophore设置的
			
			wakeup_wait(  (sem),  wait_state );//唤醒进程：设置为pcb中状态位为running，重新参与调度之中
		}
		#ifdef SEMAPHORE_DEBUG
		kernel_puts("[semaphore::up]@DEBUG: up(semaphore *sem)[after up] .\n", 0x00f, 0);
		kernel_printf("[semaphore::up] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
		#endif //!SEMAPHORE_DEBUG	
    }
    enable_interrupts();//local_intr_restore(intr_flag);//开中断返回；
	return;
#endif	//!BUSY_WAITING_IN_SEMAPHORE	
}

//Victor Dialect 
//set the first process in the queue with the state runnable, but would not delete it in the wait queue 
void
wakeup_wait(semaphore *sem, uint32_t wakeup_flags) {

	//wait_queue_del(queue, wait);
	assert(pcb[sem->wait_queue[sem->head] ].wait_state == wakeup_flags,"[syncronization::wakeup_wait]:pcb[sem->wait_queue[sem->head] ].wait_state == wakeup_flags");//有进程在等待，且（首个）进程等待的原因是semophore设置的
	assert( sem->private_is_initialized == 1 , "[syncronization::wakeup_wait] sem->private_is_initialized == 1 \n");
	assert(sem->head != sem->tail, "[syncronization::wakeup_wait] sem->head != sem->tail \n");
	
	int firstWait = sem->wait_queue[sem->head];//no need to delete
	pcb[firstWait].state = PROC_RUNNABLE;

    //wait->wakeup_flags = wakeup_flags;
	//wakeup_proc(wait->proc);

	//in the future: scheduler enqueue wakeUpASID. put it in the scheduler's ready queue
	//currently, both ready processes and waiting(sleeping) processes are in the scheduler's queue

	return;
}

/*
 * __down(semaphore *sem, uint32_t wait_state, timer_t *timer)：具体实现信号量的P操
	作，首先关掉中断，然后判断当前信号量的value是否大于0。如果是>0，则表明可以获得信
	号量，故让value减一，并打开中断返回即可；如果不是>0，则表明无法获得信号量，故需要
	将当前的进程加入到等待队列中，并打开中断，然后运行调度器选择另外一个进程执行。如
	果被V操作唤醒，则把自身关联的wait从等待队列中删除（此过程需要先关中断，完成后开中
	断）。
 */

 //@Call syscall
int re_schedule()
{
    const int syscallCode = SYS_RE_schedule;//#define SYS_printstring 
	// in #include <auxillary/unistd.h>
	
	
	asm volatile(                                //try system call
        "add $v0, $zero, %0\n\t"   // $v0: system call code
		"syscall\n\t"
		
		:
		: "r"( syscallCode )		//input
        );	
	
	return 0;
}

//@User
void 
down(semaphore *sem ) {
	uint32_t wait_state = WT_KSEM;
	
	#ifdef SEMAPHORE_DEBUG
	//用于debug，每次执行都输出debug信息，包括信号量的值，名字，当前进程ASID等等
	kernel_puts("[semaphore::down]@DEBUG: down(semaphore *sem)[before down] .\n", 0x00f, 0);
	kernel_printf("[semaphore::down] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
	#endif //!SEMAPHORE_DEBUG	

//以下是busy waiting 版本的实现。BUSY_WAITING_IN_SEMAPHORE宏定义在victordebug.h中
#ifdef BUSY_WAITING_IN_SEMAPHORE	
/*
 *** pseudo code ***
wait(S)
{
  while (S <= 0)
  ; // busy wait
  S--;
}
*/
	assert( sem->private_is_initialized == 1 , "[syncronization::down] sem->private_is_initialized == 1 \n");
	//断言本信号量已经被初始化
	while(sem->value <= 0);//busy waiting
	//wait until S > 0, then decrement S;

	disable_interrupts();//local_intr_save(intr_flag);//关中断
	sem->value--;//critical section
	enable_interrupts();//local_intr_restore(intr_flag);//开中断返回；
	return;
	


//以下是进程阻塞等待版本的实现。BUSY_WAITING_IN_SEMAPHORE宏定义在victordebug.h中
/*
wait(semaphore *S) {
    S->value--;
    if (S->value < 0) {
        add this process to S->list; block();
    }
}
*/
#else  //! #ifdef BUSY_WAITING_IN_SEMAPHORE
	assert( sem->private_is_initialized == 1 , "[syncronization::down] sem->private_is_initialized == 1 \n");
	assert(pcb[curr_proc].state  == PROC_RUNNABLE, "[syncronization::down] pcb[curr_proc].state  == PROC_RUNNABLE");
	disable_interrupts();//关掉中断
	
	sem->value --;//表明可以获得信号量，故让value减一//Note: 应该都要减去1才对

	#ifdef SEMAPHORE_DEBUG
	//用于debug，每次执行都输出debug信息，包括信号量的值，名字，当前进程ASID等等
	kernel_puts("[after down]", 0x00f, 0);
	kernel_printf("value: %d. \n" , sem->value );
	#endif //!SEMAPHORE_DEBUG	
	
    if (sem->value >= 0) {//断当前信号量的value是否大于0。
        //sem->value --;//表明可以获得信号量，故让value减一
        enable_interrupts();//打开中断返回
        return;//no one is using, allocation succeeded
    }
	
	//else 无法获得信号量
	//将当前的进程加入到等待队列中，并打开中断

	#ifdef SEMAPHORE_DEBUG
		kernel_puts("[after down] call wait_current_set\n", 0x00f, 0);
	#endif //!SEMAPHORE_DEBUG	
	wait_current_set(sem, wait_state);//将当前的进程加入到等待队列中，
	
    enable_interrupts();//local_intr_restore(intr_flag);//并打开中断
	
#ifdef SEMAPHORE_DEBUG
	kernel_puts("[after down] call re_schedule\n", 0x00f, 0);
#endif //!SEMAPHORE_DEBUG	
    re_schedule();//然后运行调度器选择另外一个进程执行。
    //至此失去控制权，不再往下执行，直至被唤醒。上下文将保存在pcb中
	
	
	//被V操作唤醒时，从此处开始执行
    disable_interrupts();//local_intr_save(intr_flag);//先关中断
#ifdef SEMAPHORE_DEBUG
	kernel_puts("[after down] wake up again, call wait_current_del\n", 0x00f, 0);
#endif //!SEMAPHORE_DEBUG		
    wait_current_del(sem, curr_proc);//则把自身关联的wait从等待队列中删除
	
	
    enable_interrupts();//local_intr_restore(intr_flag);//打开中断

	#ifdef SEMAPHORE_DEBUG
	kernel_puts("[semaphore::down]@DEBUG: up(semaphore *sem)[after down] .\n", 0x00f, 0);
	kernel_printf("[semaphore::down] curr_proc: %d, name:%s    SEM:%s  value: %d. \n" , curr_proc, pcb[curr_proc].name ,sem->name, sem->value );
	#endif //!SEMAPHORE_DEBUG	

	return;
#endif  // ! #ifdef BUSY_WAITING_IN_SEMAPHORE	
}
void//delete the current 
wait_current_del(semaphore *sem, int curr_proc)
{
	assert( sem->private_is_initialized == 1 , "[syncronization::wait_current_del] sem->private_is_initialized == 1 \n");
	//if (wait_in_queue(sem, curr_proc)) 
	{                                          
		wait_queue_del(sem, curr_proc);                                    
	}                                                                   
	
    return;
}


//return the process ASID that is deleted
int//! not used yet
wait_queue_first_del(semaphore *sem, uint32_t wait_state)
{
	assert( sem->private_is_initialized == 1 , "[syncronization::wait_queue_first_del] sem->private_is_initialized == 1 \n");
	
	assert(sem->head >=0 && sem->head <= MAX_PID-1, "[syncronization::wait_queue_first_del] Coming: sem->head >=0 && sem->head <= MAX_PID-1 \n");
	assert(sem->tail >=0 && sem->tail <= MAX_PID-1, "[syncronization::wait_queue_first_del] Coming: sem->tail >=0 && sem->tail <= MAX_PID-1 \n");
	
	assert(sem->head != sem->tail, "[syncronization::wait_queue_first_del] sem->head != sem->tail \n");
	
//	if(sem->head == sem->tail)//empty queue
//		return 0;//not in queue
    int i;
	int index_find = -1;//finding the item to delete, record its index

	index_find = sem->head;//delete the first
	assert(index_find != sem->tail, "[syncronization::wait_queue_first_del] index_find != sem->tail \n");
	
	/*
	for( i=sem->head; i!= sem->tail; )
	{
		if(i == MAX_PID-1)//the last 
		    i = 0;
		if(sem->wait_queue[i] == curr_proc)//found
		{
			index_find = i;//return 1;
			break;
		}
            			
	}*/
	int retASID = sem->wait_queue[index_find] ; //return the process ASID to be deleted(here the first process in the queue)

	int j = index_find;//ptr
	//if( j== )
	
	int j_next = j + 1;
	j_next = ( j_next ==MAX_PID )? 0 : j_next;//verilog style! Hahaha
	
	while( j_next!= sem->tail )//copy next to here
	{
		
		assert(sem->wait_queue[j_next] != -1, "[syncronization::wait_queue_first_del] sem->wait_queue[j_next] != -1 \n");
		sem->wait_queue[j] = sem->wait_queue[j_next];//copy
		sem->wait_queue[j_next] = -1;//making sure
		
		//update index
		j++;
		j = ( j==MAX_PID )? 0 : j;//update j
		j_next++;
		j_next = ( j_next==MAX_PID )? 0 : j_next;//update j_next
	}
	
	sem->tail--;//update tail ptr
	sem->tail = ( sem->tail==-1 )? MAX_PID-1 : sem->tail;//verilog style! Hahaha
	
	assert(sem->head >=0 && sem->head <= MAX_PID-1, "[syncronization::wait_queue_first_del] Leaving: sem->head >=0 && sem->head <= MAX_PID-1 \n");
	assert(sem->tail >=0 && sem->tail <= MAX_PID-1, "[syncronization::wait_queue_first_del] Leaving: sem->tail >=0 && sem->tail <= MAX_PID-1 \n");	
	
	return retASID;// return the process ASID that is deleted
}

void//Delete current
wait_queue_del(semaphore *sem,  int curr_proc)
{
	assert( sem->private_is_initialized == 1 , "[syncronization::wait_queue_del] sem->private_is_initialized == 1 \n");
	
	assert(sem->head >=0 && sem->head <= MAX_PID-1, "[syncronization::wait_queue_del] Coming: sem->head >=0 && sem->head <= MAX_PID-1 \n");
	assert(sem->tail >=0 && sem->tail <= MAX_PID-1, "[syncronization::wait_queue_del] Coming: sem->tail >=0 && sem->tail <= MAX_PID-1 \n");
	
	assert(sem->head != sem->tail, "[syncronization::wait_queue_del] sem->head != sem->tail \n");
	
//	if(sem->head == sem->tail)//empty queue
//		return 0;//not in queue
    int i;
	int index_find = -1;//finding the item to delete, record its index
	for( i=sem->head; i!= sem->tail; i++ )
	{
		if(i == MAX_PID)//the last 
		    i = 0;
		if(sem->wait_queue[i] == curr_proc)//found, ERROR!!! DON'T USE THIS FUNCTION
		{
			index_find = i;//return 1;
			break;
		}
            			
	}
	//int retASID = ; 

	int j=index_find;//ptr
	//if( j== )
	
	int j_next = j + 1;
	j_next = ( j_next == MAX_PID )? 0 : j_next;//verilog style! Hahaha
	
	while( j_next!= sem->tail )//copy next to here
	{
		
		assert(sem->wait_queue[j_next] != -1, "[syncronization::wait_queue_del] sem->wait_queue[j_next] != -1 \n");
		sem->wait_queue[j] = sem->wait_queue[j_next];//copy
		sem->wait_queue[j_next] = -1;//making sure
		
		//update index
		j++;
		j = ( j==MAX_PID-1 )? 0 : j;//update j
		j_next++;
		j_next = ( j_next==MAX_PID-1 )? 0 : j_next;//update j_next
	}
	
	sem->tail--;//update tail ptr
	sem->tail = ( sem->tail==-1 )? MAX_PID-1 : sem->tail;//verilog style! Hahaha
	
	assert(sem->head >=0 && sem->head <= MAX_PID-1, "[syncronization::wait_queue_del] Leaving: sem->head >=0 && sem->head <= MAX_PID-1 \n");
	assert(sem->tail >=0 && sem->tail <= MAX_PID-1, "[syncronization::wait_queue_del] Leaving: sem->tail >=0 && sem->tail <= MAX_PID-1 \n");	
	kernel_puts("[wait_queue_del] delete", 0x00f, 0);
	kernel_printf("process ASID:%d ,semaphore name:%s\n", curr_proc, sem->name );
	return;
}

/*
 * Set the pcb state and enque the wait queue of the semaphore
 */
void
wait_current_set(semaphore *sem, uint32_t wait_state) {
    
	assert( sem->private_is_initialized == 1 , "[syncronization::wait_current_set] sem->private_is_initialized == 1 \n");
	
	
	assert( curr_proc <= MAX_PID-1 &&  curr_proc >= 0 , "[scheduler_schedule] retASID <= size-1 &&  retASID >= 0  \n");
	assert( pcb[curr_proc].ASID >= 0, "[scheduler_schedule] pcb[curr_proc].ASID >= 0  \n");
	
	pcb[curr_proc].state = PROC_SLEEPING;
	pcb[curr_proc].wait_state = wait_state;

	
	semaphore_wait_queue_enque( sem, curr_proc);//put i
    return;
}



static void 
semaphore_wait_queue_enque(semaphore *ptr_one_q, int procASID)
{
	
	assert( ptr_one_q->private_is_initialized == 1 , "[syncronization::semaphore_wait_queue_enque] sem->private_is_initialized == 1 \n");
	
	//check full
	assert(ptr_one_q->head != ptr_one_q->tail + 1, "[semaphore_wait_queue_enque] The queue is full.\n");
	assert(ptr_one_q->tail <= MAX_PID-1 &&  ptr_one_q->tail >= 0 , "[semaphore_wait_queue_enque] ptr_one_q->tail <= MAX_PID-1 \n");
	
	kernel_puts("[semaphore_wait_queue_enque] enquue ", 0x00f, 0);
	kernel_printf("process ASID:%d ,semaphore name:%s\n", procASID, ptr_one_q->name );
	ptr_one_q->wait_queue[ptr_one_q->tail] = procASID;
	ptr_one_q->tail++;//move ptr
	if(ptr_one_q->tail == MAX_PID)//
	{
		//reaching the end of the queue
		ptr_one_q->tail = 0;//reset
	}
    //sem_print_wait_queues( ptr_one_q );
	return;
}


//@public
void sem_print_wait_queues(semaphore *sem)
{

    assert( sem->private_is_initialized == 1, "[sem_print_wait_queues] sem->private_is_initialized == 1;\n");
	assert( sem->head >= 0 &&  sem->head <= MAX_PID - 1 , "[sem_print_wait_queues]  sem->head >= 0 &&  sem->head <= MAX_PID - 1 \n");
	assert( sem->tail >= 0 &&  sem->tail <= MAX_PID - 1 , "[sem_print_wait_queues]  sem->tail >= 0 &&  sem->tail <= MAX_PID - 1 \n");
	
	
	{
		int j = sem->head;
		

		bool needPrintEnter = 0;
		int  debugPrintCount = 0;
		kernel_puts("[sem_print_wait_queues]", 0x0f0, 0);
		kernel_printf("\nHead %d,Tail %d\n", sem->head, sem->tail);
		//kernel_printf("sem name: %s\n;", sem->name	);

		while( j!=sem->tail )
		{
			debugPrintCount++;
			assert( debugPrintCount <= MAX_PID-1, "[sem::sem_print_wait_queues]  debugPrintCount <= MAX_PID-1 ");
			
			kernel_printf(" %d;", sem->wait_queue[j]	);
			//assert(sem->wait_queue[j_next] != -1, "[syncronization::wait_queue_del] sem->wait_queue[j_next] != -1 \n");
			assert(sem->wait_queue[j] != -1, "[sem::sem_print_wait_queues]  sem->wait_queue[j] != -1 \n");
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
			
		
	}
    return;

}
