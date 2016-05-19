#include "../drivers/gxconsole/dev_cons.h"
#include <mmu.h>
#include <env.h>
#include <printf.h>
#include <pmap.h>
#include <sched.h>

extern char *KERNEL_SP;
extern struct Env *curenv;

/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int sysno, int c, int a2, int a3, int a4, int a5)
{
    printcharc((char) c);
    return ;
}

/* Overview:
 * 	This function enables you to copy content of `srcaddr` to `destaddr`.
 *
 * Pre-Condition:
 * 	`destaddr` and `srcaddr` can't be NULL. Also, the `srcaddr` area
 * 	shouldn't overlap the `destaddr`, otherwise the behavior of this
 * 	function is undefined.
 *
 * Post-Condition:
 * 	the content of `destaddr` area(from `destaddr` to `destaddr`+`len`) will
 * be same as that of `srcaddr` area.
 */
void *memcpy(void *destaddr, void const *srcaddr, u_int len)
{
    char *dest = destaddr;
    char const *src = srcaddr;

    while (len-- > 0)
    {
        *dest++ = *src++;
    }

    return destaddr;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void)
{
    return curenv->env_id;
}

/* Overview:
 *	This function enables the current process to give up CPU.
 *
 * Post-Condition:
 * 	Deschedule current environment. This function will never return.
 */
void sys_yield(void)
{
    bcopy((int)KERNEL_SP - sizeof(struct Trapframe), TIMESTACK - sizeof(struct Trapframe), sizeof(struct Trapframe));
    sched_yield();
}

/* Overview:
 * 	This function is used to destroy the current environment.
 *
 * Pre-Condition:
 * 	The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 when error occurs.
 */
int sys_env_destroy(int sysno, u_int envid)
{
    /*
    	printf("[%08x] exiting gracefully\n", curenv->env_id);
    	env_destroy(curenv);
    */
    int r;
    struct Env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
    {
        return r;
    }

    printf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
    env_destroy(e);
    return 0;
}

/* Overview:
 * 	Set envid's pagefault handler entry point and exception stack.
 *
 * Pre-Condition:
 * 	xstacktop points one byte past exception stack.
 *
 * Post-Condition:
 * 	The envid's pagefault handler will be set to `func` and its
 * 	exception stack will be set to `xstacktop`.
 * 	Returns 0 on success, < 0 on error.
 */
int sys_set_pgfault_handler(int sysno, u_int envid, u_int func, u_int xstacktop)
{
    // Your code here.
    struct Env *e;
    if ( envid2env(envid, &e, 1) < 0)
        panic("sys_set_pgfault_handler not implemented");
    //printf("set pgfault handler\n");
    e->env_pgfault_handler = func;
    xstacktop = TRUP(xstacktop);
    e->env_xstacktop = xstacktop;
    return 0;
    //	panic("sys_set_pgfault_handler not implemented");
}

/* Overview:
 * 	Allocate a page of memory and map it at 'va' with permission
 * 'perm' in the address space of 'envid'.
 *
 * 	If a page is already mapped at 'va', that page is unmapped as a
 * side-effect.
 *
 * Pre-Condition:
 * perm -- PTE_V is required,
 *         PTE_COW is not allowed(return -E_INVAL),
 *         other bits are optional.
 *
 * Post-Condition:
 * Return 0 on success, < 0 on error
 *	- va must be < UTOP
 *	- env may modify its own address space or the address space of its children
 */
int sys_mem_alloc(int sysno, u_int envid, u_int va, u_int perm)
{
    // Your code here.
    struct Env *env;
    struct Page *pp;

    perm = perm | PTE_V;
    // if (perm & PTE_COW)
    //   return -E_INVAL;

    if (va > UTOP)
        return -E_INVAL;

    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;

    if (page_alloc(&pp))
        return -E_NO_MEM;
    
    bzero((void *)page2kva(pp), BY2PG);

    if (page_insert(env->env_pgdir, pp, va, perm))
    {
        page_free(pp);
        return -E_NO_MEM;
    }

    return 0;

}

/* Overview:
 * 	Map the page of memory at 'srcva' in srcid's address space
 * at 'dstva' in dstid's address space with permission 'perm'.
 * Perm has the same restrictions as in sys_mem_alloc.
 * (Probably we should add a restriction that you can't go from
 * non-writable to writable?)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Note:
 * 	Cannot access pages above UTOP.
 */
int sys_mem_map(int sysno, u_int srcid, u_int srcva, u_int dstid, u_int dstva,
                u_int perm)
{
    struct Env *srcenv;
    struct Env *dstenv;
    struct Page *pp;
    Pte *ppte;

    srcva = ROUNDDOWN(srcva, BY2PG);
    dstva = ROUNDDOWN(dstva, BY2PG);

    perm = perm | PTE_V;

    if ((srcva > UTOP) || (dstva > UTOP))
    {
        return -E_INVAL;
    }
    if (envid2env(srcid, &srcenv, 1))
    {
        return -E_BAD_ENV;
    }
    if (envid2env(dstid, &dstenv, 1))
    {
        return -E_BAD_ENV;
    }
    if ((pp = page_lookup(srcenv->env_pgdir, srcva, &ppte)) != NULL)
    {
        if (!page_insert(dstenv->env_pgdir, pp, dstva, perm))
        {
            return 0;
        }
    }
    else
    {
        panic("sys_mem_map failed to map %d : 0x%x  to  %x : 0x%x",
              srcid, srcva, dstid, dstva);
    }

    return -E_INVAL;
}

/* Overview:
 * 	Unmap the page of memory at 'va' in the address space of 'envid'
 * (if no page is mapped, the function silently succeeds)
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Cannot unmap pages above UTOP.
 */
int sys_mem_unmap(int sysno, u_int envid, u_int va)
{
    // Your code here.

    struct Env *task;

    if (envid2env(envid, &task, 1) < 0)
        return -E_BAD_ENV;

    if ((unsigned int)va >= UTOP || va != ROUNDDOWN(va, BY2PG))
    {
        //panic("sys_mem_unmap 2 UTOP %x va %x %x",UTOP, va, ROUNDDOWN(va, BY2PG));
        return -E_INVAL;
    }

    page_remove(task->env_pgdir, va);

    return 0;

}

/* Overview:
 * 	Allocate a new environment.
 *
 * Pre-Condition:
 * The new child is left as env_alloc created it, except that
 * status is set to ENV_NOT_RUNNABLE and the register set is copied
 * from the current environment.
 *
 * Post-Condition:
 * 	In the child, the register set is tweaked so sys_env_alloc returns 0.
 * 	Returns envid of new environment, or < 0 on error.
 */
int sys_env_alloc(void)
{
    struct Env *env;

    if (env_alloc(&env, curenv->env_id) < 0)
    {
        return -E_NO_FREE_ENV;
    }

    env->env_status = ENV_NOT_RUNNABLE;
    env->env_parent_id = curenv->env_id;
    env->env_tf.pc = env->env_tf.cp0_epc;
    bcopy((int)KERNEL_SP - sizeof(struct Trapframe), &(env->env_tf), sizeof(struct Trapframe));
    env->env_tf.regs[2] = 0;

    return env->env_id;

}

/* Overview:
 * 	Set envid's env_status to status.
 *
 * Pre-Condition:
 * 	status should be one of `ENV_RUNNABLE`, `ENV_NOT_RUNNABLE` and
 * `ENV_FREE`. Otherwise return -E_INVAL.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if status is not a valid status for an environment.
 * 	The status of environment will be set to `status` on success.
 */
int sys_set_env_status(int sysno, u_int envid, u_int status)
{
    struct Env *env;

    if ((status != ENV_FREE) &&
            (status != ENV_RUNNABLE) &&
            (status != ENV_NOT_RUNNABLE))
        return -E_INVAL;

    if (envid2env(envid, &env, 1))
        return -E_BAD_ENV;

    env->env_status = status;
    return 0;

}

/* Overview:
 * 	Set envid's trap frame to tf.
 *
 * Pre-Condition:
 * 	`tf` should be valid.
 *
 * Post-Condition:
 * 	Returns 0 on success, < 0 on error.
 * 	Return -E_INVAL if the environment cannot be manipulated.
 *
 * Note: This hasn't be used now?
 */
int sys_set_trapframe(int sysno, u_int envid, struct Trapframe *tf)
{

    int r;
    struct Env *e;
    int ret = envid2env(envid, &e, 1);
    if (ret)
        return ret;
    e->env_tf = *tf;
    return 0;


}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Pre-Condition:
 * 	msg can't be NULL
 *
 * Post-Condition:
 * 	This function will make the whole system stop.
 */
void sys_panic(int sysno, char *msg)
{
    // no page_fault_mode -- we are trying to panic!
    panic("%s", TRUP(msg));
}

/* Overview:
 * 	This function enables caller to receive message from
 * other process. To be more specific, it will flag
 * the current process so that other process could send
 * message to it.
 *
 * Pre-Condition:
 * 	`dstva` is valid (Note: NULL is also a valid value for `dstva`).
 *
 * Post-Condition:
 * 	This syscall will set the current process's status to
 * ENV_NOT_RUNNABLE, giving up cpu.
 */
void sys_ipc_recv(int sysno, u_int dstva)
{

    if (curenv->env_ipc_recving)
        panic("already recving!!");
    if (dstva >= UTOP)
        panic("invalid dstva!!");

    curenv->env_ipc_recving = 1;
    curenv->env_ipc_dstva = dstva;
    curenv->env_status = ENV_NOT_RUNNABLE;

    sys_yield();

}

/* Overview:
 * 	Try to send 'value' to the target env 'envid'.
 *
 * 	The send fails with a return value of -E_IPC_NOT_RECV if the
 * target has not requested IPC with sys_ipc_recv.
 * 	Otherwise, the send succeeds, and the target's ipc fields are
 * updated as follows:
 *    env_ipc_recving is set to 0 to block future sends
 *    env_ipc_from is set to the sending envid
 *    env_ipc_value is set to the 'value' parameter
 * 	The target environment is marked runnable again.
 *
 * Post-Condition:
 * 	Return 0 on success, < 0 on error.
 *
 * Hint: the only function you need to call is envid2env.
 */
int sys_ipc_can_send(int sysno, u_int envid, u_int value, u_int srcva,
                     u_int perm)
{

    struct Env *e;
    struct Page *p;
    Pte *pte;
    int r, ret = 0;
    if (r = envid2env(envid, &e, 0) < 0)
        return r;
    if (!e->env_ipc_recving)
        return -E_IPC_NOT_RECV;

    if (srcva != 0 && e->env_ipc_dstva != 0)
    {

        if (srcva >= UTOP)
            return -E_INVAL;
        //still need perm validation
        p = page_lookup(curenv->env_pgdir, srcva, 0);
        if (p == 0)
        {
            printf("[%08x] page_lookup %08x failed in sys_ipc_can_send\n",
                   curenv->env_id, srcva);
            return -E_INVAL;
        }
        r = page_insert(e->env_pgdir, p, e->env_ipc_dstva, perm);
        if (r < 0)
            return r;
        e->env_ipc_perm = perm;
    }
    else
    {
        e->env_ipc_perm = 0;
    }


    e->env_ipc_recving = 0;
    e->env_ipc_from = curenv->env_id;
    e->env_ipc_value = value;
    e->env_status = ENV_RUNNABLE;

    return 0;
}

