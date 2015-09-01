
#ifndef __LOFE_AES_H__
#define __LOFE_AES_H__

#include <stdint.h>     //for int8_t

void aes_load_key128(int8_t *enc_key);
void aes_enc128(int8_t *plainText,int8_t *cipherText);
void aes_dec128(int8_t *plainText,int8_t *cipherText);
int aes_self_test128(void);

#endif //__LOFE_AES_H__