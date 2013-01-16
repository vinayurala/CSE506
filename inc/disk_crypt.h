#ifndef JOS_DISK_CRYPT_H
#define JOS_DISK_CRYPT_H

#define JEDI 0x4A454449 //JEDI - JOS Encrypted Disk Identifier 
#define CRYPTO_BLOCK 10239

extern void *crypto_page;
extern int crypto_page_mapped;
extern char passwd[32];
extern int s_encrypted;

int full_disk_encrypt();
int transparent_disk_decrypt(int, void *);
int transparent_disk_encrypt(int, void *);
int encrypt_crypto_block(char [32]);
int decrypt_crypto_block(char [32]);
int store_passwd(int);
int encrypt_s_block();

#endif
