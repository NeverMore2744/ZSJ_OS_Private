/*
  *** 哲学家就餐问题回顾 ***
哲学家就餐问题描述如下：有五个哲学家，他们的生活方式是交替地进行思考
和进餐。哲学家们公用一张圆桌，周围放有五把椅子，每人坐一把。
在圆桌上有五个碗和五根筷子，当一个哲学家思考时，他不与其他人交谈，
饥饿时便试图取用其左、右最靠近他的筷子，但他可能一根都拿不到。
只有在他拿到两根筷子时，方能进餐，进餐完后，放下筷子又继续思考。
*/

/*
    **** 哲学家就餐问题的解决： Tannenbaum’s Algorithm ****
https://www.cs.indiana.edu/classes/p415-sjoh/hw/project/dining-philosophers/index.htm
Dining Philosophers. There is a dining room containing a circular table with five chairs. At each chair is a plate, and between each plate is a single chopstick. In the middle of the table is a bowl of spaghetti. Near the room are five philosophers who spend most of their time thinking, but who occasionally get hungry and need to eat so they can think some more.
In order to eat, a philosopher must sit at the table, pick up the two chopsticks to the left and right of a plate, then serve and eat the spaghetti on the plate.
Thus, each philosopher is represented by the following pseudocode:
        process P[i]
          while true do
            { THINK;
              PICKUP(CHOPSTICK[i], CHOPSTICK[i+1 mod 5]);
              EAT;
              PUTDOWN(CHOPSTICK[i], CHOPSTICK[i+1 mod 5])
             }
A philosopher may THINK indefinately. Every philosopher who EATs will eventually finish. Philosophers may PICKUP and PUTDOWN their chopsticks in either order, or nondeterministically, but these are atomic actions, and, of course, two philosophers cannot use a single CHOPSTICK at the same time.
The problem is to design a protocol to satisfy the liveness condition: any philosopher who tries to EAT, eventually does.
Tannenbaum's Solution. This solution uses only boolean semaphors. There is one global semaphore to provide mutual exclusion for exectution of critical protocols. There is one semaphore for each chopstick. In addition, a local two-phase prioritization scheme is used, under which philosophers defer to their neighbors who have declared themselves "hungry." All arithmetic is modulo 5.
system DINING_PHILOSOPHERS

VAR
me:    semaphore, initially 1;                    // for mutual exclusion 
s[5]:  semaphore s[5], initially 0;                //for synchronization 
pflag[5]: {THINK, HUNGRY, EAT}, initially THINK;   //philosopher flag 
As before, each philosopher is an endless cycle of thinking and eating.
procedure philosopher(i)
  {
    while TRUE do
     {
       THINKING;
       take_chopsticks(i);
       EATING;
       drop_chopsticks(i);
     }
  }
The take_chopsticks procedure involves checking the status of neighboring philosophers and then declaring one's own intention to eat. This is a two-phase protocol; first declaring the status HUNGRY, then going on to EAT.
procedure take_chopsticks(i)
  {
    DOWN(me);               // critical section 
    pflag[i] := HUNGRY;
    test[i];
    UP(me);                 // end critical section 
    DOWN(s[i])              // Eat if enabled 
   }

void test(i)            // Let phil[i] eat, if waiting 
  {
    if ( pflag[i] == HUNGRY
      && pflag[i-1] != EAT
      && pflag[i+1] != EAT)
       then
        {
          pflag[i] := EAT;
          UP(s[i])
         }
    }
Once a philosopher finishes eating, all that remains is to relinquish the resources---its two chopsticks---and thereby release waiting neighbors.
void drop_chopsticks(int i)
  {
    DOWN(me);                // critical section 
    test(i-1);               // Let phil. on left eat if possible 
    test(i+1);               // Let phil. on rght eat if possible 
    UP(me);                  // up critical section 
   }
The protocol is fairly elaborate, and Tannenbaum's presentation is made more subtle by its coding style.

*/
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

#ifdef SYNC_DINING_DEBUG_STEP
    #include <driver/ps2.h>
#endif


#define N 5 /* 哲学家数目 */
#define LEFT (i-1+N)%N /* i的左邻号码 */
#define RIGHT (i+1)%N /* i的右邻号码 */
#define THINKING 0 /* 哲学家正在思考 */
#define HUNGRY 1 /* 哲学家想取得叉子 */
#define EATING 2 /* 哲学家正在吃面 */
#define TIMES  4 /* 吃4次饭 */
//#define SLEEP_TIME 10 //! not supported yet
#define SLEEP_BUSY_WAITING_COUNT 8388480

static void print_semaphore_and_state(void);
//for stepping to debug, this input would be discarded
////stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
static inline void 
STEP_SYNC_DINING_DEBUG_GETCHAR(void)                  
{                                                                            
     {                                                                          
        #ifdef SYNC_DINING_DEBUG_STEP
        kernel_printf("[Dining::STEP_SYNC_DINING_DEBUG_GETCHAR] curr_proc: %d, name:%s  \n" , curr_proc, pcb[curr_proc].name  );
        print_semaphore_and_state();
        kernel_getchar();                                                       
        #endif
        return;
     }                                                                           
}

//-----------------philosopher problem using semaphore ------------
/*
system DINING_PHILOSOPHERS

VAR
me:    semaphore, initially 1;                    # for mutual exclusion 
s[5]:  semaphore s[5], initially 0;               # for synchronization 
pflag[5]: {THINK, HUNGRY, EAT}, initially THINK;  # philosopher flag 

# As before, each philosopher is an endless cycle of thinking and eating.

procedure philosopher(i)
  {
    while TRUE do
     {
       THINKING;
       take_chopsticks(i);
       EATING;
       drop_chopsticks(i);
     }
  }

# The take_chopsticks procedure involves checking the status of neighboring 
# philosophers and then declaring one's own intention to eat. This is a two-phase 
# protocol; first declaring the status HUNGRY, then going on to EAT.

procedure take_chopsticks(i)
  {
    DOWN(me);               # critical section 
    pflag[i] := HUNGRY;
    test[i];// ()?
    UP(me);                 # end critical section 
    DOWN(s[i])              # Eat if enabled 
   }

void test(i)                # Let phil[i] eat, if waiting 
  {
    if ( pflag[i] == HUNGRY
      && pflag[i-1] != EAT
      && pflag[i+1] != EAT)
       then
        {
          pflag[i] := EAT;
          UP(s[i])  //???
         }
    }


# Once a philosopher finishes eating, all that remains is to relinquish the 
# resources---its two chopsticks---and thereby release waiting neighbors.

void drop_chopsticks(int i)
  {
    DOWN(me);                # critical section 
    test(i-1);               # Let phil. on left eat if possible 
    test(i+1);               # Let phil. on rght eat if possible 
    UP(me);                  # up critical section 
   }

*/
//---------- philosophers problem using semaphore ----------------------
static int state_sema[N]; /* 记录每个人状态的数组 */
/* 信号量是一个特殊的整型变量 */

//There is one global semaphore to provide mutual exclusion for exectution of critical protocols.
static semaphore mutex; /* 临界区互斥 */

//There is one semaphore for each chopstick.
static semaphore s[N]; /* 每个哲学家一个信号量 */


//@Debug: print all kinds of imformation about the current state including semaphores.
static void 
print_semaphore_and_state(void)
{
    disable_interrupts();//local_intr_save(intr_flag);//关中断
    {
        kernel_puts("[print_semaphore_and_state] Prints States.\n", 0x0ff, 0);
        kernel_printf("semaphore mutex: %d \n",mutex);
        kernel_puts("semaphore s[N].", 0x00f, 0);
        int i;
        for(i=0;i<N;i++)
        {
            kernel_printf("%d ",s[i]); /* 每个哲学家一个信号量 */
        }
        kernel_puts("\nint state_sema[N]", 0x00f, 0);
        for(i=0;i<N;i++)
        {
            kernel_printf("%d ",state_sema[i]); /* 每个哲学家一个信号量 */
        }
        kernel_puts("\nTHINKING: 0 HUNGRY:1 EATING:2\n ", 0x00f, 0);
        //#define THINKING 0 /* 哲学家正在思考 */
        //#define HUNGRY 1 /* 哲学家想取得叉子 */
        //#define EATING 2 /* 哲学家正在吃面 */
    }
    enable_interrupts();//local_intr_restore(intr_flag);//开中断返回；



    return;
}

static void 
phi_test_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
    if(state_sema[i]==HUNGRY&&state_sema[LEFT]!=EATING
            &&state_sema[RIGHT]!=EATING)
    {
        state_sema[i]=EATING;
        up(&s[i]);
    }
}

static void 
phi_take_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.

    down(&mutex); /* 进入临界区 */
    state_sema[i]=HUNGRY; /* 记录下哲学家i饥饿的事实 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    

    phi_test_sema(i); /* 试图得到两只叉子 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    

    up(&mutex); /* 离开临界区 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    

    down(&s[i]); /* 如果得不到叉子就阻塞 */  //对，不是up 

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    
}

static void 
phi_put_forks_sema(int i) /* i：哲学家号码从0到N-1 */
{ 
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    
    
    down(&mutex); /* 进入临界区 */
    state_sema[i]=THINKING; /* 哲学家进餐结束 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    
        phi_test_sema(LEFT); /* 看一下左邻居现在是否能进餐 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        

        phi_test_sema(RIGHT); /* 看一下右邻居现在是否能进餐 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
        up(&mutex); /* 离开临界区 */

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
}

static int 
philosopher_using_semaphore(void * arg) /* i：哲学家号码，从0到N-1 */
{
    int i, iter=0;
    i=(int)arg;//passing parameter using $a0

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.    
    
    kernel_puts("[philosopher_using_semaphore]I am a philosopher.\n", 0x00f, 0);

    while(iter++<TIMES)
    { /* 无限循环，其实是有限循环，循环次数见TIMES */
        kernel_printf("[philosopher_using_semaphore] Iter %d, No.%d philosopher_sema is thinking\n",iter,i); /* 哲学家正在思考 */
        int i_busy_waiting = 0;
        while(++i_busy_waiting !=SLEEP_BUSY_WAITING_COUNT);//waiting for a while//do_sleep(SLEEP_TIME);
        
        kernel_printf("No.%d philosopher_sema wants to eat.\n",i); /* 进餐 */ 

#ifdef SYNC_DINING_DEBUG_STEP
        print_semaphore_and_state();//print all kinds of imformation about the current state including semaphores.
        //kernel_getchar();    //for stepping to debug, this input would be discarded
#endif              
        phi_take_forks_sema(i); 
        /* 需要两只叉子，或者阻塞 */
        assert(state_sema[i]==EATING, "[philosopher_using_semaphore] state_sema[i]==EATING");
        assert(state_sema[LEFT]!=EATING, "[philosopher_using_semaphore] state_sema[LEFT]!=EATING");
        assert(state_sema[RIGHT]!=EATING, "[philosopher_using_semaphore] state_sema[RIGHT]!=EATING");             
        kernel_printf("Iter %d, No.%d philosopher_sema is eating\n",iter,i); /* 进餐 */
        
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
        i_busy_waiting = 0; while(++i_busy_waiting !=SLEEP_BUSY_WAITING_COUNT);//waiting for a while//do_sleep(SLEEP_TIME);
             
        kernel_printf("No.%d philosopher_sema wants to put forks back.\n",i); /* 进餐 */

        phi_put_forks_sema(i); 
        /* 把两把叉子同时放回桌子 */

        

        kernel_printf("No.%d philosopher_sema succeeded in putting forks back.\n",i); /* 进餐 */
   
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.

    }
    //cprintf("%d philosopher_sema quit\n",i);
    kernel_puts("[philosopher_using_semaphore]philosopher_sema quit: No. ", 0x00f, 0);
    kernel_printf("%d\n",i); /* 哲学家 */

    return 0;    
}

// My caller is: 
// static int init_main(void *arg)  - the second kernel thread used to create user_main kernel threads
// pid = 1, init
// NOTE: DO_WAIT() has not been implemented
static char names_of_philosopher[][32] = {
    "philosopher 0",
    "philosopher 1",
    "philosopher 2",
    "philosopher 3",
    "philosopher 4",
    "philosopher 5"
};

/*
 *** void check_sync(void) ***
  *** 哲学家就餐问题的顶层函数，调用此函数可测试哲学家就餐问题 ***
对于check_sync函数的第一部分，首先实现初始化了一个互斥信号量，然后创建了对应5个哲
学家行为的5个信号量，并创建5个内核线程代表5个哲学家，每个内核线程完成了基于信号量
的哲学家吃饭睡觉思考行为实现。这部分是给学生作为练习参考用的。学生可以看看信号量
是如何实现的，以及如何利用信号量完成哲学家问题。
对于check_sync函数的第二部分，首先初始化了管程，然后又创建了5个内核线程代表5个哲
学家，每个内核线程要完成基于管程的哲学家吃饭、睡觉、思考的行为实现。这部分需要学
生来具体完成。学生需要掌握如何用信号量来实现条件变量，以及包含条件变量的管程如何
能够确保哲学家能够正常思考和吃饭。
*/
//global function: version 1
void check_sync(void){

    int i;//index for loop

    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
    kernel_memset( s , 0, sizeof(s));//semaphore s[N]; /* 每个哲学家一个信号量 */
    kernel_memset( &mutex , 0, sizeof(mutex));//semaphore mutex; /* 临界区互斥 */
    kernel_memset( state_sema , 0, sizeof(state_sema));//int state_sema[N]; /* 记录每个人状态的数组 */

    
    //check semaphore
    STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
    

    sem_init(&mutex, 1, "mutex for critical section");/* 临界区互斥 */
    for(i=0;i<N;i++)
    {  /* 哲学家数目 */

        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        

        sem_init(&s[i], 0, names_of_philosopher[i] );/* 每个哲学家一个信号量 */
        
        STEP_SYNC_DINING_DEBUG_GETCHAR(); //stepping, get char. Note that if macro SYNC_DINING_DEBUG_STEP is not defined, This function would do noting.
        
        
        int pid = kernel_thread(philosopher_using_semaphore, (void *)i, 0);/* 每个哲学家一个进程 */
        if (pid <= 1) {
            kernel_printf("[check_sync]  kernel_thread return ASID: %d. \n" , pid );
            assert( pid >= 2 , "[check_sync] create No.%d philosopher_using_semaphore failed.\n");
        }
        kernel_strcpy(pcb[pid].name, names_of_philosopher[i] );/* 每个哲学家一个进程 */
        //struct proc_struct *philosopher_proc_sema[N];
        //philosopher_proc_sema[i] = find_proc(pid);
        //set_proc_name(philosopher_proc_sema[i], "philosopher_sema_proc");
        int i_busy_waiting = 0;    

        while(++i_busy_waiting !=SLEEP_BUSY_WAITING_COUNT);//waiting for a while//do_sleep(SLEEP_TIME);
   

    }//for(i=0;i<N;i++) /* 哲学家数目 */
    kernel_puts("[check_sync] Critical: This function would not return to shell.\n", 0x00f, 0);
    while(1);//never return
    return;

}
