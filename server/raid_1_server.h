#ifndef RAID_1_SERVER
#define RAID_1_SERVER

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>

#include "../nrf_print.h"

int raid_1_getattr(const char *path, struct stat *stbuf);

int raid_1_access(const char *path, int mask);

int raid_1_readlink(const char *path, char *buf, size_t size);

int raid_1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi);

int raid_1_mknod(const char *path, mode_t mode, dev_t rdev);

int raid_1_mkdir(const char *path, mode_t mode);

int raid_1_unlink(const char *path);

int raid_1_rmdir(const char *path);

int raid_1_symlink(const char *from, const char *to);

int raid_1_rename(const char *from, const char *to);

int raid_1_link(const char *from, const char *to);

int raid_1_chmod(const char *path, mode_t mode);

int raid_1_chown(const char *path, uid_t uid, gid_t gid);

int raid_1_truncate(const char *path, off_t size);

int raid_1_utimens(const char *path, const struct timespec ts[2]);

int raid_1_open(const char *path, struct fuse_file_info *fi);

int raid_1_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi);

int raid_1_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi);

int raid_1_statfs(const char *path, struct statvfs *stbuf);

int raid_1_release(const char *path, struct fuse_file_info *fi);

int raid_1_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi);

int
raid_1_lock(const char *path, struct fuse_file_info *fi, 
	 int cmd, struct flock *lock);

#endif