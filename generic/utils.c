#include <string.h>
#include "lofe_internals.h"

off_t align_start(off_t offset) {
	off_t aligned_offset;
	size_t align_offset_mask = BLOCK_SIZE - 1;
	align_offset_mask = ~align_offset_mask;
	aligned_offset = offset & align_offset_mask;
	return aligned_offset;
}
off_t align_end(off_t offset) {
	off_t aligned_offset;
	size_t align_offset_mask = BLOCK_SIZE - 1;
	aligned_offset = offset;
	if (aligned_offset & align_offset_mask) {
		aligned_offset &= ~align_offset_mask;
		aligned_offset += BLOCK_SIZE;
	}
	return aligned_offset;
}
void align_size_offset(size_t size, off_t offset, size_t *aligned_size, off_t *aligned_offset) {
	*aligned_offset = align_start(offset);
	off_t end_offset = offset + size;
	off_t aligned_end_offset = align_end(end_offset);
	*aligned_size = (size_t)(aligned_end_offset - *aligned_offset);
}

int lofe_write_new_header(lofe_file_handle_t h, uint64_t len) {
	lofe_header_t header;
	header.info = 0;
	header.iv[0] = 0x0123456789ABCDEF;
	header.iv[1] = 0x1122334455667788;
	//get_random(header.iv, sizeof(header.iv));
	header.content_len = len;
	return lofe_write_header(h, header);
}

int lofe_update_header_len(lofe_file_handle_t h, uint64_t content_len) {
	lofe_header_t header;
	if (lofe_read_header(h, &header)) return -1;
	header.content_len = content_len;
	if (lofe_write_header(h, header)) return -1;
	return 0;
}

//returns:
// -1 if error
//  0 if offset is beyond EOF
//  otherwise the number of byte read (can be smaller than requested if we hit the end of file)
int lofe_read(lofe_file_handle_t h, int8_t *buf, size_t size, off_t offset){
	int res;
	size_t aligned_size;
	off_t aligned_offset;
	off_t read_offset;
	size_t read_size;
	lofe_header_t header;
	size_t n_blocks;
	size_t regular_blocks;
	size_t block;
	size_t last_block_unaligned_size;
	off_t content_end_read_pos;
	
	if (offset < 0) return -1;

	//get the current size of the content, store it in header.content_len
	if(lofe_read_header(h,&header)) return -1;
	
	//Check EOF condition
	if(((uint64_t)offset)>=header.content_len) return 0;
	
	//adjust the size so that we don't read out of the actual content (that can happen due to padding)
	read_size = size;
	if(((uint64_t)(offset+size))>header.content_len){
		read_size = (size_t)(header.content_len-offset);
	}
	
	content_end_read_pos = offset + read_size;
	align_size_offset(read_size,offset,&aligned_size,&aligned_offset);
	read_offset = aligned_offset+sizeof(lofe_header_t);
	n_blocks = aligned_size / BLOCK_SIZE;
	regular_blocks = n_blocks;
	last_block_unaligned_size = content_end_read_pos % BLOCK_SIZE;
	
	if(last_block_unaligned_size) 
		regular_blocks--;//remove the last block from the count of regular blocks

	//process initial block if it is not aligned, otherwise it is treated as a regular block
	if(aligned_offset!=offset){
		off_t unaligned_offset = offset-aligned_offset;
		size_t unaligned_size = BLOCK_SIZE-read_size;
		int8_t aligned_buf[BLOCK_SIZE];
	
		//read unaligned data
		res=lofe_read_block(h, aligned_buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			return -1;
		}
			
		//copy to output buffer
		memcpy(buf,aligned_buf+unaligned_offset,unaligned_size);
		read_offset+=BLOCK_SIZE;
		buf+=unaligned_offset;
	}
	
	//process regular blocks
	for(block=0;block<regular_blocks;block++){
		res=lofe_read_block(h, buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			return -1;
		}	
		read_offset+=BLOCK_SIZE;
		buf+=BLOCK_SIZE;
	}
	
	//process final block if it is not aligned, otherwise it was treated as a regular block
	if(last_block_unaligned_size){
		int8_t aligned_buf[BLOCK_SIZE];
		
		//read end of the block from file
		res=lofe_read_block(h, aligned_buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			return -1;
		}
		
		//copy to output buffer
		memcpy(buf,aligned_buf,last_block_unaligned_size);
	}
	return read_size;
}


int lofe_write(lofe_file_handle_t h, const int8_t * const _buf, size_t size, off_t offset) {
	int res;
	lofe_header_t header;
	off_t content_end_write_pos;
	size_t aligned_size;
	off_t aligned_offset;
	off_t write_offset;
	size_t n_blocks;
	size_t regular_blocks;
	size_t block;
	size_t last_block_unaligned_size;
    int8_t * buf = (int8_t*) _buf;

	if (offset < 0) return -1;

	//get the current size of the content, store it in header.content_len
	if (lofe_read_header(h, &header)) return -1;

	content_end_write_pos = offset + size;
	align_size_offset(size, offset, &aligned_size, &aligned_offset);
	write_offset = aligned_offset + sizeof(lofe_header_t);
	n_blocks = aligned_size / BLOCK_SIZE;
	regular_blocks = n_blocks;
	last_block_unaligned_size = content_end_write_pos % BLOCK_SIZE;

	if (last_block_unaligned_size)
		regular_blocks--;//remove the last block from the count of regular blocks

						 //process initial block if it is not aligned, otherwise it is treated as a regular block
	if (aligned_offset != offset) {
		size_t read_size = (size_t)(offset - aligned_offset);
		off_t unaligned_offset = read_size;
		size_t unaligned_size = BLOCK_SIZE - read_size;
		int8_t aligned_buf[BLOCK_SIZE];

		if (0 == regular_blocks) {//first block = last block
			if (last_block_unaligned_size) {
				read_size = BLOCK_SIZE;
				last_block_unaligned_size = 0;
			}
		}
		else {
			regular_blocks--;//remove the first block from the count of regular blocks.
		}

		//read beginning of the block from file
		//res = pread(fd, aligned_buf, read_size, write_offset);
		res = lofe_read_block(h, aligned_buf, write_offset, &header);
		if (res < 0) { //0 is OK here, it just means end of file, so we are doing a write that will grow the file.
			return -1;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf + unaligned_offset, buf, unaligned_size);
		//write the aligned block to file
		res = lofe_write_block(h, aligned_buf, write_offset, &header);
		if (res == -1) {
			return -1;
		}
		write_offset += BLOCK_SIZE;
		buf += unaligned_size;
	}

	//process regular blocks
	for (block = 0; block<regular_blocks; block++) {
		res = lofe_write_block(h, buf, write_offset, &header);
		if (res == -1) {
			return -1;
		}
		write_offset += BLOCK_SIZE;
		buf += BLOCK_SIZE;
	}

	//process final block if it is not aligned, otherwise it was treated as a regular block
	if (last_block_unaligned_size) {
		//off_t read_size = BLOCK_SIZE - last_block_unaligned_size;
		int8_t aligned_buf[BLOCK_SIZE];

		//read end of the block from file
		res = lofe_read_block(h, aligned_buf, write_offset, &header);
		if (res < 0) { //0 is OK here, it just means end of file, so we are doing a write that will grow the file.
			return -1;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf, buf, last_block_unaligned_size);
		//write the aligned block to file
		res = lofe_write_block(h, aligned_buf, write_offset, &header);
		if (res == -1) {
			return -1;
		}
	}

	//if the file grew, update the header
	if (header.content_len< (uint64_t)content_end_write_pos) {
		res = lofe_update_header_len(h, content_end_write_pos);
		if (res) {
			return -1;
		}
	}
	return 0;
}


