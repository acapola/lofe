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
	size+=sizeof(lofe_header_t);//keep the header, truncate only the content to the requested size.
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
	/*//determine if the file already exist
	int dont_exist = access (path, F_OK);
	if(dont_exist) printf("file does not exist\n");
	else printf("file exist\n");*/
    //open the file in the underlying filesystem
	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;
	/*//if we created the file, write the header
	if(dont_exist || (fi->flags & O_CREAT)){
		printf("O_CREATE flag: creating header...");
		lofe_header_t header;
		header.info=0;
		header.iv[0] = 0x0123456789ABCDEF;
		header.iv[1] = 0x1122334455667788;
		header.content_len=0;
		res = pwrite(fd, &header, sizeof(header), 0);
		if (res == -1){
			printf("failure: %d\n",errno);
			res = -errno;
		}
		printf("success\n");
	} else {
		printf("header expected to be present already\n");
	}*/
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
	(void) fi;
	`fixPath`
    fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;
	res = pread(fd, buf, size, offset+sizeof(lofe_header_t));
	if (res == -1)
		res = -errno;
	close(fd);
	return res;
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
	(void) fi;
	`fixPath`
    fd = open(path, O_WRONLY);
	if (fd == -1)
		return -errno;
	res = pwrite(fd, buf, size, offset+sizeof(lofe_header_t));
	if (res == -1)
		res = -errno;
	close(fd);
	return res;
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
	char *fuse_argv[100];
	int fuse_argc = argc+3;
	int i;
	for(i=0;i<argc;i++) fuse_argv[i]=argv[i];
	fuse_argv[argc+0] = "-o";
	fuse_argv[argc+1] = "auto_unmount";
	fuse_argv[argc+2] = "-f";
	
	base_path="/home/seb/tmp/public/lofe";
    base_path_len = strlen(base_path);
    umask(0);
    return fuse_main(fuse_argc, fuse_argv, &xmp_oper, NULL);
}

