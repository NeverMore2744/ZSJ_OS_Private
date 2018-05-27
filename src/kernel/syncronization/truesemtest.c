
#include "../victordebug.h"
//#include <stdio.h>
//#include <proc.h>
#include <zjunix/pc.h>
#include <intr.h>//enable/disable interrupt
//#include <sem.h>
#include <victor/semaphore.h>
//#include <monitor.h>
#include <assert.h>
#include <zjunix/utils.h>
#include <driver/vga.h>
#include <driver/ps2.h>

#define SLEEP_BUSY_WAITING_COUNT 8388480//5

static const int N_true = 2;//the number of process 

static void print_semaphore_and_state(void);

//for stepping to debug, this input would be discarded
////stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
static inline void 
STEP_SYNC_DINING_DEBUG_GETCHAR(void)                  
{                                                                            
     {                                                                          
        #ifdef SYNC_DINING_DEBUG_STEP
        kernel_printf("[truesemtest::STEP_SYNC_DINING_DEBUG_GETCHAR] curr_proc: %d, name:%s  \n" , curr_proc, pcb[curr_proc].name  );
        print_semaphore_and_state();
        //kernel_getchar();                                                       
        #endif
        return;
     }                                                                           
}

//There is one global semaphore to provide mutual exclusion for exectution of critical protocols.
static semaphore mutex; /* 临界区互斥 */
static semaphore mutex2; /* 临界区互斥 */

//@Debug: print all kinds of imformation about the current state including semaphores.
static void 
print_semaphore_and_state(void)
{
    disable_interrupts();//local_intr_save(intr_flag);//关中断
    {
        kernel_puts("[print_semaphore_and_state] Prints States.\n", 0x0ff, 0);
        kernel_printf("semaphore mutex: %d \n",mutex.value);
        sem_print_wait_queues(&mutex);
        kernel_printf("semaphore mutex2: %d \n",mutex2.value);
        sem_print_wait_queues(&mutex2);

    }
    enable_interrupts();//local_intr_restore(intr_flag);//开中断返回；

    kernel_getchar();



    return;
}

//MAIN FUCNTION     
static const int iTotalRunTimes = 5;
static int 
wait_signal_semaphore(void * arg)
{
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    
    kernel_puts("[testing_ture _semaphore]I am wait_signal_semaphore(void * arg).\n", 0x00f, 0);
    int iter = 0;
    while(iter++<iTotalRunTimes)
    {
        kernel_puts("[wait_signal_semaphore] before down(&mutex); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        down(&mutex); /* 进入临界区 */
        kernel_puts("[wait_signal_semaphore] after down(&mutex); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
        kernel_puts("[wait_signal_semaphore] before up(&mutex2); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        up(&mutex2); /* 进入临界区 */
        kernel_puts("[wait_signal_semaphore] after up(&mutex2); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
         
    }
    return 0;
}

static int 
signal_wait_semaphore(void * arg)
{
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    
    kernel_puts("[testing_ture _semaphore]I am signal_wait_semaphore(void * arg).\n", 0x00f, 0);
    int iter = 0;
    while(iter++<iTotalRunTimes)
    {
        kernel_puts("[signal_wait_semaphore] before up(&mutex); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        up(&mutex); /* 进入临界区 */
        kernel_puts("[signal_wait_semaphore] after up(&mutex); .\n", 0xf0f, 0);

        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
        kernel_puts("[signal_wait_semaphore] before down(&mutex2); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        down(&mutex2); /* 进入临界区 */
        kernel_puts("[signal_wait_semaphore] after down(&mutex2); .\n", 0xf0f, 0);
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
         
    }
    return 0;
}

static int 
true_using_semaphore(void * arg) /* i：哲学家号码，从0到N-1 */
{

    int i, iter=0;
    i=(int)arg;//passing parameter using $a0

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    
    
    kernel_puts("[philosopher_using_semaphore]I am a philosopher.\n", 0x00f, 0);
    
    if( i==1 ) 
    up(&mutex2); /* 离开临界区 */

    down(&mutex); /* 进入临界区 */
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    
    while(iter++<iTotalRunTimes)
    { /* 无限循环，其实是有限循环，循环次数见TIMES */
        
        
        kernel_printf("[philosopher_using_semaphore] Iter %d, No.%d philosopher_sema is thinking\n",iter,i); /* 哲学家正在思考 */
        if( i==0 && iter == iTotalRunTimes/2 )//0 child, stop it
        {
            kernel_puts("[philosopher_using_semaphore] i==0 && iter == iTotalRunTimes/2.\n", 0x0f0, 0);
            down(&mutex2); /* 进入临界区 */

        }
        
        
    }
    up(&mutex); /* 离开临界区 */
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    
    return 0;

}

// My caller is: 
// static int init_main(void *arg)  - the second kernel thread used to create user_main kernel threads
// pid = 1, init
// NOTE: DO_WAIT() has not been implemented
static char names_of_true[][32] = {
    "true sem test 0",
    "true sem test 1",
    "true sem test 2",
    "true sem test 3",
    "true sem test 4",
    "true sem test 5"
};

//syncronization checking version 2
//this function never return. kernel::syncronization::truesemtest.c
void true_sem_check_sync(void){
    
        int i;//index for loop
        kernel_puts("[truesemtest]void true_sem_check_sync(void).\n", 0x00f, 0);
        //STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
            

        kernel_memset( &mutex , 0, sizeof(mutex));//semaphore mutex; /* 临界区互斥 */
        kernel_memset( &mutex , 0, sizeof(mutex2));//semaphore mutex; /* 临界区互斥 */
        //check semaphore
        //STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
    
        sem_init(&mutex, 0, "mutex for critical section");/* 临界区互斥 */
        sem_init(&mutex2, 0, "mutex2 another mutex");/* 临界区互斥 */
        
        for(i=0;i<N_true;i++)
        {  /* 哲学家数目 */
    
            //STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
            
            int pid;
            //pid = kernel_thread(true_using_semaphore, (void *)i, 0);/* 每个哲学家一个进程 */
            if(i==0)
            pid = kernel_thread(wait_signal_semaphore, (void *)i, 0);
            else
            pid = kernel_thread(signal_wait_semaphore, (void *)i, 0);

            if (pid <= 1) {
                kernel_printf("[check_sync]  kernel_thread return ASID: %d. \n" , pid );
                assert( pid >= 2 , "[check_sync] create No.%d philosopher_using_semaphore failed.\n");
            }
            kernel_strcpy(pcb[pid].name, names_of_true[i] );/* 每个哲学家一个进程 */
            //struct proc_struct *philosopher_proc_sema[N];
            //philosopher_proc_sema[i] = find_proc(pid);
            //set_proc_name(philosopher_proc_sema[i], "philosopher_sema_proc");
            int i_busy_waiting = 0;    
    
            while(++i_busy_waiting !=SLEEP_BUSY_WAITING_COUNT);//waiting for a while//do_sleep(SLEEP_TIME);
       
    
        }//for(i=0;i<N;i++) /* 哲学家数目 */
        kernel_puts("[true_sem_check_sync] Critical: This function would not return.\n", 0x00f, 0);
        while(1);//never return
        return;
    
    }
    