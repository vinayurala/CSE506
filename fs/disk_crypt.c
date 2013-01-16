#include <inc/disk_crypt.h>
#include <inc/aes.h>
#include <fs/fs.h>
#include <inc/memlayout.h>
#include <inc/lib.h>

//#define TEST 
#define TEST_BLOCK 8867

aes_context ctx;
aes_context ctx_crypto;
char passwd[32];
void *crypto_page;
int crypto_page_mapped;
int s_encrypted;

void 
hexdump(void *addr, char *type)
{
	int *temp = (int *)addr;
	int i;

	cprintf("\nFirst 16 bytes of %s: \n", type);
	cprintf("0x%08x: ", temp);
	for(i = 0; i < 8; i++)
	{
		cprintf("0x%08x  ", temp[i]);
		if(i == 3)
		{
			cprintf("\n");
			cprintf("0x%08x: ", (temp + 0x04));
		}
	}
	cprintf("\n");

}

uint32_t 
get_nblocks()
{
	struct Super *s = diskaddr(1);
	return s->s_nblocks;
}

int
store_passwd(int flag)
{
	char *passwd_check;
	int i;

	if(!flag)
	{
		cprintf("\nSince there is no compatible TPM, please enter a password to encrypt the disk. Please choose a combination of" \
			" alphanumeric characters as your password (Character limit: 32)\n");
	}
	while(1)
	{
		close(0);
		if((i = opencons()) < 0)
		{
			cprintf("\nopencons error: %e\n", i);
			return i;
		}
		if((i = dup(0, 1)) < 0)
		{
			cprintf("\ndup error: %e", i);
			return i;
		} 
		passwd_check = readpasswd("Enter password: ");
		
		if(passwd_check[0] == '\0')
		{
			cprintf("\nNo password entered!\n");
			continue;
		}
		if((strlen(passwd_check)) > 32)
		{
			cprintf("\nEnter a password less than 32 characters\n");
			continue;
		}
		passwd_check[strlen(passwd_check)] = '\0';
		memmove(passwd, passwd_check, strlen(passwd_check));
		memset(passwd_check, 0, strlen(passwd_check));

		if(flag)
			return 1;

		passwd_check = readpasswd("Re-enter password: ");
		passwd_check[strlen(passwd_check)] = '\0';

		if(!(strcmp(passwd, passwd_check)))
			return 1;
	
		cprintf("\nPasswords do not match! Please enter them again.\n");
	}
	return 0;
}

int 
init_encrypt(uint8_t *key)
{
	int r, i;

	aes_set_key(&ctx, key, 256);

        // Reserve block 10239(last block available) for now, need it for saving AES context
	// and it's keys.
	if((r = alloc_crypto_block(CRYPTO_BLOCK)))
	{
		cprintf("\nCannot allocate crypto block! Reason: %e\n", r);
		return 0;
	}

	if(!(r = store_passwd(0)))
	{
		cprintf("\nSet password error: %e\n", r);
		free_crypto_block(CRYPTO_BLOCK);
		return 0;
	}
	s_encrypted = 0;
	return 1;
}

int 
full_disk_encrypt()
{
	int perm = PTE_U | PTE_W | PTE_P;
	int i, blk_no, j, r, enc_blk;
	uint8_t ptext_buf[16], ctext_buf[16];
	// AES key, it is encrypted on disk, although it would have been ideal if it were random
	uint8_t key[32] = "Sample AES key for JOS";
	int *crypto_magic = malloc(sizeof(int));
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ctext_scratch = (void *)(UTEMP + 2 * PGSIZE);

	uint32_t nblocks = get_nblocks();

	if(!(init_encrypt(key)))
		return -E_GTH;

	if((r = sys_page_alloc(0, crypto_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		free_crypto_block(CRYPTO_BLOCK);
		return -E_GTH;
	}

	if((r = sys_page_alloc(0, ctext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		free_crypto_block(CRYPTO_BLOCK);
		return -E_GTH;
	}

	// Dump only key and the magic number to CRYPTO_BLOCK
	// We'll try optimizing this, but bitmap works at the block level
	// so there's not much room for optimization.

	memset(crypto_scratch, 0, PGSIZE);
	*crypto_magic = JEDI;
	memmove(crypto_page, (void *)crypto_magic, sizeof(crypto_magic));
	memmove((crypto_page + sizeof(crypto_magic)), key, sizeof(key));
	ide_write(CRYPTO_BLOCK * BLKSECTS, crypto_page, 8);

	cprintf("\nEncrypting disk.");
	for(i = 3; i <= nblocks-2; i++)
	{
		if(!(block_is_free(i)))
		{
			// Theatricality
			if(!(i % 512))
				cprintf(".");
			ide_read(i * BLKSECTS, crypto_scratch, BLKSECTS);
			for(j = 0; j < PGSIZE; j += 16)
			{
				memmove(ptext_buf, (char *)(crypto_scratch + j), 16);
				aes_encrypt(&ctx, ptext_buf, ctext_buf);
				memmove((char *)(ctext_scratch + j), ctext_buf, 16); 
			}
			ide_write(i * BLKSECTS, ctext_scratch, BLKSECTS);
			ide_write(CRYPTO_BLOCK * BLKSECTS, crypto_page, 8);
		}
	}
	encrypt_crypto_block(passwd);
	encrypt_s_block();

	// Obliterate everything
	memset(crypto_scratch, 0, PGSIZE);
	memset(ctext_scratch, 0, PGSIZE);
	memset(key, 0, 32);
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(&ctx, 0, sizeof(ctx));
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(&ctx, 0, sizeof(ctx));
	i = 0;

	sys_page_unmap(0, crypto_scratch);
	sys_page_unmap(0, ctext_scratch);

	return 0;
}

int
transparent_disk_decrypt(int block_no, void *dst_addr)
{
	int perm = PTE_U | PTE_W | PTE_P;
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ptext_scratch = (void *)(UTEMP + 2 * PGSIZE);
	int r, j;
	uint8_t ptext_buf[16], ctext_buf[16];
	uint8_t *key;

	if((r = sys_page_alloc(0, crypto_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}
	if((r = sys_page_alloc(0, ptext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}

	if(!crypto_page_mapped)
	{
		decrypt_crypto_block(passwd);
		crypto_page_mapped = 1;
	}

	key = (uint8_t *)(crypto_page + 8);
	aes_set_key(&ctx, key, 256);
	memset(crypto_scratch, 0, PGSIZE);

	ide_read(block_no * BLKSECTS, crypto_scratch, BLKSECTS);
	for(j = 0; j < PGSIZE; j += 16)
	{
		memmove(ctext_buf, (char *)(crypto_scratch + j), 16);
		aes_decrypt(&ctx, ctext_buf, ptext_buf);
		memmove((char *)(ptext_scratch + j), ptext_buf, 16); 
	}

	memmove(dst_addr, ptext_scratch, PGSIZE);

#ifdef TEST
	if(block_no == TEST_BLOCK)
	{
		cprintf("\nBlock no. : %d\n", block_no);
		hexdump(crypto_scratch, "encrypted contents (stored on disk)");
		hexdump(dst_addr, "decrypted contents (mapped in memory)");
	}
#endif

	// Zero out all locally created arrays
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(crypto_scratch, 0, PGSIZE);
	memset(ptext_scratch, 0, PGSIZE);

	return 0;
}

int
transparent_disk_encrypt(int block_no, void *src_addr)
{
	int perm = PTE_U | PTE_W | PTE_P;
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ctext_scratch = (void *)(UTEMP + 2 * PGSIZE);
	int r, j;
	uint8_t ptext_buf[16], ctext_buf[16];

	if((r = sys_page_alloc(0, crypto_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}
	if((r = sys_page_alloc(0, ctext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}
	if(!crypto_page_mapped)
		decrypt_crypto_block(passwd);

	uint8_t *key = (uint8_t *)(crypto_page + 8);
	aes_set_key(&ctx, key, 256);
	memset(crypto_scratch, 0, PGSIZE);

	for(j = 0; j < PGSIZE; j += 16)
	{
		memmove(ptext_buf, (char *)(src_addr + j), 16);
		aes_encrypt(&ctx, ptext_buf, ctext_buf);
		memmove((char *)(ctext_scratch + j), ctext_buf, 16); 
	}

	ide_write(block_no * BLKSECTS, ctext_scratch, BLKSECTS);

	// Zero out all locally created arrays
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(crypto_scratch, 0, PGSIZE);
	memset(ctext_scratch, 0, PGSIZE);
	memset(&ctx, 0, sizeof(ctx));

	return 0;
}

int 
encrypt_crypto_block(char passwd_local[32])
{
	int perm = PTE_U | PTE_W | PTE_P;
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ctext_scratch = (void *)(UTEMP + 2 * PGSIZE);
	int r, j;
	uint8_t ptext_buf[16], ctext_buf[16];

	if((r = sys_page_alloc(0, ctext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}

	aes_set_key(&ctx_crypto, (uint8_t *)passwd_local, 256);

	for(j = 0; j < PGSIZE; j += 16)
	{
		memmove(ptext_buf, (char *)(crypto_page + j), 16);
		aes_encrypt(&ctx_crypto, ptext_buf, ctext_buf);
		memmove((char *)(ctext_scratch + j), ctext_buf, 16); 
	}

	ide_write(CRYPTO_BLOCK * BLKSECTS, ctext_scratch, BLKSECTS);

	// Zero out all locally created arrays
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(crypto_scratch, 0, PGSIZE);
	memset(ctext_scratch, 0, PGSIZE);

	return 0;

}

int 
decrypt_crypto_block(char passwd_local[32])
{
	int perm = PTE_U | PTE_W | PTE_P;
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ptext_scratch = (void *)(UTEMP + 2 * PGSIZE);
	int r, j;
	uint8_t ptext_buf[16], ctext_buf[16];

	if((r = sys_page_alloc(0, ptext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}

	if((r = sys_page_alloc(0, crypto_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}

	aes_set_key(&ctx_crypto, (uint8_t *)passwd_local, 256);
	ide_read(CRYPTO_BLOCK * BLKSECTS, crypto_scratch, BLKSECTS);

	for(j = 0; j < PGSIZE; j += 16)
	{
		memmove(ctext_buf, (char *)(crypto_scratch + j), 16);
		aes_decrypt(&ctx_crypto, ctext_buf, ptext_buf);
		memmove((char *)(ptext_scratch + j), ptext_buf, 16); 
	}

	memmove(crypto_page, ptext_scratch, PGSIZE);
	// Zero out all locally created arrays
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(crypto_scratch, 0, PGSIZE);
	memset(ptext_scratch, 0, PGSIZE);

	return 0;

}
int 
encrypt_s_block()
{
	int perm = PTE_U | PTE_W | PTE_P;
	void *crypto_scratch = (void *)(UTEMP + PGSIZE);
	void *ctext_scratch = (void *)(UTEMP + 2 * PGSIZE);
	int r, j;
	uint8_t ptext_buf[16], ctext_buf[16];

	if((r = sys_page_alloc(0, ctext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}
	if((r = sys_page_alloc(0, ctext_scratch, perm)))
	{
		cprintf("\nCannot allocate temp page. Error code: %e\n", r);
		return -E_GTH;
	}

	if(!crypto_page_mapped)
	{
		decrypt_crypto_block(passwd);
		crypto_page_mapped = 1;
	}

	uint8_t *key = (uint8_t *)(crypto_page + 8);
	aes_set_key(&ctx, key, 256);
	memset(crypto_scratch, 0, PGSIZE);

	ide_read(1 * BLKSECTS, crypto_scratch, BLKSECTS);
	for(j = 0; j < PGSIZE; j += 16)
	{
		memmove(ptext_buf, (char *)(crypto_scratch + j), 16);
		aes_encrypt(&ctx, ptext_buf, ctext_buf);
		memmove((char *)(ctext_scratch + j), ctext_buf, 16); 
	}
	
	ide_write(1 * BLKSECTS, ctext_scratch, BLKSECTS);
	s_encrypted = 1;

	// Zero out all locally created arrays
	memset(ptext_buf, 0, sizeof(ptext_buf));
	memset(ctext_buf, 0, sizeof(ctext_buf));
	memset(crypto_scratch, 0, PGSIZE);
	memset(ctext_scratch, 0, PGSIZE);
	memset(&ctx, 0, sizeof(ctx));

	return 0;
}
