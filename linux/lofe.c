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

static char *base_path;
static unsigned int base_path_len;


static void translate_path(const char *path_from_sys, char *actual_path_ptr){
		printf("path_from_sys=%s\n",path_from_sys);   

	strcpy(actual_path_ptr,base_path);
    strcpy(actual_path_ptr+base_path_len,path_from_sys);
		printf("actual_path_ptr=%s\n",actual_path_ptr);   

}


static int xmp_getattr(const char *path, struct stat *stbuf){
    int res;
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
    return 0;
}
static int xmp_access(const char *path, int mask){
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
static int xmp_mknod(const char *path, mode_t mode, dev_t rdev){
	int res;
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
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;
	return 0;
}
static int xmp_mkdir(const char *path, mode_t mode){
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
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = truncate(path, size);
	if (res == -1)
		return -errno;
	return 0;
}
#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2]){
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
	int res;
		char actual_path_ptr[1024];
	unsigned int len_path = strlen(path)+base_path_len+1;//+1 for final null char
	if(len_path>sizeof(actual_path_ptr)){
		printf("FATAL ERROR: path too long (%u needed, %lu is the maximum)",len_path,sizeof(actual_path_ptr));
		exit(-1);
	}
	translate_path(path,actual_path_ptr);
    path = actual_path_ptr;

    res = open(path, fi->flags);
	if (res == -1)
		return -errno;
	close(res);
	return 0;
}
static int xmp_read(
			const char *path, 
			char *buf, 
			size_t size, 
			off_t offset,
			struct fuse_file_info *fi){
	int fd;
	int res;
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
	res = pread(fd, buf, size, offset);
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
	int fd;
	int res;
	(void) fi;
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
	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;
	close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf){
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

