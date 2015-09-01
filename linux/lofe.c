/*
FUSE: Filesystem in Userspace
Copyright (C) 2001-2007 Miklos Szeredi <miklos@szeredi.hu>
Copyright (C) 2011 Sebastian Pipping <sebastian@pipping.org>
This program can be distributed under the terms of the GNU GPL.
See the file COPYING.
*/
#define FUSE_USE_VERSION 30
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include "lofe_internals.h"

static char *base_path;
static unsigned int base_path_len;

static FILE *urandom=0;
static void get_random(int8_t*buf, unsigned int len){
	unsigned int byte_count;
	int res = fread(buf, 1, len, urandom);
	if(res!=len){
		if(res == -1){
			printf("FATAL_ERROR: can't read urandom!\n");
			exit(-errno);
		} else {
			printf("FATAL_ERROR: urandom did not return enough data!\n");
			exit(-1);
		}
	}
}



static void translate_path(const char *path_from_sys, char *actual_path_ptr){
	//printf("path_from_sys=%s\n",path_from_sys);
	strcpy(actual_path_ptr,base_path);
    strcpy(actual_path_ptr+base_path_len,path_from_sys);
	printf("actual_path_ptr=%s\n",actual_path_ptr);
}


static int write_header(int fd, uint64_t len){
	lofe_header_t header;
	header.info=0;
	//header.iv[0] = 0x0123456789ABCDEF;
	//header.iv[1] = 0x1122334455667788;
	get_random(header.iv, sizeof(header.iv));
	header.content_len=len;
	int res = pwrite(fd, &header, sizeof(header), 0);
	if (res == -1){
		printf("write_header failure: %d\n",errno);
		res = -errno;
	}
	return 0;
}

static int read_header2(const char *native_path, lofe_header_t *header){
	int fd;
	int res;
	fd = open(native_path, O_RDONLY);
	if (fd == -1)
		return -errno;
	res = pread(fd, header, sizeof(lofe_header_t), 0);
	if (res == -1)
		return -errno;	
	close(fd);
	return 0;
}

static int write_header2(const char *native_path, lofe_header_t new_header){
	int fd;
	int res;
	fd = open(native_path, O_WRONLY);
	if (fd == -1)
		return -errno;
	res = pwrite(fd, &new_header, sizeof(lofe_header_t), 0);
	if (res == -1)
		return -errno;	
	close(fd);
	return 0;
}

static int update_header_len(int fd, uint64_t content_len){
	int res;
	lofe_header_t header;
	res = pread(fd, &header, sizeof(lofe_header_t), 0);
	header.content_len = content_len;
	res = pwrite(fd, &header, sizeof(lofe_header_t), 0);
	if (res == -1)
		return -errno;	
	return 0;
}

static int update_header_len2(const char *native_path, uint64_t len){
	int fd;
	int res;
	fd = open(native_path, O_RDWR);
	if (fd == -1)
		return -errno;
	res=update_header_len(fd,len);
	if (res == -1)
		return -errno;	
	close(fd);
	return 0;
}

static int lofe_read_block(int fd, int8_t*buf, off_t offset, const lofe_header_t * const header){
	int8_t enc_buf[BLOCK_SIZE];
	if(offset%BLOCK_SIZE) {
		printf("ERROR: lofe_read_block, unaligned offset: %X\n",offset);
		return -EINVAL;
	}
	int res = pread(fd, enc_buf, BLOCK_SIZE, offset);
	if (res <= 0){
		return res;
	}
	if(res!=BLOCK_SIZE){
		printf("ERROR: lofe_read_block, could not read all the requested bytes\n");
		return -EIO;
	}
	lofe_decrypt_block(buf,enc_buf,header->iv,offset);
	return BLOCK_SIZE;
}

static int lofe_write_block(int fd, const int8_t * const buf, off_t offset, const lofe_header_t * const header){
	int8_t enc_buf[BLOCK_SIZE];
	if(offset%BLOCK_SIZE) {
		printf("ERROR: lofe_write_block, unaligned offset: %X\n",offset);
		return -EINVAL;
	}
	lofe_encrypt_block(enc_buf,buf,header->iv,offset);
	int res = pwrite(fd, enc_buf, BLOCK_SIZE, offset);
	if (res == -1){
		return -1;
	}
	if(res!=BLOCK_SIZE){
		printf("ERROR: lofe_write_block, could not write all the requested bytes\n");
		return -EIO;
	}
	return 0;
}


static int xmp_getattr(const char *path, struct stat *stbuf){
    printf("xmp_getattr\n");
	int res;
	lofe_header_t header;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;
	if(S_IFREG==(stbuf->st_mode & S_IFMT)){//if regular file
        res = read_header2(path, &header);
        if (res)
            return res;
        stbuf->st_size=header.content_len;//return the size of the content
    }
    return 0;
}
static int xmp_access(const char *path, int mask){
	printf("xmp_access\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = access(path, mask);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
	printf("xmp_readlink\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;
	buf[res] = '\0';
	return 0;
}
static int xmp_readdir(
			const char *path, 
			void *buf, 
			fuse_fill_dir_t filler,
			off_t offset, 
			struct fuse_file_info *fi){
	printf("xmp_readdir\n");
	DIR *dp;
	struct dirent *de;
	(void) offset;
	(void) fi;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    dp = opendir(path);
	if (dp == NULL)
		return -errno;
	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
		break;
	}
	closedir(dp);
	return 0;
}

//create a file
static int xmp_mknod(const char *path, mode_t mode, dev_t rdev){
	printf("xmp_mknod\n");
	int res;
	int fd;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    /* On Linux this could just be 'mknod(path, mode, rdev)' but this
	is more portable */
	if (S_ISREG(mode)) {
		fd = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (fd >= 0){
			int res2=write_header(fd,0);
			if(res2) res2 = -errno;
			res = close(fd);
			if(res2) return res2;
			if(res) return -errno;
		}
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_mkdir(const char *path, mode_t mode){
	printf("xmp_mkdir\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = mkdir(path, mode);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_unlink(const char *path){
	printf("xmp_unlink\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = unlink(path);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_rmdir(const char *path){
	printf("xmp_rmdir\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = rmdir(path);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_symlink(const char *from, const char *to){
	printf("xmp_symlink\n");
	int res;
		char actual_from_ptr[1024];
	unsigned int len_from = strlen(from)+base_path_len+1;//+1 for final null char
	if(len_from>sizeof(actual_from_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_from,sizeof(actual_from_ptr));
		exit(-1);
	}
	translate_path(from,actual_from_ptr);
    from = actual_from_ptr;

    	char actual_to_ptr[1024];
	unsigned int len_to = strlen(to)+base_path_len+1;//+1 for final null char
	if(len_to>sizeof(actual_to_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_to,sizeof(actual_to_ptr));
		exit(-1);
	}
	translate_path(to,actual_to_ptr);
    to = actual_to_ptr;

    res = symlink(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_rename(const char *from, const char *to){
	printf("xmp_rename\n");
	int res;
		char actual_from_ptr[1024];
	unsigned int len_from = strlen(from)+base_path_len+1;//+1 for final null char
	if(len_from>sizeof(actual_from_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_from,sizeof(actual_from_ptr));
		exit(-1);
	}
	translate_path(from,actual_from_ptr);
    from = actual_from_ptr;

    	char actual_to_ptr[1024];
	unsigned int len_to = strlen(to)+base_path_len+1;//+1 for final null char
	if(len_to>sizeof(actual_to_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_to,sizeof(actual_to_ptr));
		exit(-1);
	}
	translate_path(to,actual_to_ptr);
    to = actual_to_ptr;

    res = rename(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_link(const char *from, const char *to){
	printf("xmp_link\n");
	int res;
		char actual_from_ptr[1024];
	unsigned int len_from = strlen(from)+base_path_len+1;//+1 for final null char
	if(len_from>sizeof(actual_from_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_from,sizeof(actual_from_ptr));
		exit(-1);
	}
	translate_path(from,actual_from_ptr);
    from = actual_from_ptr;

    	char actual_to_ptr[1024];
	unsigned int len_to = strlen(to)+base_path_len+1;//+1 for final null char
	if(len_to>sizeof(actual_to_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_to,sizeof(actual_to_ptr));
		exit(-1);
	}
	translate_path(to,actual_to_ptr);
    to = actual_to_ptr;

    res = link(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_chmod(const char *path, mode_t mode){
	printf("xmp_chmod\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = chmod(path, mode);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_chown(const char *path, uid_t uid, gid_t gid){
	printf("xmp_chown\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_truncate(const char *path, off_t size){
	printf("xmp_truncate\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

	//update header
	res = update_header_len2(path, size);//write the requested size in the header
	if (res)
		return res;
	size+=sizeof(lofe_header_t);//keep the header, truncate only the content to the requested size.
	size = align_end(size);//keep actual file size a multiple of the block size;
    res = truncate(path, size);
	if (res == -1)
		return -errno;
	return 0;
}
#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2]){
	printf("xmp_utimens\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    /* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
	return -errno;
	return 0;
}
#endif
static int xmp_open(const char *path, struct fuse_file_info *fi){
	int fd;
	int res=0;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

	printf("openning %s\n",path);
	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;
	close(fd);
	return res;
}
static int xmp_read(
			const char *path, 
			char *buf, 
			size_t size, 
			off_t offset,
			struct fuse_file_info *fi){
	printf("xmp_read\n");
	int fd;
	int res;
	size_t aligned_size;
	off_t aligned_offset;
	size_t read_offset;
	size_t read_size;
	lofe_header_t header;
	size_t n_blocks;
	size_t regular_blocks;
	size_t block;
	size_t last_block_unaligned_size;
	off_t content_end_read_pos;
	(void) fi;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;
	
	//get the current size of the content, store it in header.content_len
	res = pread(fd, &header, sizeof(lofe_header_t), 0);
	if (res == -1){
		close(fd);
		return -errno;
	}
	
	//Check EOF condition
	if(offset>=header.content_len){
		close(fd);
		return 0;
	}
	
	//adjust the size so that we don't read out of the actual content (that can happen due to padding)
	read_size = size;
	if(offset+size>header.content_len){
		read_size = header.content_len-offset;
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
		off_t unaligned_size = BLOCK_SIZE-read_size;
		int8_t aligned_buf[BLOCK_SIZE];
	
		//read unaligned data
		res=lofe_read_block(fd, aligned_buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			close(fd);
			return -errno;
		}
			
		//copy to output buffer
		memcpy(buf,aligned_buf+unaligned_offset,unaligned_size);
		read_offset+=BLOCK_SIZE;
		buf+=unaligned_offset;
	}
	
	//process regular blocks
	for(block=0;block<regular_blocks;block++){
		res=lofe_read_block(fd, buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			close(fd);
			return -errno;
		}	
		read_offset+=BLOCK_SIZE;
		buf+=BLOCK_SIZE;
	}
	
	//process final block if it is not aligned, otherwise it was treated as a regular block
	if(last_block_unaligned_size){
		int8_t aligned_buf[BLOCK_SIZE];
		
		//read end of the block from file
		res=lofe_read_block(fd, aligned_buf, read_offset, &header);
		if (res <= 0){//should not get EOF here
			close(fd);
			return -errno;
		}
		
		//copy to output buffer
		memcpy(buf,aligned_buf,last_block_unaligned_size);
	}
	close(fd);
	
	return read_size;
}

static int xmp_write(
			const char *path, 
			const char *buf, 
			size_t size,
			off_t offset, 
			struct fuse_file_info *fi){
	printf("xmp_write\n");
	int fd;
	int res;
	lofe_header_t header;
	off_t content_end_write_pos;
	size_t aligned_size;
	off_t aligned_offset;
	size_t write_offset;
	size_t n_blocks;
	size_t regular_blocks;
	size_t block;
	size_t last_block_unaligned_size;
	struct stat st;
	(void) fi;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    fd = open(path, O_RDWR);
	if (fd == -1)
		return -errno;
	
	//get the current size of the content, store it in header.content_len
	res = pread(fd, &header, sizeof(lofe_header_t), 0);
	if (res == -1){
		close(fd);
		return -errno;
	}
	
	content_end_write_pos = offset + size;
	align_size_offset(size,offset,&aligned_size,&aligned_offset);
	write_offset = aligned_offset+sizeof(lofe_header_t);
	n_blocks = aligned_size / BLOCK_SIZE;
	regular_blocks = n_blocks;
	last_block_unaligned_size = content_end_write_pos % BLOCK_SIZE;
		
	if(last_block_unaligned_size) 
		regular_blocks--;//remove the last block from the count of regular blocks

	//process initial block if it is not aligned, otherwise it is treated as a regular block
	if(aligned_offset!=offset){
		off_t read_size = offset-aligned_offset;
		off_t unaligned_offset = read_size;
		off_t unaligned_size = BLOCK_SIZE-read_size;
		int8_t aligned_buf[BLOCK_SIZE];
		
        if(0==regular_blocks){//first block = last block
            if(last_block_unaligned_size){
                read_size=BLOCK_SIZE;
                last_block_unaligned_size=0;
            }
        } else {
            regular_blocks--;//remove the first block from the count of regular blocks.
		}
		
		//read beginning of the block from file
		//res = pread(fd, aligned_buf, read_size, write_offset);
		res = lofe_read_block(fd, aligned_buf, write_offset, &header);
		if (res < 0){ //0 is OK here, it just means end of file, so we are doing a write that will grow the file.
			close(fd);
			return -errno;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf+unaligned_offset,buf,unaligned_size);
		//write the aligned block to file
		res = lofe_write_block(fd, aligned_buf, write_offset, &header);
		if (res == -1){
			close(fd);
			return -errno;
		}	
		write_offset+=BLOCK_SIZE;
		buf+=unaligned_size;
	} 
	
	//process regular blocks
	for(block=0;block<regular_blocks;block++){
		res = lofe_write_block(fd, buf, write_offset, &header);
		if (res == -1){
			close(fd);
			return -errno;
		}	
		write_offset+=BLOCK_SIZE;
		buf+=BLOCK_SIZE;
	}
	
	//process final block if it is not aligned, otherwise it was treated as a regular block
	if(last_block_unaligned_size){
		off_t read_size = BLOCK_SIZE-last_block_unaligned_size;
		int8_t aligned_buf[BLOCK_SIZE];
		
		//read end of the block from file
		res = lofe_read_block(fd, aligned_buf, write_offset, &header);
		if (res < 0){ //0 is OK here, it just means end of file, so we are doing a write that will grow the file.
			close(fd);
			return -errno;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf,buf,last_block_unaligned_size);
		//write the aligned block to file
		res = lofe_write_block(fd, aligned_buf, write_offset, &header);
		if (res == -1){
			close(fd);
			return -errno;
		}
	}
	
	//if the file grew, update the header
	if(header.content_len<content_end_write_pos){
		res = update_header_len(fd, content_end_write_pos);
		if (res){
			close(fd);
			return -errno;
		}		
	}
	
	close(fd);
	return size;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf){
	printf("xmp_statfs\n");
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi){
	/* Just a stub. This method is optional and can safely be left
	unimplemented */
	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(
			const char *path,
			int isdatasync,
			struct fuse_file_info *fi){
	/* Just a stub. This method is optional and can safely be left
	unimplemented */
	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(
			const char *path, 
			int mode,
			off_t offset, 
			off_t length, 
			struct fuse_file_info *fi){
	printf("xmp_fallocate\n");
	int fd;
	int res;
	(void) fi;
	if (mode)
		return -EOPNOTSUPP;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;
	res = -posix_fallocate(fd, offset, length);
	close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(
			const char *path, 
			const char *name, 
			const char *value,
			size_t size, 
			int flags){
	printf("xmp_setxattr\n");
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(
			const char *path, 
			const char *name, 
			char *value,
			size_t size){
	printf("xmp_getxattr\n");
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(
			const char *path, 
			char *list, 
			size_t size){
	printf("xmp_listxattr\n");
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(
			const char *path, 
			const char *name){
	printf("xmp_removexattr\n");
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.getattr = xmp_getattr,
	.access = xmp_access,
	.readlink = xmp_readlink,
	.readdir = xmp_readdir,
	.mknod = xmp_mknod,
	.mkdir = xmp_mkdir,
	.symlink = xmp_symlink,
	.unlink = xmp_unlink,
	.rmdir = xmp_rmdir,
	.rename = xmp_rename,
	.link = xmp_link,
	.chmod = xmp_chmod,
	.chown = xmp_chown,
	.truncate = xmp_truncate,
	#ifdef HAVE_UTIMENSAT
	.utimens = xmp_utimens,
	#endif
	.open = xmp_open,
	.read = xmp_read,
	.write = xmp_write,
	.statfs = xmp_statfs,
	.release = xmp_release,
	.fsync = xmp_fsync,
	#ifdef HAVE_POSIX_FALLOCATE
	.fallocate = xmp_fallocate,
	#endif
	#ifdef HAVE_SETXATTR
	.setxattr = xmp_setxattr,
	.getxattr = xmp_getxattr,
	.listxattr = xmp_listxattr,
	.removexattr = xmp_removexattr,
	#endif
};
int main(int argc, char *argv[]){
	uint8_t key_bytes[] = {0xC5, 0xE6, 0x67, 0xEE, 0x10, 0x97, 0x19, 0x74, 0xDA, 0xC5, 0x52, 0x65, 0x26, 0x01, 0x77, 0x05};
	char *fuse_argv[100];
	int fuse_argc = argc+3;
	int i;
	int res;
	
	int aes_self_test_result = aes_self_test128();
    if(aes_self_test_result){
        printf("aes self test failed with error code: %d\n",aes_self_test_result);
        exit(-1);
    }
	
	urandom = fopen("/dev/urandom", "r");
	
	//secret key
    aes_load_key128(key_bytes);
    //for(i=0;i<sizeof(key_bytes);i++) ((uint8_t*)key)[i] = key_bytes[i]; 
        
    //command line arguments:    
	for(i=0;i<argc;i++) fuse_argv[i]=argv[i];
	fuse_argv[argc+0] = "-o";
	fuse_argv[argc+1] = "auto_unmount";
	fuse_argv[argc+2] = "-f";
	
	base_path="/home/seb/tmp/public/lofe";
    base_path_len = strlen(base_path);
    umask(0);
    res = fuse_main(fuse_argc, fuse_argv, &xmp_oper, NULL);
	if(urandom!=0) fclose(urandom);
	return res;
}

