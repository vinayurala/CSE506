// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
	int perm = PTE_P | PTE_W | PTE_U;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).
	if(!(err & FEC_WR))
		panic("\nNo write access at addr = 0x%x, rip = %x!\n", addr, utf->utf_rip);
	if(!(vpt[VPN(addr)] & PTE_COW))
		panic("\nCOW bit not set at addr: 0x%0x!\n", addr);

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.
	r = sys_page_alloc(0, (void *)PFTEMP, perm);
	if(r)
		panic("\nPanic at sys_page_alloc: %e\n", r);
	
	memmove(PFTEMP, (void *)(ROUNDDOWN(addr, PGSIZE)), PGSIZE);
	
	r = sys_page_map(0, (void *)PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), perm);
	if(r)
		panic("\nPanic at sys_page_map: %e\n", r);
	
	r = sys_page_unmap(0, PFTEMP);
	if(r)
		panic("Panic at sys_page_unmap for PFTEMP: %e\n", r);

	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.

	 uintptr_t addr;  
	 pte_t pte = vpt[pn];
	 int cow_perm = PTE_P | PTE_U | PTE_COW; 

	 addr = (uintptr_t)(pn << PGSHIFT);
	 if(pte & PTE_SHARE)
	 {
		 r = sys_page_map(0, (void *)addr, envid, (void *)addr, PGOFF(pte));
		 if(r)
			 panic("\nsys_page_map error: %e\n", r);
		 return 0;
	 } 
	 if(!((pte & PTE_W) || (pte & PTE_COW))) 
	 { 
		 r = sys_page_map(0, (void *)addr, envid, (void *)addr, PGOFF(pte)); 
		 if(r) 
	 		panic("\nsys_page_map error: %e\n", r); 
	 	return 0;
	 }  
	 r = sys_page_map(0, (void *)addr, envid, (void *)addr, cow_perm); 
	 if(r)
	 	panic("\nsys_page_map error for parent: %e\n", r);

	 r = sys_page_map(0, (void *)addr, 0, (void *)addr, cow_perm);
	 if(r) 
		 panic("\nsys_page_map error for child: %e\n", r);

	//panic("duppage not implemented");


	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	int r;
	uint64_t i;

	set_pgfault_handler(pgfault);
	envid = sys_exofork();
	if(envid < 0)
	{
		panic("\nsys_exofork error: %e\n", envid);
		return -1;
	}

        else if(envid == 0)
	{
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	int pn, end_pn;

	for(pn = PPN(UTEXT); pn < PPN(UTOP); ) 
	 { 
		 if(!((vpml4e[VPML4E(pn)] & PTE_P)&&(vpde[pn >> 18] & PTE_P) && (vpd[pn >> 9] & PTE_P))) 
	 	{ 
	 		pn += NPTENTRIES; 
			continue; 
	 	} 
		for(end_pn = pn + NPTENTRIES; pn < end_pn ; pn++)
	 	{ 
			if((vpt[pn] & PTE_P) != PTE_P) 
	 			continue; 

	 		if(pn == PPN(UXSTACKTOP - 1)) 
	 			continue; 
	 		   duppage(envid, pn); 
	 	} 
	 } 

	r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P); 
	if(r)
	{
	 	panic("\nCould not allocate a page for user exception stack in fork: %e\n", r);
	 	return r; 
	 } 
	  if ((r = sys_page_alloc(envid, (void*)(USTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)  
	  	panic("sys_page_alloc: %e", r); 

	 if ((r = sys_page_map(envid, (void*)(USTACKTOP - PGSIZE), 0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0) 
	 	panic("sys_page_map: %e", r); 

	 memmove(UTEMP, (void*)(USTACKTOP-PGSIZE), PGSIZE);

	 if ((r = sys_page_unmap(0, UTEMP)) < 0) 
		panic("sys_page_unmap: %e", r); 

        r = sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall); 
	if(r)
	{ 
		panic("\nCould not register page fault handler in fork: %e\n",r); 
	  	return r;  
        } 

	if((r = sys_env_set_status(envid, ENV_RUNNABLE)))
        { 
	 	panic("\nCould not set child's status to ENV_RUNNABLE in fork: %e\n",r); 
	 	return r;
	 }
	//panic("fork not implemented");

	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
