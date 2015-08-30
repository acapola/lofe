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


typedef struct lofe_header_struct_t {
	uint64_t info;
	uint64_t content_len;
	uint64_t iv[2];
} lofe_header_t;

uint64_t key[2];
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
static void lofe_encrypt(char *buf, size_t size){
    size_t i;
    if(size % BLOCK_SIZE){printf("FATAL_ERROR: encrypt, size not aligned on block size\n");exit(-1);}
    for(i=0;i<size;i++){
        buf[i] = buf[i]+1;
    }
}
static void lofe_decrypt(char *buf, size_t size){
    size_t i;
    if(size % BLOCK_SIZE){printf("FATAL_ERROR: decrypt, size not aligned on block size\n");exit(-1);}
    for(i=0;i<size;i++){
        buf[i] = buf[i]-1;
    }
}

static char *base_path;
static unsigned int base_path_len;

``
proc displayStrVar { name } {
	#return ""
	``printf("`$name`=%s\n",`$name`);``
	return [tgpp::getProcOutput]
}
proc log { printfArgs } {
	``printf(`$printfArgs`);printf("\n");``
	return [tgpp::getProcOutput]
}
proc logStr { str } {
	``printf("`$str`\n");``
	return [tgpp::getProcOutput]
}
``

static void translate_path(const char *path_from_sys, char *actual_path_ptr){
	//`displayStrVar path_from_sys`
	strcpy(actual_path_ptr,base_path);
    strcpy(actual_path_ptr+base_path_len,path_from_sys);
	`displayStrVar "actual_path_ptr"`
}

``
proc fixPath { {name path} } {``
	char actual_`$name`_ptr[1024];
	unsigned int len_`$name` = strlen(`$name`)+base_path_len+1;//+1 for final null char
	if(len_`$name`>sizeof(actual_`$name`_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_`$name`,sizeof(actual_`$name`_ptr));
		exit(-1);
	}
	translate_path(`$name`,actual_`$name`_ptr);
    `$name` = actual_`$name`_ptr;
``	return [tgpp::getProcOutput]
}``

static int write_header(int fd, uint64_t len){
	lofe_header_t header;
	header.info=0;
	header.iv[0] = 0x0123456789ABCDEF;
	header.iv[1] = 0x1122334455667788;
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

static int lofe_read_block(int fd, uint8_t*buf, off_t offset, const lofe_header_t * const header){
	if(offset%BLOCK_SIZE) {
		printf("ERROR: lofe_read_block, unaligned offset: %X\n",offset);
		return -1;
	}
	int res = pread(fd, buf, BLOCK_SIZE, offset);
	if (res == -1){
		return -1;
	}
	if(res!=BLOCK_SIZE){
		printf("ERROR: lofe_read_block, could not read all the requested bytes\n");
		return -1;
	}
	return 0;
}

static int lofe_write_block(int fd, const uint8_t * const buf, off_t offset, const lofe_header_t * const header){
	if(offset%BLOCK_SIZE) {
		printf("ERROR: lofe_write_block, unaligned offset: %X\n",offset);
		return -1;
	}
	int res = pwrite(fd, buf, BLOCK_SIZE, offset);
	if (res == -1){
		return -1;
	}
	if(res!=BLOCK_SIZE){
		printf("ERROR: lofe_write_block, could not write all the requested bytes\n");
		return -1;
	}
	return 0;
}


static int xmp_getattr(const char *path, struct stat *stbuf){
    `logStr xmp_getattr`
	int res;
	`fixPath`
    res = lstat(path, stbuf);
    if (res == -1)
        return -errno;
	stbuf->st_size-=sizeof(lofe_header_t);//return the size of the file without the size of the header
    return 0;
}
static int xmp_access(const char *path, int mask){
	`logStr xmp_access`
	int res;
	`fixPath`
    res = access(path, mask);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
	`logStr xmp_readlink`
	int res;
	`fixPath`
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
	`logStr xmp_readdir`
	DIR *dp;
	struct dirent *de;
	(void) offset;
	(void) fi;
	`fixPath`
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
	`logStr xmp_mknod`
	int res;
	int fd;
	`fixPath`
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
	`logStr xmp_mkdir`
	int res;
	`fixPath`
    res = mkdir(path, mode);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_unlink(const char *path){
	`logStr xmp_unlink`
	int res;
	`fixPath`
    res = unlink(path);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_rmdir(const char *path){
	`logStr xmp_rmdir`
	int res;
	`fixPath`
    res = rmdir(path);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_symlink(const char *from, const char *to){
	`logStr xmp_symlink`
	int res;
	`fixPath from`
    `fixPath to`
    res = symlink(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_rename(const char *from, const char *to){
	`logStr xmp_rename`
	int res;
	`fixPath from`
    `fixPath to`
    res = rename(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_link(const char *from, const char *to){
	`logStr xmp_link`
	int res;
	`fixPath from`
    `fixPath to`
    res = link(from, to);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_chmod(const char *path, mode_t mode){
	`logStr xmp_chmod`
	int res;
	`fixPath`
    res = chmod(path, mode);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_chown(const char *path, uid_t uid, gid_t gid){
	`logStr xmp_chown`
	int res;
	`fixPath`
    res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_truncate(const char *path, off_t size){
	`logStr xmp_truncate`
	int res;
	`fixPath`
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
	`logStr xmp_utimens`
	int res;
	`fixPath`
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
	`fixPath`
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
	`logStr xmp_read`
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
	`fixPath`
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
		uint8_t aligned_buf[BLOCK_SIZE];
	
		//read unaligned data
		//res = pread(fd, aligned_buf, BLOCK_SIZE, read_offset);
		res=lofe_read_block(fd, aligned_buf, read_offset, &header);
		if (res == -1){
			close(fd);
			return -1;
		}
			
		//copy to output buffer
		memcpy(buf,aligned_buf+unaligned_offset,unaligned_size);
		read_offset+=BLOCK_SIZE;
		buf+=unaligned_offset;
	}
	
	//process regular blocks
	for(block=0;block<regular_blocks;block++){
		//res = pread(fd, buf, BLOCK_SIZE, read_offset);
		res=lofe_read_block(fd, buf, read_offset, &header);
		if (res == -1){
			close(fd);
			return -errno;
		}	
		read_offset+=BLOCK_SIZE;
		buf+=BLOCK_SIZE;
	}
	
	//process final block if it is not aligned, otherwise it was treated as a regular block
	if(last_block_unaligned_size){
		uint8_t aligned_buf[BLOCK_SIZE];
		
		//read end of the block from file
		//res = pread(fd, aligned_buf, BLOCK_SIZE, read_offset);
		res=lofe_read_block(fd, aligned_buf, read_offset, &header);
		if (res == -1){
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
	`logStr xmp_write`
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
	`fixPath`
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
		uint8_t aligned_buf[BLOCK_SIZE];
		
        if(0==regular_blocks){//first block = last block
            if(last_block_unaligned_size){
                read_size=BLOCK_SIZE;
                last_block_unaligned_size=0;
            }
        } else {
            regular_blocks--;//remove the first block from the count of regular blocks.
		}
		
		//read beginning of the block from file
		res = pread(fd, aligned_buf, read_size, write_offset);
		if (res == -1){
			close(fd);
			return -errno;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf+unaligned_offset,buf,unaligned_size);
		//write the aligned block to file
		//res = pwrite(fd, aligned_buf, BLOCK_SIZE, write_offset);
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
		//res = pwrite(fd, buf, BLOCK_SIZE, write_offset);
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
		uint8_t aligned_buf[BLOCK_SIZE];
		
		//read end of the block from file
		res = pread(fd, aligned_buf+last_block_unaligned_size, read_size, write_offset);
		if (res == -1){
			close(fd);
			return -errno;
		}
		//copy the unaligned data to aligned_buf to form a full block
		memcpy(aligned_buf,buf,last_block_unaligned_size);
		//write the aligned block to file
		//res = pwrite(fd, aligned_buf, BLOCK_SIZE, write_offset);
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
	`logStr xmp_statfs`
	int res;
	`fixPath`
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
	`logStr xmp_fallocate`
	int fd;
	int res;
	(void) fi;
	if (mode)
		return -EOPNOTSUPP;
	`fixPath`
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
	`logStr xmp_setxattr`
	`fixPath`
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
	`logStr xmp_getxattr`
	`fixPath`
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(
			const char *path, 
			char *list, 
			size_t size){
	`logStr xmp_listxattr`
	`fixPath`
    int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(
			const char *path, 
			const char *name){
	`logStr xmp_removexattr`
	`fixPath`
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
	//secret key
    for(i=0;i<sizeof(key_bytes);i++) ((uint8_t*)key)[i] = key_bytes[i]; 
        
    //command line arguments:    
	for(i=0;i<argc;i++) fuse_argv[i]=argv[i];
	fuse_argv[argc+0] = "-o";
	fuse_argv[argc+1] = "auto_unmount";
	fuse_argv[argc+2] = "-f";
	
	base_path="/home/seb/tmp/public/lofe";
    base_path_len = strlen(base_path);
    umask(0);
    return fuse_main(fuse_argc, fuse_argv, &xmp_oper, NULL);
}

