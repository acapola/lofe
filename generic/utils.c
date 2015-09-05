
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

