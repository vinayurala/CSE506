#ifndef JOS_AES_H
#define JOS_AES_H

#include <inc/types.h>
/*
#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif
*/

typedef uint8_t uint8;
typedef uint32_t uint32;

typedef struct
{
    uint32_t erk[64];     /* encryption round keys */
    uint32_t drk[64];     /* decryption round keys */
    int nr;             /* number of rounds */
}
aes_context;

int  aes_set_key( aes_context *ctx, uint8_t *key, int nbits );
void aes_encrypt( aes_context *ctx, uint8_t input[16], uint8_t output[16] );
void aes_decrypt( aes_context *ctx, uint8_t input[16], uint8_t output[16] );

#endif /* aes.h */
