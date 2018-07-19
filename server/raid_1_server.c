#include "raid_1_server.h"

int raid_1_getattr(const char *path, struct stat *stbuf)
{
	char *info_msg = (char*)malloc(strlen(path) + 25);
	sprintf(info_msg, "raid_1_getattr: Path is %s", path);
	nrf_print_info(info_msg);
	free(info_msg);

	nrf_print_struct(stbuf, "stat", 1, 1);
	
	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

int raid_1_access(const char *path, int mask)
{
	char *info_msg = (char*)malloc(strlen(path) + 24);
	sprintf(info_msg, "raid_1_access: Path is %s", path);
	nrf_print_info(info_msg);
	free(info_msg);

	// int res;

	// res = access(path, mask);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_readlink(const char *path, char *buf, size_t size)
{

	// int res;

	// res = readlink(path, buf, size - 1);
	// if (res == -1)
	// 	return -errno;

	// buf[res] = '\0';
	return 0;
}

int raid_1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	// FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	// fprintf(f, "raid_1_readdir: path: %s\n", path);
	// printf("raid_1_readdir: path: %s\n", path);
	// fclose(f);

	// DIR *dp;
	// struct dirent *de;

	// (void) offset;
	// (void) fi;

	// dp = opendir(path);
	// if (dp == NULL)
	// 	return -errno;

	// while ((de = readdir(dp)) != NULL) {
	// 	struct stat st;
	// 	memset(&st, 0, sizeof(st));
	// 	st.st_ino = de->d_ino;
	// 	st.st_mode = de->d_type << 12;
	// 	if (filler(buf, de->d_name, &st, 0))
	// 		break;
	// }

	// closedir(dp);
	return 0;
}

int raid_1_mknod(const char *path, mode_t mode, dev_t rdev)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_mknod: path: %s\n", path);
	printf("raid_1_mknod: path: %s\n", path);
	fclose(f);

	// int res;

	// /* On Linux this could just be 'mknod(path, mode, rdev)' but this
	//    is more portable */
	// if (S_ISREG(mode)) {
	// 	res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
	// 	if (res >= 0)
	// 		res = close(res);
	// } else if (S_ISFIFO(mode))
	// 	res = mkfifo(path, mode);
	// else
	// 	res = mknod(path, mode, rdev);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_mkdir(const char *path, mode_t mode)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_mkdir: path: %s\n", path);
	printf("raid_1_mkdir: path: %s\n", path);
	fclose(f);

	// int res;

	// res = mkdir(path, mode);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_unlink(const char *path)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_unlink: path: %s\n", path);
	printf("raid_1_unlink: path: %s\n", path);
	fclose(f);

	// int res;

	// res = unlink(path);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_rmdir(const char *path)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_rmdir: path: %s\n", path);
	printf("raid_1_rmdir: path: %s\n", path);
	fclose(f);

	// int res;

	// res = rmdir(path);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_symlink(const char *from, const char *to)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_symlink: path: %s\n", from);
	printf("raid_1_symlink: path: %s\n", from);
	fclose(f);

	// int res;

	// res = symlink(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_rename(const char *from, const char *to)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_rename: path: %s\n", from);
	printf("raid_1_rename: path: %s\n", from);
	fclose(f);

	// int res;

	// res = rename(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_link(const char *from, const char *to)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_link: path: %s\n", from);
	printf("raid_1_link: path: %s\n", from);
	fclose(f);

	// int res;

	// res = link(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_chmod(const char *path, mode_t mode)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_chmod: path: %s\n", path);
	printf("raid_1_chmod: path: %s\n", path);
	fclose(f);

	// int res;

	// res = chmod(path, mode);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_chown(const char *path, uid_t uid, gid_t gid)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_chown: path: %s\n", path);
	printf("raid_1_chown: path: %s\n", path);
	fclose(f);

	// int res;

	// res = lchown(path, uid, gid);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_truncate(const char *path, off_t size)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_truncate: path: %s\n", path);
	printf("raid_1_truncate: path: %s\n", path);
	fclose(f);

	// int res;

	// res = truncate(path, size);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_utimens(const char *path, const struct timespec ts[2])
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_utimens: path: %s\n", path);
	printf("raid_1_utimens: path: %s\n", path);
	fclose(f);

	// int res;
	// struct timeval tv[2];

	// tv[0].tv_sec = ts[0].tv_sec;
	// tv[0].tv_usec = ts[0].tv_nsec / 1000;
	// tv[1].tv_sec = ts[1].tv_sec;
	// tv[1].tv_usec = ts[1].tv_nsec / 1000;

	// res = utimes(path, tv);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_open(const char *path, struct fuse_file_info *fi)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_open: path: %s\n", path);
	printf("raid_1_open: path: %s\n", path);
	fclose(f);

	// int res;

	// res = open(path, fi->flags);
	// if (res == -1)
	// 	return -errno;

	// close(res);
	return 0;
}

int raid_1_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_read: path: %s\n", path);
	printf("raid_1_read: path: %s\n", path);
	fclose(f);

	// int fd;
	// int res;

	// (void) fi;
	// fd = open(path, O_RDONLY);
	// if (fd == -1)
	// 	return -errno;

	// res = pread(fd, buf, size, offset);
	// if (res == -1)
	// 	res = -errno;

	// close(fd);
	// return res;
	return 0; // Look up
}

int raid_1_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_write: path: %s\n", path);
	printf("raid_1_write: path: %s\n", path);
	fclose(f);

	// int fd;
	// int res;

	// (void) fi;
	// fd = open(path, O_WRONLY);
	// if (fd == -1)
	// 	return -errno;

	// res = pwrite(fd, buf, size, offset);
	// if (res == -1)
	// 	res = -errno;

	// close(fd);
	// return res;
	return 0; // Look up
}

int raid_1_statfs(const char *path, struct statvfs *stbuf)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_statfs: path: %s\n", path);
	printf("raid_1_statfs: path: %s\n", path);
	fclose(f);

	// int res;

	// res = statvfs(path, stbuf);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

int raid_1_release(const char *path, struct fuse_file_info *fi)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_release: path: %s\n", path);
	printf("raid_1_release: path: %s\n", path);
	fclose(f);

	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

int raid_1_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_fsync: path: %s\n", path);
	printf("raid_1_fsync: path: %s\n", path);
	fclose(f);

	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

int
raid_1_lock(const char *path, struct fuse_file_info *fi, 
	 int cmd, struct flock *lock)
{
	FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "a");
	fprintf(f, "raid_1_lock: path: %s\n", path);
	printf("raid_1_lock: path: %s\n", path);
	fclose(f);

	// int fd;

	// (void)fi;
	
	// fd = open(path, O_WRONLY);
	// if (fd == -1)
	// 	return -errno;

	// if (cmd != F_GETLK && cmd != F_SETLK && cmd != F_SETLKW)
	// 	return -EINVAL;

	// if (fcntl(fd, cmd, lock) == -1)
	// 	return -errno;

	return 0;
}