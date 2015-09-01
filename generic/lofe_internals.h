#ifndef __LOFE_INTERNALS_H__
#define __LOFE_INTERNALS_H__

#include <stdlib.h>
#include <stdint.h>
#include "lofe_aes.h"

typedef struct lofe_header_struct_t {
	uint64_t info;
	uint64_t content_len;
	uint64_t iv[2];
} lofe_header_t;

#define BLOCK_SIZE 16
	
static off_t align_start(off_t offset){
    off_t aligned_offset;
    size_t align_offset_mask = BLOCK_SIZE-1;
    align_offset_mask = ~align_offset_mask;
    aligned_offset = offset & align_offset_mask;
    return aligned_offset;
}
static off_t align_end(off_t offset){
    off_t aligned_offset;
    size_t align_offset_mask = BLOCK_SIZE-1;
	aligned_offset=offset;
    if(aligned_offset & align_offset_mask){
		aligned_offset &= ~align_offset_mask;
		aligned_offset += BLOCK_SIZE;
	}
    return aligned_offset;
}
static void align_size_offset(size_t size, off_t offset, size_t *aligned_size, off_t *aligned_offset){
    *aligned_offset = align_start(offset);
    off_t end_offset = offset+size;
    off_t aligned_end_offset = align_end(end_offset);
    *aligned_size = aligned_end_offset - *aligned_offset;
}

void lofe_encrypt_block(int8_t *dst,int8_t *src, uint64_t iv[2], uint64_t offset);
void lofe_decrypt_block(int8_t *dst,int8_t *src,uint64_t iv[2], uint64_t offset);

/*
static void lofe_encrypt_block(int8_t *dst,int8_t *src, uint64_t iv[2], uint64_t offset){
	aes_enc128(src,dst);
}
static void lofe_decrypt_block(int8_t *dst,int8_t *src,uint64_t iv[2], uint64_t offset){
	aes_dec128(src,dst);
}
*/

#endif //__LOFE_INTERNALS_H__
