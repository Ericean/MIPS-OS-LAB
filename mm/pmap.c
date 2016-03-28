#include "mmu.h"

#include "pmap.h"

#include "printf.h"

#include "env.h"

#include "error.h"

#include "types.h"





/* These variables are set by mips_detect_memory() */

u_long maxpa;            /* Maximum physical address */

u_long npage;            /* Amount of memory(in pages) */

u_long basemem;          /* Amount of base memory(in bytes) */

u_long extmem;           /* Amount of extended memory(in bytes) */



Pde* boot_pgdir;



struct Page *pages;

static u_long freemem;



static struct Page_list page_free_list;	/* Free list of physical pages */



void mips_detect_memory()

{



	basemem = 64 * 1024 * 1024;

	extmem = 0;

	maxpa = basemem;


	npage = maxpa / BY2PG;

	//printf("npage :%x   %d\n",npage,npage);

	printf("Physical memory: %dK available, ", (int)(maxpa / 1024));

	printf("base = %dK, extended = %dK\n", (int)(basemem / 1024), (int)(extmem / 1024));

}


// return virtual address
static void * alloc(u_int n, u_int align, int clear)

{

	//end is defined in mips_vm_init()

	extern char end[];

	u_long alloced_mem;

	// initialize freemem if this is the first time
	if (freemem == 0) freemem = (u_long)end;

	freemem = ROUND(freemem, align);

	if ( (freemem + n < freemem) || (freemem + n > KERNBASE + maxpa)) {

		panic("out of memory during vm_init");
	}

	alloced_mem = (void *)freemem;

	freemem = freemem + n;


	if (clear)  bzero(alloced_mem, n);

	return (void *)alloced_mem;


}





//

// Given pgdir, the current page directory base,

// walk the 2-level page table structure to find

// the Pte for virtual address va.  Return a pointer to it.

//

// If there is no such directory page:

//	- if create == 0, return 0.

//	- otherwise allocate a new directory page and install it.

//

// This function is abstracting away the 2-level nature of

// the page directory for us by allocating new page tables

// as needed.

//

// Boot_pgdir_walk cannot fail.  It's too early to fail.

//



//return a virtual page table entry address
static Pte* boot_pgdir_walk(Pde *pgdir, u_long va, int create)

{
	Pde *pde;

	Pte *ptable;

	pde = (Pde *)(&pgdir[PDX(va)]);

	//a little doubt
	if (*pde & PTE_V)
		ptable = (Pte *)KADDR(PTE_ADDR(*pde));
	else
	{

		if (!create)return 0;

		ptable = (Pte*)alloc(BY2PG, BY2PG, 1);
		*pde = PADDR(ptable) | PTE_R | PTE_V;


	}

	return (Pte *)(&ptable[PTX(va)]);


}

//

// Map [va, va+size) of virtual address space to physical [pa, pa+size)

// in the page table rooted at pgdir.  Size is a multiple of BY2PG.

// Use permission bits perm|PTE_V for the entries.

//

void boot_map_segment(Pde *pgdir, u_long va, u_long size, u_long pa, int perm)

{


	int i;

	Pte * pageTable;

	for (i = 0; i < size; i += BY2PG)
	{

		*boot_pgdir_walk(pgdir, va + i, 1) = ((pa + i) | perm | PTE_V);

	}

}



// Set up a two-level page table:

//

// This function only sets up the kernel part of the address space

// (ie. addresses >= UTOP).  The user part of the address space

// will be setup later.

//

void mips_vm_init()

{

	extern char KVPT[];

	extern char end[];

	extern int mCONTEXT;

	extern struct Env *envs;

	struct Env *en;



	Pde* pgdir;

	u_int n;



	//page 0
	pgdir = (Pde *)alloc(BY2PG, BY2PG, 1);

	printf("to memory %x for struct page directory.\n", freemem);

	mCONTEXT = (int)(pgdir);

	boot_pgdir = pgdir;

	pages = (struct Page*)alloc(sizeof(struct Page) * npage, BY2PG, 1);

	//printf("pages:...%x\n",pages);

	printf("to memory %x for struct Pages.\n", freemem);





	//create pgdir entry for all kernel virtual memory from physical 0

	boot_pgdir_walk(pgdir, ULIM, 1);

	boot_map_segment(pgdir, ULIM, PDMAP, 0, 0);

	//printf("%x\n", va2pa(boot_pgdir,ULIM+BY2PG));

	//before this print information cannot be printed

	printf("mips_vm_init:boot_pgdir is %x\n", boot_pgdir);

	printf("pmap.c:\t mips vm init success\n");

	envs = (struct Env*)alloc(NENV * sizeof(struct Env), BY2PG, 1);

	boot_map_segment(pgdir, UENVS, NENV * sizeof(struct Env), PADDR(envs), PTE_R);

}











//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------

// Tracking of physical pages.

// -

//static void page_initpp(struct Page *pp);

void

page_init(void)

{
	int i;
	int use;

	LIST_INIT (&page_free_list);

	//align free memory to page boundry
	alloc(0, BY2PG, 0);
	
	//not sure if can flush the allocated pages
	for (i = 0; i < npage; i++) {

		use = 1;
		//Bottom basemem bytes are free except page 0.
		//not to touch kernel code
		if (i != 0 && i < basemem / BY2PG)
			use = 0;

		//The memory over the kernel is free.
		// if (i >= (freemem - KERNBASE) / BY2PG)
		// 	use = 0;

		//use pages to manage memory
		pages[i].pp_ref = use;
		if (!use)
			LIST_INSERT_HEAD(&page_free_list, &pages[i], pp_link);

	}
	// printf("%x\n",page2kva(&pages[0]));


}



//

// Initialize a Page structure.

//

static void

page_initpp(struct Page *pp)

{

	bzero(pp, sizeof(*pp));

}





//

// Allocates a physical page.

//

// *pp -- is set to point to the Page struct of the newly allocated

// page

//

// RETURNS

//   0 -- on success

//   -E_NO_MEM -- otherwise

//

// Hint: use LIST_FIRST, LIST_REMOVE, page_initpp()

// Hint: pp_ref should not be incremented

int

page_alloc(struct Page **pp)

{


	if ((*pp = LIST_FIRST(&page_free_list)) == NULL)
		return -E_NO_MEM;

	LIST_REMOVE(*pp, pp_link);
	//init page
	page_initpp(*pp);
	bzero((Pte *)page2kva(*pp), BY2PG);

	return 0;


}





//

// Return a page to the free list.

// (This function should only be called when pp->pp_ref reaches 0.)

//

void

page_free(struct Page *pp)

{
	//make sure free operation is valid
	assert(pp->pp_ref == 0);
	LIST_INSERT_HEAD(&page_free_list, pp, pp_link);

}







void

page_decref(struct Page *pp)

{

	if ((pp->pp_ref == 0) || (--pp->pp_ref == 0))
		page_free(pp);

}





//

// This is boot_pgdir_walk with a different allocate function.

// Unlike boot_pgdir_walk, pgdir_walk can fail, so we have to

// return pte via a pointer parameter.

//

// Stores address of page table entry in *pte.

// Stores 0 if there is no such entry or on error.

//

// RETURNS:

//   0 on success

//   -E_NO_MEM, if page table couldn't be allocated

//



int

pgdir_walk(Pde *pgdir, u_long va, int create, Pte **ppte)

{


	Pde *pde;

	Pte *pageTable;

	struct Page *pageTablePage;
	*ppte = 0;

	pde = (Pde *)(&pgdir[PDX(va)]);

	if (*pde & PTE_V)
		pageTable = (Pte *)KADDR(PTE_ADDR(*pde));
	else {
		if (!create) return 0;
		if ((page_alloc(&pageTablePage)) < 0) {

			return  -E_NO_MEM;
			//printf("page allocate succeed in pgdir_walk");
			//panic("page_alloc failure when doing pgdir_walk\n");
		}
		pageTablePage->pp_ref++;
		pageTable = (Pte*)page2kva(pageTablePage);
		//update pde
		*pde = page2pa(pageTablePage) | PTE_R | PTE_V;

	}
	//return pte
	*ppte = &pageTable[PTX(va)];
	//printf("pgdir_walk succeed.allocated page is %x\n",pageTable);

	return 0;


}







void

tlb_invalidate(Pde *pgdir, u_long va)

{

	if (curenv)

		tlb_out(PTE_ADDR(va) | GET_ENV_ASID(curenv->env_id));

	else

		tlb_out(PTE_ADDR(va));



}





//

// Map the physical page 'pp' at virtual address 'va'.

// The permissions (the low 12 bits) of the page table

//  entry should be set to 'perm|PTE_P'.

//





int

page_insert(Pde *pgdir, struct Page *pp, u_long va, u_int perm)

{	// Fill this function in

	u_int PERM;

	Pte *pgtable_entry;

	PERM = perm | PTE_V;



	pgdir_walk(pgdir, va, 0, &pgtable_entry);



	if (pgtable_entry != 0 && (*pgtable_entry & PTE_V) != 0)

		if (pa2page(*pgtable_entry) != pp) page_remove(pgdir, va);

		else

		{

			tlb_invalidate(pgdir, va);

			*pgtable_entry = (page2pa(pp) | PERM);

			return 0;

		}

	tlb_invalidate(pgdir, va);

	if (pgdir_walk(pgdir, va, 1, &pgtable_entry) != 0)

		return -E_NO_MEM;// panic("page insert wrong.\n");

	*pgtable_entry = (page2pa(pp) | PERM);

	pp->pp_ref++;

	return 0;

}







//return the Page which va map to.

struct Page*

page_lookup(Pde *pgdir, u_long va, Pte **ppte)

{

	struct Page *ppage;

	Pte *pte;



	pgdir_walk(pgdir, va, 0, &pte);

	if (pte == 0) return 0;

	if ((*pte & PTE_V) == 0) return 0;    //the page is not in memory.

	ppage = pa2page(*pte);

	if (ppte) *ppte = pte;

	return ppage;

}





//

// Unmaps the physical page at virtual address 'va'.

//

// Details:

//   - The ref count on the physical page should decrement.

//   - The physical page should be freed if the refcount reaches 0.

//   - The pg table entry corresponding to 'va' should be set to 0.

//     (if such a PTE exists)

//   - The TLB must be invalidated if you remove an entry from

//	   the pg dir/pg table.

//

// Hint: The TA solution is implemented using

//  pgdir_walk(), page_free(), tlb_invalidate()

//



void

page_remove(Pde *pgdir, u_long va)

{

	// Fill this function in

	//v?????????????????

	Pte * ppte;

	u_long pa;



	pa = va2pa(pgdir, va);

	//PTX is error,should use PPN(pa),PTX just have 12 bit valid number

	//#define PTX(va)		((((u_long)(va))>>12) & 0x03FF)

	//#define PPN(va)		(((u_long)(va))>>12)

	//page_decref(&pages[PTX(pa)]);

	page_decref(pa2page(pa));

	pgdir_walk(pgdir, va, 0, &ppte);

	*ppte = 0;

	tlb_invalidate(pgdir, va);


	return;

}







void

page_check(void)

{

	struct Page *pp, *pp0, *pp1, *pp2;

	struct Page_list fl;



	// should be able to allocate three pages

	pp0 = pp1 = pp2 = 0;

	assert(page_alloc(&pp0) == 0);

	assert(page_alloc(&pp1) == 0);

	assert(page_alloc(&pp2) == 0);



	assert(pp0);

	assert(pp1 && pp1 != pp0);

	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	//printf("bootdir: %x\n",boot_pgdir);
	//printf("va2pa(boot_pgdir, 0x0): %x \n",va2pa(boot_pgdir, 0x0));

	// temporarily steal the rest of the free pages

	fl = page_free_list;

	//after this this page_free list must be empty!!!!

	LIST_INIT(&page_free_list);





	// should be no free memory

	assert(page_alloc(&pp) == -E_NO_MEM);

	printf("start page_insert\n");

	// there is no free memory, so we can't allocate a page table

	assert(page_insert(boot_pgdir, pp1, 0x0, 0) < 0);



	// free pp0 and try again: pp0 should be used for page table

	page_free(pp0);

	//va address 0x0 pageout error "too low"

	assert(page_insert(boot_pgdir, pp1, 0x0, 0) == 0);

	//assert(page_insert(boot_pgdir, pp1, 0x10000, 0) == 0);

	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));

	printf("va2pa(boot_pgdir, 0x0) is %x\n", va2pa(boot_pgdir, 0x0));

	printf("page2pa(pp1) is %x\n", page2pa(pp1));

	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));

	assert(pp1->pp_ref == 1);



	// should be able to map pp2 at BY2PG because pp0 is already allocated for page table

	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);

	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));

	assert(pp2->pp_ref == 1);

	// should be no free memory

	assert(page_alloc(&pp) == -E_NO_MEM);

//printf("why\n");

	// should be able to map pp2 at BY2PG because it's already there

	assert(page_insert(boot_pgdir, pp2, BY2PG, 0) == 0);

	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp2));

	assert(pp2->pp_ref == 1);

//printf("It is so unbelivable\n");

	// pp2 should NOT be on the free list

	// could happen in ref counts are handled sloppily in page_insert

	assert(page_alloc(&pp) == -E_NO_MEM);



	// should not be able to map at PDMAP because need free page for page table

	assert(page_insert(boot_pgdir, pp0, PDMAP, 0) < 0);



	// insert pp1 at BY2PG (replacing pp2)

	assert(page_insert(boot_pgdir, pp1, BY2PG, 0) == 0);



	// should have pp1 at both 0 and BY2PG, pp2 nowhere, ...

	assert(va2pa(boot_pgdir, 0x0) == page2pa(pp1));

	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));

	// ... and ref counts should reflect this

	assert(pp1->pp_ref == 2);

	printf("pp2->pp_ref %d\n", pp2->pp_ref);

	assert(pp2->pp_ref == 0);

	printf("end page_insert\n");



	// pp2 should be returned by page_alloc

	assert(page_alloc(&pp) == 0 && pp == pp2);



	// unmapping pp1 at 0 should keep pp1 at BY2PG

	page_remove(boot_pgdir, 0x0);

	assert(va2pa(boot_pgdir, 0x0) == ~0);

	assert(va2pa(boot_pgdir, BY2PG) == page2pa(pp1));

	assert(pp1->pp_ref == 1);

	assert(pp2->pp_ref == 0);



	// unmapping pp1 at BY2PG should free it

	page_remove(boot_pgdir, BY2PG);

	assert(va2pa(boot_pgdir, 0x0) == ~0);

	assert(va2pa(boot_pgdir, BY2PG) == ~0);

	assert(pp1->pp_ref == 0);

	assert(pp2->pp_ref == 0);



	// so it should be returned by page_alloc

	assert(page_alloc(&pp) == 0 && pp == pp1);



	// should be no free memory

	assert(page_alloc(&pp) == -E_NO_MEM);



	// forcibly take pp0 back

	assert(PTE_ADDR(boot_pgdir[0]) == page2pa(pp0));

	boot_pgdir[0] = 0;

	assert(pp0->pp_ref == 1);

	pp0->pp_ref = 0;



	// give free list back

	page_free_list = fl;



	// free the pages we took

	page_free(pp0);

	page_free(pp1);

	page_free(pp2);



	printf("page_check() succeeded!\n");

}







void pageout(int va, int context)

{

	u_long r;

	struct Page *p = NULL;



	if (context < 0x80000000)

		panic("tlb refill and alloc error!");

	if ((va > 0x7f400000) && (va < 0x7f800000))

		panic(">>>>>>>>>>>>>>>>>>>>>>it's env's zone");



	if (va < 0x10000)

		panic("^^^^^^TOO LOW^^^^^^^^^");

	if ((r = page_alloc(&p)) < 0)

		panic ("page alloc error!");

	p->pp_ref++;



	page_insert((int*)context, p, VA2PFN(va), PTE_R);

	printf("pageout:\t@@@___0x%x___@@@  ins a page \n", va);

}


