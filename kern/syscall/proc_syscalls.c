#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include <copyinout.h>
#include <synch.h>
#include <machine/trapframe.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
	
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
	#if OPT_A2
	
		pid_t currpid = p->proc_pid;
		pid_set_exitstatus(currpid, exitcode);
		pid_set_has_exited(currpid, true);
		
		struct lock *exitLock = pid_get_exit_lock(currpid);
		struct cv *exitCV = pid_get_exit_cv(currpid);
		
		lock_acquire(exitLock);
		cv_broadcast(exitCV, exitLock);
		lock_release(exitLock);
	#else
	
		(void)exitcode;
	
	#endif  /* OPT_A2 */
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
	#if OPT_A2
		*retval = curproc->proc_pid;
	#else  	
		*retval = 1;
	#endif  /* OPT_A2 */
	
	return 0;
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  
  if (options != 0) {
    return(EINVAL);
  }
  
  #if OPT_A2
	if(!pid_check_if_exists(pid)){
		return ESRCH;
	}	
	
	if(pid_get_parent_pid(pid) != curproc->proc_pid){
		return ECHILD;
	}
	
	struct lock *exitLock = pid_get_exit_lock(pid);
	struct cv *exitCV = pid_get_exit_cv(pid);
	lock_acquire(exitLock);
	while(!pid_get_has_exited(pid)) {
		cv_wait(exitCV, exitLock);
	}	
	lock_release(exitLock);
	
	
	exitstatus = _MKWAIT_EXIT(pid_get_exitstatus(pid));
	
	if(status == NULL){
		return EFAULT;
	}
  #else
	  /* for now, just pretend the exitstatus is 0 */
	  exitstatus = 0;
  #endif /* OPT_A2 */
  
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
static void entrypoint(void * ptr, unsigned long unusedval){

  (void)unusedval;

  enter_forked_process((struct trapframe *) ptr);
}

int sys_fork(struct trapframe *parent_trapframe, pid_t *retval)
{
  KASSERT(curproc != NULL);
  KASSERT(parent_trapframe != NULL);
  KASSERT(retval != NULL);
  
  int result;
  struct proc *parent_proc = curproc;

  struct trapframe *child_trapframe = kmalloc(sizeof(struct trapframe));
  
  if (child_trapframe == NULL){
    return ENOMEM;
  }
  
  *child_trapframe = *parent_trapframe;
  
  struct proc *child_proc = proc_create_runprogram("child_proc");
  if (child_proc == NULL) {
	kfree(child_trapframe);
    return(ENPROC);
  }
  
  result = as_copy(parent_proc->p_addrspace, &child_proc->p_addrspace);
  if(result) {
	kfree(child_trapframe);
	proc_destroy(child_proc);
	return result;
  }

  pid_set_parent_pid(child_proc->proc_pid, parent_proc->proc_pid);
  
  result = thread_fork("child_thread", child_proc, entrypoint,
    child_trapframe, 0);
  
  if (result) {
    as_destroy(child_proc->p_addrspace);
    proc_destroy(child_proc);
    kfree(child_trapframe);
    return result;
  }

  *retval = child_proc->proc_pid;
  
  return 0;
}
#endif /* OPT_A2 */

