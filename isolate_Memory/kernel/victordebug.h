#ifndef __VICTOR_KERNEL_DEBUG_H__
#define __VICTOR_KERNEL_DEBUG_H__

//Class Schedule
// schedule(sched.c): display multilevel feed back queue imformation, and step.
//#define SCHED_MLFBQ_DEBUG

//Class Syncronization
// semaphore(semaphore.[c]): display multilevel feed back queue imformation, and step.
//using kernel_getchar();    //the first input would be discarded
#define SEMAPHORE_DEBUG

//Class CheckSyncronization
// diningphilosophers(.[c]): step, and display
//using kernel_getchar();    //the first input would be discarded
#define SYNC_DINING_DEBUG_STEP

#endif //! __VICTOR_KERNEL_DEBUG_H__

