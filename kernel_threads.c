#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "util.h"
#include "kernel_cc.h"


void start_alt_main_thread()
{
    PTCB* ptcb = cur_thread()->ptcb;

  int exitval;

  Task call = ptcb->task;
  int argl = ptcb->argl;
  void* args= ptcb->args;

  exitval = call(argl,args);

 

  sys_ThreadExit(exitval);
}





/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
    PCB* curproc = CURPROC;
    TCB* new_thread= spawn_thread(curproc, start_alt_main_thread);
    

   // if (new_thread==NULL){
   // return NOTHREAD;}
    
    

    PTCB* ptcb = (PTCB*) xmalloc(sizeof(PTCB));
     if (ptcb == NULL) {
        return NOTHREAD;  // Error: Memory allocation failed
    }
  

    /*initialize PTCB*/
    ptcb->task = task;
    ptcb->argl = argl;
    ptcb->args = args;
    ptcb->tcb = new_thread;
    ptcb->exited = 0;  
    ptcb->detached = 0;
    ptcb->refcount = 0;  // Start with 0 reference count   30/10
    ptcb->exit_cv= COND_INIT;


    new_thread->ptcb = ptcb;
   

   


    ptcb->ptcb_list_node = *rlnode_init(&ptcb->ptcb_list_node,ptcb);
    rlist_push_back(&curproc->ptcb_list, &ptcb-> ptcb_list_node);
    curproc->thread_count++;

    wakeup(ptcb->tcb);


  return (Tid_t) ptcb;
}



// Tid_t sys_CreateThread(Task task, int argl, void* args)
// {
//   PTCB* main_ptcb = xmalloc(sizeof(PTCB));

//   if(main_ptcb == NULL){
//     return NOTHREAD;
//   }

//   main_ptcb->tcb = spawn_thread(CURPROC, start_alt_main_thread);

//   main_ptcb->task = task;
//   main_ptcb->argl = argl;
//   main_ptcb->args = args;

//   main_ptcb->exit_cv = COND_INIT;
//   main_ptcb->tcb->ptcb = main_ptcb;

//   main_ptcb->detached = 0;
//   main_ptcb->exited = 0;
//   main_ptcb->refcount = 0;

//   rlnode_init(&main_ptcb->ptcb_list_node, main_ptcb);
//   rlist_push_back(&CURPROC->ptcb_list, & main_ptcb->ptcb_list_node);
//   CURPROC->thread_count++;
//   wakeup(main_ptcb->tcb);
  



//     return (Tid_t) main_ptcb;
// }





//               WIP 



//free rlnode pcb child
//check if exitval is null 



/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
  return (Tid_t) cur_thread() ->ptcb;

  
}








//               WIP 



//free rlnode pcb child
//check if exitval is null 


/**
  @brief Join the given thread.
  */

int sys_ThreadJoin(Tid_t tid, int* exitval) {
    

    
    PTCB* ptcb = (PTCB*) tid;
    rlnode *currNode = rlist_find(&CURPROC->ptcb_list, ptcb, NULL); // Points to head of PCB rl list
    
    // Check if the calling thread is trying to join itself
    if (ptcb == cur_thread()->ptcb ) {
    return -1; // Error: Cannot join itself
    }

     // Retrieve PTCB of the target thread
     
    if (currNode == NULL || ptcb->detached  ) {
        return -1; // Error: Invalid thread ID or detached
    }


    if(tid == NOTHREAD ) return -1;

    // Increment refcount since we are joining this thread
    
    ptcb->refcount++;

    // Wait for the thread to finish
    
    while (!ptcb->exited && !ptcb->detached) {
    

        kernel_wait(&ptcb->exit_cv, SCHED_USER); // Wait until exit condition is met

       
    }

    ptcb->refcount--; 
    // Assign the exit value upon wakeup

    if (exitval != NULL){
    *exitval = ptcb->exitval;
}

          // Decrement refcount after joining is complete

    
    


 // Check if refcount is zero and clean up if necessary
    if (ptcb->refcount == 0) {


        // Perform any cleanup here 
        rlist_remove(&ptcb->ptcb_list_node);
        free(ptcb);
    }




    return 0; // Success
}





/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid){

if (tid == NOTHREAD) return -1 ;

    PTCB* ptcb = (PTCB*)tid;  /* Retrieve PTCB of the target thread*/

  

  // Check if the thread is already joined or already detached
  PCB* cur_pcb = CURPROC;
  rlnode* node = rlist_find(& cur_pcb->ptcb_list, ptcb, NULL);
  if(node == NULL ){ 
    return -1;
  }



    ptcb->detached = 1;


    // Notify any potential waiters that the thread is detached
    kernel_broadcast(&ptcb->exit_cv);

    
    return 0; 
}







//Implement SysExit
 //Efoson ginetai exit ena thread meiwnetai to threadcount
// prepei na ginei kernel broadcast 
//else curproc!+1
//allakse to exitval tou ptcb 
    //prpepei to exited na ginei 1  na meiwthei to thread count an einai 0 ginetai sys exit 
  // to kernel sleep afora kathe thread pou tha kanei exit den tha mpei sto if an einai 0 to thread count 

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

     PTCB* ptcb = cur_thread()->ptcb;
     PCB* curproc = CURPROC;


     // set exit value for thread

     ptcb->exitval = exitval;
     ptcb-> exited =1;
     // Decrement thread count and notify waiters
    curproc->thread_count--;
    kernel_broadcast(&ptcb->exit_cv);



 


//If this is the last thread then clean up the process
if(curproc->thread_count==0 && get_pid(curproc) != 1){

/* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);

  } // ENDS the if


  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));


  /* 
    Do all the other cleanup we want here, close files etc. 
   */

  /* Release the args data */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }
  

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;


 
  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);
 
}






