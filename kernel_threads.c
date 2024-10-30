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
  

    /*initialize PTCB*/
    ptcb->task = task;
    ptcb->argl = argl;
    ptcb->args = args;
    ptcb->tcb = new_thread;
    ptcb-> exitval = 0;
    ptcb->detached = 0;
    CondVarInit(&ptcb->exit_cv);


    new_thread->ptcb = ptcb;
   

    Tid_t tid = (Tid_t) ptcb;


    rlnode_init(&ptcb->ptcb_list_node,ptcb);
    rlist_push_back(&curproc->ptcb_list, &ptcb-> ptcb_list_node);
    curproc->thread_count++;

    wakeup(new_thread);


  return tid;
}
















//               WIP 



//free rlnode pcb child
//check if exitval is null 



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

int sys_ThreadJoin(Tid_t tid, int* exitval) {
    Tid_t callTid = sys_ThreadSelf();

    // Check if the calling thread is trying to join itself
    if (callTid == tid) {
        return -1; // Error: Cannot join itself
    }

     // Retrieve PTCB of the target thread
    PTCB* ptcb = (PTCB*) tid;  // Use tid to obtain the target ptcb
    if (ptcb == NULL) {
        return -1; // Error: Invalid thread ID
    }

    // Retrieve PTCB of the target thread
    PCB* pcb = CURPROC;  
    rlnode *currNode = pcb->ptcb_list.next; // Points to head of PCB rl list
    boolean found = false;

    // Traverse through the rlnodelist of the PCB
    while (currNode != &pcb->ptcb_list) {
        if (currNode == &ptcb->ptcb_list_node) {
            found = true;
            break;
        }
        currNode = currNode->next;
    }

    if (!found) {
        return -1; // Error: Thread ID is invalid
    }

    // Check if the thread is detached or does not exist
    if (ptcb == NULL || ptcb->detached) {
        return -1; // Error: Thread is invalid or detached
    }

    // Increment refcount since we are joining this thread
    ptcb->refcount++;

    // Wait for the thread to finish
    while (ptcb->exitval == 0 && !ptcb->detached) {
    

        kernel_wait(&ptcb->exit_cv, CURTHREAD); // Wait until exit condition is met

       
    }


    // Assign the exit value upon wakeup
    *exitval = ptcb->exitval;

     ptcb->refcount--;      // Decrement refcount after joining is complete

    

 // Check if refcount is zero and clean up if necessary
    if (ptcb->refcount == 0) {
        // Perform any cleanup here (e.g., free ptcb)
        



        free(ptcb);
    }


    return 0; // Success
}




















/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid){

    PTCB* ptcb = (PTCB*)tid;  /* Retrieve PTCB of the target thread*/


    // Check if the thread is already joined
    if (ptcb->refcount > 0) {
        return -1; // Error: thread is already joined, cannot detach
    }


    ptcb->detached=1;


    // Notify any potential waiters that the thread is detached
    kernel_broadcast(&ptcb->exit_cv);

       

    return 0; 
}







/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

 //Implement SysExit
 //Efoson ginetai exit ena thread meiwnetai to threadcount
// prepei na ginei kernel broadcast 
//else curproc!+1
//allakse to exitval tou ptcb 
    //prpepei to exited na ginei 1  na meiwthei to thread count an einai 0 ginetai sys exit 
  // to kernel sleep afora kathe thread pou tha kanei exit den tha mpei sto if an einai 0 to thread count 



}