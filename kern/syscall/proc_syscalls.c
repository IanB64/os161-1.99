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

//ASST2
#include "opt-A2.h"
#include <copyinout.h>
#include <synch.h>
#include <machine/trapframe.h>
//ASST2

//ASST2b
#include <vfs.h>
#include <kern/fcntl.h>
#include <limits.h>
//ASST2b

/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode)
{

    struct addrspace *as;
    struct proc *p = curproc;
    /* for now, just include this to keep the compiler from complaining about
       an unused variable */
#if OPT_A2

    pid_set_exit(curproc->pid_node, exitcode);

#else

    (void)exitcode;

#endif //OPT_A2
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

#if OPT_A2

    lock_acquire(pid_lock);
    cv_broadcast(pid_cv, pid_lock);
    lock_release(pid_lock);

    pid_set_children_not_interested(p->pid_node);

#endif//OPT_A2

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

    *retval = pid_getpid(curproc->pid_node);
#else

    *retval = 1;

#endif //OPT_A2

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

    struct pid_node_t *child = pid_find_child(curproc->pid_node, pid);
    if(!child) {
        return ECHILD;
    }

    lock_acquire(pid_lock);
    while(!pid_is_exited(child)) {
        cv_wait(pid_cv, pid_lock);
    }
    lock_release(pid_lock);

    exitstatus = _MKWAIT_EXIT(pid_get_exitcode(child));

    if(status == NULL) {
        return EFAULT;
    }
#else
    /* for now, just pretend the exitstatus is 0 */
    exitstatus = 0;
#endif //OPT_A2

    result = copyout((void *)&exitstatus,status,sizeof(int));
    if (result) {
        return(result);
    }
    *retval = pid;
    return(0);
}

#if OPT_A2

//helper for thread_fork(),it need a two arguments fun pointer.
//but enter_forked_process only need one.
static void entrypoint(void * ptr, unsigned long unusedval)
{

    (void)unusedval;

    enter_forked_process((struct trapframe *) ptr);
}

//user process fork handler
int sys_fork(struct trapframe *tf, pid_t *retval)
{
    KASSERT(curproc != NULL);
    KASSERT(tf != NULL);
    KASSERT(retval != NULL);

    int result;

    struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));

    if (child_tf == NULL) {
        return ENOMEM;
    }

    //copy trapframe to its child
    *child_tf = *tf;
    //create a child process
    struct proc *child_proc = proc_create_runprogram(curproc->p_name);
    if (child_proc == NULL) {
        kfree(child_tf);
        return(ENPROC);
    }
    //copy address space to its child
    result = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
    if(result) {
        kfree(child_tf);
        proc_destroy(child_proc);
        return result;
    }

    //add a new child
    pid_add_child(curproc->pid_node, child_proc->pid_node);

    result = thread_fork(curthread->t_name, child_proc, entrypoint,
                         child_tf, 0);

    if (result) {
        as_destroy(child_proc->p_addrspace);
        proc_destroy(child_proc);
        kfree(child_tf);
        return result;
    }

    //return child's pid
    *retval = pid_getpid(child_proc->pid_node);
    return 0;
}

//user process execv handler
int
execv(userptr_t progname, userptr_t args)
{
    if(progname == NULL) {
        return ENOENT;
    }

    if(args == NULL) {
        return EFAULT;
    }

    if(strlen((char *)progname) > PATH_MAX) {
        return E2BIG;
    }

    vaddr_t entrypoint, stackptr;
    int result;
    size_t actual_len;//just for  the last argument of copyoutsrt()

    char **argv_from = (char **)args;

    //open file
    struct vnode *v;
    result = vfs_open((char *)progname, O_RDONLY, 0, &v);
    if(result) {
        return result;
    }

    // count for the number of arguments
    int argc = 0;
    while(argv_from[argc] != NULL) {
        argc++;
    }

    //request memory for arguments pointer temporarily
    char **argv_kernel = kmalloc(sizeof(char *) * argc);
    if(argv_kernel == NULL) {
        return ENOMEM;
    }

    //copy in arguments from old user space of old user process to kernel
    for(int i = 0; i < argc; i++) {
        argv_kernel[i] = kmalloc(ARG_MAX);
        if(argv_kernel[i] == NULL) {
            return ENOMEM;
        }
        result = copyinstr((const_userptr_t) argv_from[i], argv_kernel[i], ARG_MAX, &actual_len);
        if(result) {
            return result;
        }

    }

    //new userspace prepare: create as, load elf, set stackptr

    struct addrspace *new_as = as_create();
    if(new_as == NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    struct addrspace *old_as = curproc_setas(new_as);
    as_activate();
    as_destroy(old_as);

    result = load_elf(v,&entrypoint);
    if(result) {
        vfs_close(v);
        return result;
    }

    vfs_close(v);

    result = as_define_stack(new_as, &stackptr);
    if(result) {
        return result;
    }

    //arguments pointers for storing pointer to aguments in userspace stack
    vaddr_t *argv_to = (vaddr_t *)kmalloc(sizeof(vaddr_t) * argc);

    //Copy out arguments from kernel to user space
    for(int i = 0; i < argc; i++) {
        int len = strlen(argv_kernel[i]) + 1;
        stackptr -= ROUNDUP(len, 8);
        argv_to[i] = (vaddr_t)stackptr;
        result = copyoutstr(argv_kernel[i], (userptr_t) stackptr, len, &actual_len);
        if(result) {
            return result;
        }
    }

    //copy out arguments pointers from kernel to user space
    argv_to[argc] = 0;
    for(int i = argc; i >= 0; i--) {
        stackptr -= sizeof(vaddr_t);
        result = copyout(&argv_to[i], (userptr_t)stackptr, sizeof(vaddr_t));
        if(result) {
            return result;
        }
    }

    //free all allocated memory
    for(int i = 0; i < argc; i++) {
        kfree(argv_kernel[i]);
    }
    kfree(argv_kernel);
    kfree(argv_to);

    /* Warp to user mode. */

    //enter new process(user space)
    //argument painter is the same with stack pointer when user program begin to run
    enter_new_process(argc,  (userptr_t)stackptr, stackptr, entrypoint);

    /* enter_new_process does not return. */
    panic("enter new process returned\n");
    return EINVAL;
}

#endif //OPT_A2



