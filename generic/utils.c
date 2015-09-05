
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
	*aligned_size = aligned_end_offset - *aligned_offset;
}

