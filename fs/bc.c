#include "fs.h"
#include <inc/disk_crypt.h>

int s_encrypted;

// Return the virtual address of this disk block.
void*
diskaddr(uint64_t blockno)
{
	if (blockno == 0 || (super && blockno >= super->s_nblocks))
		panic("bad block number %08x in diskaddr", blockno);
	return (char*) (DISKMAP + blockno * BLKSIZE);
}

// Is this virtual address mapped?
bool
va_is_mapped(void *va)
{
	return (vpml4e[VPML4E(va)] & PTE_P) && (vpde[VPDPE(va)] & PTE_P) && (vpd[VPD(va)] & PTE_P) && (vpt[PPN(va)] & PTE_P);
}

// Is this virtual address dirty?
bool
va_is_dirty(void *va)
{
	return (vpt[PPN(va)] & PTE_D) != 0;
}

// Fault any disk block that is read or written in to memory by
// loading it from disk.
// Hint: Use ide_read and BLKSECTS.
static void
bc_pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;
	int r;

	// Check that the fault was within the block cache region
	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("page fault in FS: eip %08x, va %08x, err %04x",
		      utf->utf_rip, addr, utf->utf_err);

	// Sanity check the block number.
	if (super && blockno >= super->s_nblocks)
		panic("reading non-existent block %08x\n", blockno);

	// Allocate a page in the disk map region, read the contents
	// of the block from the disk into that page, and mark the
	// page not-dirty (since reading the data from disk will mark
	// the page dirty).
	//
	// LAB 5: Your code here

	void *dst_addr = (void *) ROUNDDOWN(addr, BLKSIZE);
	r = sys_page_alloc(thisenv->env_id, dst_addr, PTE_U | PTE_P | PTE_W);

	// Project addition --> Transparent disk decryption (called only if disk block
	// is encrypted). Bitmap block has been left out of encryption, bitmap block data is tightly
	// bound in other routines, so can't encrypt.
	 if(blockno == 2)
	     ide_read(blockno * BLKSECTS, dst_addr, BLKSECTS);
	else if (blockno == 1) /* || (blockno == 2)) */
	 {
		 if(!s_encrypted)
			 ide_read(blockno * BLKSECTS, dst_addr, BLKSECTS);
		 else
		 {
			 r = transparent_disk_decrypt(blockno, dst_addr);
			 if(r)
				 return;

		 }

	 }
	 else 
	 { 
		 r = transparent_disk_decrypt(blockno, dst_addr);
		 if(r)
			 return;
	 }

	// panic("bc_pgfault not implemented");

	// Check that the block we read was allocated. (exercise for
	// the reader: why do we do this *after* reading the block
	// in?)
	if (bitmap && block_is_free(blockno))
		panic("reading free block %08x\n", blockno);
}

// Flush the contents of the block containing VA out to disk if
// necessary, then clear the PTE_D bit using sys_page_map.
// If the block is not in the block cache or is not dirty, does
// nothing.
// Hint: Use va_is_mapped, va_is_dirty, and ide_write.
// Hint: Use the PTE_SYSCALL constant when calling sys_page_map.
// Hint: Don't forget to round addr down.
void
flush_block(void *addr)
{
	uint64_t blockno = ((uint64_t)addr - DISKMAP) / BLKSIZE;
	int r;

	if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
		panic("flush_block of bad va %08x", addr);

	// LAB 5: Your code here.
	void *dst_addr = (void *)(ROUNDDOWN(addr, PGSIZE));
	// Project addition --> Transparent disk encryption.
	if(va_is_mapped(dst_addr))
	{
		if(va_is_dirty(dst_addr))
		{
			if((blockno == 0) || (blockno == 2)) 
			 	ide_write(blockno * BLKSECTS, dst_addr, BLKSECTS);
			else if((blockno == 1))
			{
				if(!s_encrypted)
					ide_write(blockno * BLKSECTS, dst_addr, BLKSECTS);
				else
				{
					r = transparent_disk_encrypt(blockno, dst_addr);
					if(r)
						return;
				}
			}
			else 
			{ 
				r = transparent_disk_encrypt(blockno, dst_addr);
				if(r)
					return;
			}
			sys_page_map(thisenv->env_id, dst_addr, thisenv->env_id, dst_addr, PTE_SYSCALL);
		}
	}

//	panic("flush_block not implemented");
}

// Test that the block cache works, by smashing the superblock and
// reading it back.
static void
check_bc(void)
{
	struct Super backup;

	// back up super block
	memmove(&backup, diskaddr(1), sizeof backup);

	// smash it
	strcpy(diskaddr(1), "OOPS!\n");
	flush_block(diskaddr(1));
	assert(va_is_mapped(diskaddr(1)));
	assert(!va_is_dirty(diskaddr(1)));

	// clear it out
	sys_page_unmap(0, diskaddr(1));
	assert(!va_is_mapped(diskaddr(1)));

	// read it back in
	assert(strcmp(diskaddr(1), "OOPS!\n") == 0);

	// fix it
	memmove(diskaddr(1), &backup, sizeof backup);
	flush_block(diskaddr(1));

	cprintf("block cache is good\n");
}

void
bc_init(void)
{
	set_pgfault_handler(bc_pgfault);
	//cprintf("in bc_init hello\n");
	check_bc();
}

