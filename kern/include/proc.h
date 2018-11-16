/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include <thread.h> /* required for struct threadarray */

//ASST2
#include "opt-A2.h"
//ASST2

struct addrspace;
struct vnode;
#ifdef UW
struct semaphore;
#endif // UW

#if OPT_A2

struct pid_node_t {
    struct pid_node_t *left_child;
    struct pid_node_t *right_sibling;
    bool interested;
    bool exited;
    int exitcode;
    pid_t pid;
};

#endif //OPT_A2

/*
 * Process structure.
 */
struct proc {
    char *p_name;			/* Name of this process */
    struct spinlock p_lock;		/* Lock for this structure */
    struct threadarray p_threads;	/* Threads in this process */

    /* VM */
    struct addrspace *p_addrspace;	/* virtual address space */

    /* VFS */
    struct vnode *p_cwd;		/* current working directory */

#ifdef UW
    /* a vnode to refer to the console device */
    /* this is a quick-and-dirty way to get console writes working */
    /* you will probably need to change this when implementing file-related
       system calls, since each process will need to keep track of all files
       it has opened, not just the console. */
    struct vnode *console;                /* a vnode for the console device */
#endif

    /* add more material here as needed */

#if OPT_A2

    struct pid_node_t *pid_node;

#endif //OPT_A2
};

#if OPT_A2
extern struct lock *pid_lock;
extern struct cv *pid_cv;

struct pid_node_t *pid_create(void);
void pid_set_children_not_interested(struct pid_node_t *pid_node);
void pid_destroy(struct pid_node_t * pid_tree_root);
pid_t pid_getpid(struct pid_node_t *pid_node);
struct pid_node_t *pid_find_child(struct pid_node_t *parent, pid_t child_pid);
void pid_add_child(struct pid_node_t *parent, struct pid_node_t *child);
void pid_set_exit(struct pid_node_t *pid_node, int exitcode);
bool pid_is_exited(struct pid_node_t *pid_node);
int pid_get_exitcode(struct pid_node_t *pid_node);

#endif //OPT_A2

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Semaphore used to signal when there are no more processes */
#ifdef UW
extern struct semaphore *no_proc_sem;
#endif // UW

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *curproc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *curproc_setas(struct addrspace *);


#endif /* _PROC_H_ */

