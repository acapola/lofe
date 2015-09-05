#ifndef __LOFE_INTERNALS_H__
#define __LOFE_INTERNALS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "osal.h"
#include "lofe_aes.h"

typedef struct lofe_header_struct_t {
	uint64_t info;
	uint64_t content_len;
	uint64_t iv[2];
} lofe_header_t;

#define BLOCK_SIZE 16

off_t align_start(off_t offset);
off_t align_end(off_t offset);
void align_size_offset(size_t size, off_t offset, size_t *aligned_size, off_t *aligned_offset);

void lofe_load_key(int8_t *key,int8_t *key_tweak);
void lofe_encrypt_block(int8_t *dst,int8_t *src, uint64_t iv[2], uint64_t offset);
void lofe_decrypt_block(int8_t *dst,int8_t *src,uint64_t iv[2], uint64_t offset);


int lofe_start_vfs(char *encrypted_files_path, char *mount_point);

#ifdef __cplusplus
}
#endif

#endif //__LOFE_INTERNALS_H__
