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
#include <mips/trapframe.h>
#include <array.h>
#include <synch.h>
#include <vfs.h>
#include <kern/fcntl.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */


#if OPT_A2
void sys__exit(int exitcode) {
  lock_acquire(lock);
  struct process_array *comrade;
  for (unsigned int i = 0;  i < array_num(china); i++) {
     comrade = array_get(china, i);
     //u found the struct
    if (comrade->self == curproc->pid){
      break;
    }
  }

  if(comrade->daddy != -1){
    struct process_array *comrade_dad;
    for (unsigned int i = 0;  i < array_num(china); i++) {
      comrade_dad = array_get(china, i);
      //u found the daddy
      if (comrade_dad->self == comrade->daddy){
        break;
      }
    }

    //if the parent still exist
    if(comrade_dad->health == 1){
      //make urself a zombie
      comrade->health = 2;
      //leave the exit code for ur daddy
      comrade->e_code = _MKWAIT_EXIT(exitcode);
      //call ur daddy incase its sleeping
      cv_signal(waitpid_cv, lock);
      //if ur dad is dead
    } else if (comrade_dad->health == 3 || comrade_dad->health == 2){
      comrade->health = 3;
    }
  } else {
    comrade->health=3;
  }

  for (unsigned int i = 0;  i < array_num(china); i++) {
    struct process_array *people = array_get(china, i);
    if(people->daddy == comrade->self && people->health == 2){
      people->health = 3;
    }
  }
  lock_release(lock);

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

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
#endif

#if OPT_A2

int
sys_fork(pid_t *retval, struct trapframe *tf){
  struct proc *cp = proc_create_runprogram(curproc->p_name);
  if(!cp){
    return ENPROC;
  }
  as_copy(curproc_getas(), &(cp->p_addrspace));
  if (!(cp->p_addrspace)){
    proc_destroy(cp);
    return ENOMEM;
  }
  struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
  if (!child_tf){
    proc_destroy(cp);
    return ENOMEM;
  }
  memcpy(child_tf,tf, sizeof(struct trapframe));
  int pass = thread_fork(curthread->t_name, cp, &enter_forked_process, child_tf, 1);
  if (pass){ //fails
    proc_destroy(cp);
    kfree(child_tf);
    return pass;
  }

  for(unsigned int i = 0;  i < array_num(china); i++){
    struct process_array *people = array_get(china, i);
    if (people->self == cp->pid){
      people->daddy = curproc->pid;
    }
  }

  *retval = cp->pid;
  return 0;
}
#endif

#if OPT_A2
/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}
#endif
/* stub handler for waitpid() system call                */

#if OPT_A2
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  lock_acquire(lock);
  struct process_array *baby;
  //get ur process struct
  for (unsigned int i = 0;  i < array_num(china); i++) {
    baby = array_get(china, i);
    if (baby->self == pid){
      break;
    }
  }

  if(baby->self != pid){
    lock_release(lock);
    return(ESRCH);
  }

  if (baby->daddy != curproc->pid){
    lock_release(lock);
    return(ECHILD);
  }

  //wait if ur baby is still alive
  while(1 == baby->health) {
    cv_wait(waitpid_cv, lock);
  }

  lock_release(lock);

  if (options != 0) {
    return(EINVAL);
  }

  exitstatus = baby->e_code;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}
#endif


#if OPT_A2
int sys_execv(userptr_t proc_name, userptr_t args){
  	struct addrspace *as;
    struct addrspace *as_die;
  	struct vnode *v;
  	vaddr_t entrypoint, stackptr;
  	int result;

    //mine
    //
    if(proc_name == NULL){
      return ENOENT;
    }

    int counter =0;
    int program_counter = 0;
    userptr_t k_args;
    char* k_otherargs;

    while(k_args !=0){
      result = copyin(args + program_counter, &k_args, sizeof(userptr_t));
      if(result){
        return result;
      }
      if(k_args != 0){
        counter++;
        program_counter += 4;
      } else {
        break;
      }
    }

    char** copied = kmalloc(sizeof(char) * counter);
    if(copied == NULL){
      return result;
    }

    program_counter = 0;
    int i;
    for (i = 0; i < counter; i++){
      result = copyin(args + program_counter, &k_otherargs, sizeof(userptr_t));
      if(result){
        return result;
      }else{
        if (!result){
          copied[i] = kmalloc((strlen(k_otherargs) + 1) * sizeof(char));
          result = copyinstr((userptr_t)k_otherargs, copied[i], strlen(k_otherargs) + 1, NULL);
          if(result){
            return result;
          }
          program_counter = program_counter + 4;
        }
      }
    }

    char* theproc = kmalloc(sizeof(char) * (strlen((char*)proc_name) +1) );
    if(theproc ==NULL){
      return ENOMEM;
    }

    result = copyinstr(proc_name, theproc, strlen((char*)proc_name) + 1, NULL);
    if(result){
      return result;
    }

  	/* Open the file. */
    //END OF first part

    char *fname_temp;
    fname_temp = kstrdup((char*)proc_name);
  	result = vfs_open(fname_temp, O_RDONLY, 0, &v);
    kfree(fname_temp);
  	if (result) {
  		return result;
  	}

  	/* We should be a new process. */
  	/* Create a new address space. */
  	as = as_create();
  	if (as ==NULL) {
  		vfs_close(v);
  		return ENOMEM;
  	}

  	/* Switch to it and activate it. */
  	as_die = curproc_setas(as);
  	as_activate();

  	/* Load the executable. */
  	result = load_elf(v, &entrypoint);
  	if (result) {
  		/* p_addrspace will go away when curproc is destroyed */
  		vfs_close(v);
  		return result;
  	}

  	/* Done with the file now. */
  	vfs_close(v);

  	/* Define the user stack in the address space */
  	result = as_define_stack(as, &stackptr);
  	if (result) {
  		/* p_addrspace will go away when curproc is destroyed */
  		return result;
  	}

    ///hard part

    vaddr_t arrayy[1+counter];
    arrayy[counter] = 0;

    int iter = counter - 1;
    while (iter > -1){
      stackptr = stackptr - ROUNDUP(strlen(copied[iter]) +1, 8);
      result = copyoutstr(copied[iter], (userptr_t)stackptr, strlen(copied[iter])+1, NULL);
      if(!result){
        arrayy[iter] = stackptr;
        iter--;
      }
      if(result){
          return result;
      }
    }

    for(int i = counter; i > -1 ; i--){
      stackptr = stackptr - ROUNDUP(sizeof(vaddr_t), 4);
      result = copyout(&arrayy[i], (userptr_t)stackptr, sizeof(vaddr_t));
      int printres = 0;
      if(result){
        printres = 1;
      }
      if (printres == 1){
        return result;
      }
    }

    //free meme
    unsigned int x = sizeof(copied)/sizeof(char*);
    unsigned int in =0 ;
    while (in< x){
      kfree(copied[in]);
      in++;
    }
    kfree(copied);
    as_destroy(as_die);

  	/* Warp to user mode. */
  	enter_new_process(counter /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
  			  stackptr, entrypoint);

  	/* enter_new_process does not return. */
  	panic("enter_new_process returned\n");
  	return EINVAL;


}

#endif
