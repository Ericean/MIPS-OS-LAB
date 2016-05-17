// implement fork from user space

#include "lib.h"
#include <mmu.h>
#include <env.h>



/* ----------------- help functions ---------------- */

/* Overview:
 * 	Copy `len` bytes from `src` to `dst`.
 *
 * Pre-Condition:
 * 	`src` and `dst` can't be NULL. Also, the `src` area 
 * 	 shouldn't overlap the `dest`, otherwise the behavior of this 
 * 	 function is undefined.
 */
void user_bcopy(const void *src, void *dst, size_t len)
{
	void *max;

	//	writef("~~~~~~~~~~~~~~~~ src:%x dst:%x len:%x\n",(int)src,(int)dst,len);
	max = dst + len;

	// copy machine words while possible
	if (((int)src % 4 == 0) && ((int)dst % 4 == 0)) {
		while (dst + 3 < max) {
			*(int *)dst = *(int *)src;
			dst += 4;
			src += 4;
		}
	}

	// finish remaining 0-3 bytes
	while (dst < max) {
		*(char *)dst = *(char *)src;
		dst += 1;
		src += 1;
	}

	//for(;;);
}

/* Overview:
 * 	Sets the first n bytes of the block of memory 
 * pointed by `v` to zero.
 * 
 * Pre-Condition:
 * 	`v` must be valid.
 *
 * Post-Condition:
 * 	the content of the space(from `v` to `v`+ n) 
 * will be set to zero.
 */
void user_bzero(void *v, u_int n)
{
	char *p;
	int m;

	p = v;
	m = n;

	while (--m >= 0) {
		*p++ = 0;
	}
}
/*--------------------------------------------------------------*/

/* Overview:
 * 	Custom page fault handler - if faulting page is copy-on-write,
 * map in our own private writable copy.
 * 
 * Pre-Condition:
 * 	`va` is the address which leads to a TLBS exception.
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
	u_char *tmp = (u_char*)(UTEXT-BY2PG);	// should be available!;
  
  	Pte pte = vpd[PDX(va)];
  
  // va = va & 0xfffff000; // Get the base address of the page
  // if (debug) syscall_putchar("pgfault");
  // if (!(err & FEC_WR)) {
  //   syscall_putchar("non-writing page fault\n");
  //   syscall_env_destroy(0);
  // }
  
  if (pte & PTE_V) {
    pte = vpt[VPN(va)];
    if ((pte & PTE_V) && (pte & PTE_COW)) 
    {
      if ((syscall_mem_alloc(0, (u_int)tmp, PTE_V|PTE_R)) < 0) 
      {
			syscall_putchar("Failed to allocate memory for copy\n");
			syscall_env_destroy(0);
      }
      user_bcopy((u_char *)va, tmp, BY2PG);
      if (syscall_mem_map(0, (u_int)tmp, 0, va, (((pte & ~PTE_COW) | PTE_R)) < 0) )
		  syscall_env_destroy(0);
    }
    
    if (syscall_mem_unmap(0, (u_int)tmp) < 0)
		syscall_putchar("Failed to unmap in pgfault");
   	else 
   	{
    	syscall_putchar("Page fault unexpected\n");
   		syscall_env_destroy(0);
  	}
}

	//	writef("fork.c:pgfault():\t va:%x\n",va);
    
    //map the new page at a temporary place

	//copy the content
	
    //map the page on the appropriate place
	
    //unmap the temporary place
	
}

/* Overview:
 * 	Map our virtual page `pn` (address pn*BY2PG) into the target `envid`
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
 * 	PTE_LIBRARY indicates that the page is shared between processes.
 * A page with PTE_LIBRARY may have PTE_R at the same time. You
 * should process it correctly.
 */
static void
duppage(u_int envid, u_int pn)
{

	int r;
	u_int addr =pn*BY2PG;
	Pte pte;
	u_int perm;

	pte= vpt[VPN(addr)];
	if(pte & PTE_V){
		if(pte&PTE_LIBRARY){
			// Library pages are always copied no-matter what
      		// pte = pte & PTE_USER;      
      		if ((r = syscall_mem_map(0, addr, envid, addr, pte)))
			user_panic("Failing to map 0x%x ro : %e\n", addr, r);
		}
		else{

			 // Otherwise, copy-on-write writeable pages or COW pages
       if ((pte & PTE_R) || (pte & PTE_COW)) {
			 pte = (((pte & ~PTE_R) | PTE_COW));
	   		if ((r = syscall_mem_map(0, addr, envid, addr, pte)))
	       		user_panic("Failing to map 0x%x rw : %e\n", addr, r);
	   		if ((r = syscall_mem_map(0, addr, 0, addr, pte)))
	        	user_panic("Failing to remap 0x%x rw : %e\n", addr, r);
     	 } 
       else {
	       // Else just copy it
	       //pte = pte & (PTE_USER);
	      if ((r = syscall_mem_map(0, addr, envid, addr, pte)))
	        user_panic("Failing to map 0x%x ro : %e\n", addr, r);
         }
		}
	}





	/* Note:
	 *  I am afraid I have some bad news for you. There is a ridiculous, 
	 * annoying and awful bug here. I could find another more adjectives 
	 * to qualify it, but you have to reproduce it to understand 
	 * how disturbing it is.
	 * 	To reproduce this bug, you should follow the steps bellow:
	 * 	1. uncomment the statement "writef("");" bellow.
	 * 	2. make clean && make
	 * 	3. lauch Gxemul and check the result.
	 * 	4. you can add serveral `writef("");` and repeat step2~3.
	 * 	Then, you will find that additional `writef("");` may lead to
	 * a kernel panic. Interestingly, some students, who faced a strange 
	 * kernel panic problem, found that adding a `writef("");` could solve
	 * the problem. 
	 *  Unfortunately, we cannot find the code which leads to this bug,
	 * although we have debugged it for serveral weeks. If you face this
	 * bug, we would like to say "Good luck. God bless."
	 */
	// writef("");

}

/* Overview:
 * 	User-level fork. Create a child and then copy our address space
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
	//int pn;
	Pte pte;
	u_int addr, addr2;


	extern struct Env *envs;
	extern struct Env *env;
	u_int i;

	//install a pgfault handler
	set_pgfault_handler(pgfault);
	// alloc a child env
	envid=syscall_env_alloc();
	if(envid<0) 
		return envid;//error

	if(envid==0){
		env= &envs[ENVX(syscall_getenvid())];
		return 0;
	}
	else{
			 addr = 0;
   			 while (addr < (UTOP-PDMAP)) 
   			 { // don't do this for the exception stack
    			  	pte = vpd[PDX(addr)];
      				if (pte & PTE_V) 
      				{
      					for (addr2 = addr; addr2 < addr+PDMAP; addr2 += BY2PG) 
	  					duppage(envid, addr2/BY2PG);
					}
    			   addr += PDMAP;
    		}
   			 // Get all final pages up to the USTACKTOP, but not including UXSTACKTOP
    		for (addr2 = addr; addr2 < addr+(PDMAP-BY2PG); addr2 += BY2PG) 
    			 duppage(envid, addr2/BY2PG);
   				 
		   	if (syscall_mem_alloc(envid, UXSTACKTOP-BY2PG, PTE_V|PTE_R)) {
		      user_panic("Couldn't map a page for the child exception stack\n");
		      return -E_NO_MEM;
		    }
		    if (syscall_set_pgfault_handler(envid, (u_int)__asm_pgfault_handler, UXSTACKTOP)) {
		      user_panic("Failed to set child's pgfault handler\n");
		      return -E_UNSPECIFIED;
		    }
		    if (syscall_set_env_status(envid, ENV_RUNNABLE)) {
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
