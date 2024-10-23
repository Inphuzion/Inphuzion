
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
    PCB* curproc = CURPROC;
    TCB* new_thread= spawn_thread(curproc, start_alt_main_thread);
    

    if (new_thread==NULL){
      return NOTHREAD;}
    
    

    PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));
  

    //initialize the PTCB with task parameters
    ptcb->task = task;
    ptcb->argl = argl;
    ptcb->args = args;
    ptcb->tcb = new_thread;
    ptcb-> exitval = 0;


    new_thread->ptcb = ptcb;
   


    rlnode_init(&ptcb->ptcb_list_node,ptcb);
    rlist_push_back(&curproc->ptcb_list, &ptcb-> ptcb_list_node);
    curproc->thread_count++;



    wakeup(new_thread);





	return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD ->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

