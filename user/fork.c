// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>



/* ----------------- help functions ---------------- */

/* Overview:
 *  Copy `len` bytes from `src` to `dst`.
 *
 * Pre-Condition:
 *  `src` and `dst` can't be NULL. Also, the `src` area
 *   shouldn't overlap the `dest`, otherwise the behavior of this
 *   function is undefined.
 */
void user_bcopy(const void *src, void *dst, size_t len)
{
    void *max;

    //  writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
    max = dst + len;

    // copy machine words while possible
    if (((int)src % 4 == 0) && ((int)dst % 4 == 0))
    {
        while (dst + 3 < max)
        {
            *(int *)dst = *(int *)src;
            dst += 4;
            src += 4;
        }
    }

    // finish remaining 0-3 bytes
    while (dst < max)
    {
        *(char *)dst = *(char *)src;
        dst += 1;
        src += 1;
    }

    //for(;;);
}

/* Overview:
 *  Sets the first n bytes of the block of memory
 * pointed by `v` to zero.
 *
 * Pre-Condition:
 *  `v` must be valid.
 *
 * Post-Condition:
 *  the content of the space(from `v` to `v`+ n)
 * will be set to zero.
 */
void user_bzero(void *v, u_int n)
{
    char *p;
    int m;

    p = v;
    m = n;

    while (--m >= 0)
    {
        *p++ = 0;
    }
}
/*--------------------------------------------------------------*/

/* Overview:
 *  Custom page fault handler - if faulting page is copy-on-write,
 * map in our own private writable copy.
 *
 * Pre-Condition:
 *  `va` is the address which leads to a TLBS exception.
 *
 * Post-Condition:
 *  Launch a user_panic if `va` is not a copy-on-write page.
 * Otherwise, this handler should map a private writable copy of
 * the faulting page at correct address.
 */
static void
pgfault(u_int va)
{
    int r;
    int i;
    va = ROUNDDOWN(va, BY2PG);
    u_char *PFTEMP = (u_char*)(UTEXT - BY2PG);

    if (!((*vpt)[VPN(va)] & PTE_COW ))
        user_panic("PTE_COW failed!");

    if (syscall_mem_alloc(0, PFTEMP, PTE_V | PTE_R) < 0)
        user_panic("syscall_mem_alloc failed!");

     user_bcopy((void*)va, PFTEMP, BY2PG);

    if (syscall_mem_map(0, PFTEMP, 0, va, PTE_V | PTE_R) < 0)
        user_panic("syscall_mem_map failed!");

    if (syscall_mem_unmap(0, PFTEMP) < 0)
        user_panic("syscall_mem_unmap failed!");
}

/* Overview:
 *  Map our virtual page `pn` (address pn*BY2PG) into the target `envid`
 * at the same virtual address.
 *
 * Post-Condition:
 *  if the page is writable or copy-on-write, the new mapping must be
 * created copy on write and then our mapping must be marked
 * copy on write as well. In another word, both of the new mapping and
 * our mapping should be copy-on-write if the page is writable or
 * copy-on-write.
 *
 * Hint:
 *  PTE_LIBRARY indicates that the page is shared between processes.
 * A page with PTE_LIBRARY may have PTE_R at the same time. You
 * should process it correctly.
 */
static void
    duppage(u_int envid, u_int pn)
    {
        int r;
        u_int addr;
        Pte pte;
        u_int perm;

        perm = ((*vpt)[pn]) & 0xfff;

        if ( (perm & PTE_R) != 0 || (perm & PTE_COW) != 0)
        {
            if (perm & PTE_LIBRARY) {
                perm = perm | PTE_V | PTE_R;
            }
            else {
                perm = perm | PTE_V | PTE_R | PTE_COW;
            }

            if (syscall_mem_map(0, pn * BY2PG, envid, pn * BY2PG, perm) == -1)
                user_panic("duppage failed at 1");

            if (syscall_mem_map(0, pn * BY2PG, 0, pn * BY2PG, perm) == -1)
                user_panic("duppage failed at 2");
        }
        else {
            if (syscall_mem_map(0, pn * BY2PG, envid, pn * BY2PG, perm) == -1)
                user_panic("duppage failed at 3");
        }
    }




/* Note:
 *  I am afraid I have some bad news for you. There is a ridiculous,
 * annoying and awful bug here. I could find another more adjectives
 * to qualify it, but you have to reproduce it to understand
 * how disturbing it is.
 *  To reproduce this bug, you should follow the steps bellow:
 *  1. uncomment the statement "writef("");" bellow.
 *  2. make clean && make
 *  3. lauch Gxemul and check the result.
 *  4. you can add serveral `writef("");` and repeat step2~3.
 *  Then, you will find that additional `writef("");` may lead to
 * a kernel panic. Interestingly, some students, who faced a strange
 * kernel panic problem, found that adding a `writef("");` could solve
 * the problem.
 *  Unfortunately, we cannot find the code which leads to this bug,
 * although we have debugged it for serveral weeks. If you face this
 * bug, we would like to say "Good luck. God bless."
 */
// writef("");



/* Overview:
 *  User-level fork. Create a child and then copy our address space
 * and page fault handler setup to the child.
 *
 * Hint: use vpd, vpt, and duppage.
 * Hint: remember to fix "env" in the child process!
 * Note: `set_pgfault_handler`(user/pgfault.c) is different from
 *       `syscall_set_pgfault_handler`.
 */
extern void __asm_pgfault_handler(void);
int
fork(void)
{
    u_int envid;
    int pn;
    Pte pte;
    u_int addr, addr2;


    extern struct Env *envs;
    extern struct Env *env;
    u_int i;

    //writef("entering fork...\n");
    //install a pgfault handler
    set_pgfault_handler(pgfault);
    // alloc a child env
    envid = syscall_env_alloc();

    if (envid < 0)
        return -1;//error

    if (envid == 0)
    {
        env = &envs[ENVX(syscall_getenvid())];
        return 0;
    }
    else
    {

        for(pn = 0; pn < ( UTOP / BY2PG) - 1 ; pn ++){
              if(((*vpd)[pn/PTE2PT]) != 0 && ((*vpt)[pn]) != 0){
                      duppage(envid, pn);
                }
        }


        if (syscall_mem_alloc(envid, UXSTACKTOP - BY2PG, PTE_V | PTE_R))
        {
            user_panic("Couldn't map a page for the child exception stack\n");
            return -E_NO_MEM;
        }
        if (syscall_set_pgfault_handler(envid, (u_int)__asm_pgfault_handler, UXSTACKTOP))
        {
            user_panic("Failed to set child's pgfault handler\n");
            return -E_UNSPECIFIED;
        }
        if (syscall_set_env_status(envid, ENV_RUNNABLE))
        {
            user_panic("Failed to mark child as runnable\n");
            return -E_UNSPECIFIED;
        }
        return envid;
    }


    //The parent installs pgfault using set_pgfault_handler

    //alloc a new alloc
}

// Challenge!
int
sfork(void)
{
    user_panic("sfork not implemented");
    return -E_INVAL;
}
