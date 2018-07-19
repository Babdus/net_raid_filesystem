#include "fuse_raid_1.h"

#define SET 34
#define GET 43

struct storage *global_storage;

FILE *err_file;

int server_fd_1 = -1;
int server_fd_2 = -1;


/* 
	Sends to both server buffer with function and its arguments in it.
	Returns 0 if we should try to receive responses from both servers,
	returns 1 if we should listen only to first server,
	returns 2 if we should listen only to second server and
	returns -1 if no information could be sent to either of servers.
*/

static void print2(const char *syscall, const char *path)
{
	err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(path) + strlen(syscall) + 28);
	sprintf(info_msg, "raid_1_%s: Path is %s", syscall, path);
	fprintf(err_file, "%s", info_msg);
	nrf_print_info(info_msg);
	free(info_msg);
	fclose(err_file);
}

static void print3(const char *syscall, const char *from, const char *to)
{
	err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(from) + strlen(to) + strlen(syscall) + 29);
	sprintf(info_msg, "raid_1_%s: From %s to %s", syscall, from, to);
	fprintf(err_file, "%s", info_msg);
	nrf_print_info(info_msg);
	free(info_msg);
	fclose(err_file);
}

static int send_to_server(char buf[], size_t len, int mode)
{
	int status_1 = 0;
	int status_2 = 0;

	while (write(server_fd_1, buf, len) == -1)
	{
		if ((server_fd_1 = connect_to_server(global_storage->servers)) == -1){
			fprintf(err_file, "raid_1: cannot connect to server 1: %s:%d\n", global_storage->servers->ip, global_storage->servers->port);
			status_1 = -1;
			break;
		}
	}

	fprintf(err_file, "raid_1: sent to server 1 status: %d\n", status_1);

	if (mode == GET && status_1 == 0)
		return 1;

	while (write(server_fd_2, buf, len) == -1)
	{
		if ((server_fd_2 = connect_to_server(global_storage->servers->next_server)) == -1){
			fprintf(err_file, "raid_1: cannot connect to server 2: %s:%d\n", global_storage->servers->next_server->ip, global_storage->servers->next_server->port);			
			status_2 = -1;
			break;
		}
	}

	fprintf(err_file, "raid_1: sent to server 2 status: %d\n", status_2);

	if (status_2 == 0)
	{
		if (status_1 == 0)
			return 0;
		else
			return 2;
	}
	else
		return -1;
}

static int lstat_on_server(const char *path, struct stat *stbuf)
{
	char buf[1024];
	sprintf(buf, "1%s", "lstat");
	memcpy(buf + 7, path, strlen(path) + 1);
	int status = send_to_server(buf, strlen(path) + 8, GET);

	if (status == -1) return -1;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(struct stat) + sizeof(int)) == -1) return -1;

	memcpy(stbuf, buf, sizeof(struct stat));

	int rv;
	memcpy(&rv, buf + sizeof(struct stat), sizeof(int));

	err_file = fopen(global_config->err_log_path, "a");
	fprintf(err_file, "lstat_on_server: Stat->inode: %zu\n", stbuf->st_ino);
	fclose(err_file);

	return rv;
}

static int raid_1_getattr(const char *path, struct stat *stbuf)
{
	print2("getattr", path);
	// fprintf(err_file, "[%s] raid_1_getattr: struct stat st_ino: %zu\n", asctime(timeinfo), (size_t)(stbuf->st_ino));

	int res;

	res = lstat_on_server(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int access_on_server(const char *path, int mask)
{
	char buf[1024];
	sprintf(buf, "1%s", "access");
	memcpy(buf + 8, path, strlen(path) + 1);
	memcpy(buf + 9 + strlen(path), &mask, sizeof(int));
	int status = send_to_server(buf, strlen(path) + 9 + sizeof(int), GET);

	if (status == -1) return -1;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(int)) == -1) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));

	return rv;
}

static int raid_1_access(const char *path, int mask)
{
	print2("access", path);

	int res;

	res = access_on_server(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int raid_1_readlink(const char *path, char *buf, size_t size)
{
	print2("readlink", path);

	// int res;

	// res = readlink(path, buf, size - 1);
	// if (res == -1)
	// 	return -errno;

	// buf[res] = '\0';
	return 0;
}

static DIR *opendir_on_server(const char *path)
{
	char buf[1024];
	sprintf(buf, "1%s", "opendir");
	memcpy(buf + 9, path, strlen(path) + 1);
	int status = send_to_server(buf, strlen(path) + 10, GET);

	if (status == -1) return NULL;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(DIR *)) == -1) return NULL;

	DIR *dir;
	memcpy(&dir, buf, sizeof(DIR *));

	err_file = fopen(global_config->err_log_path, "a");
	fprintf(err_file, "opendir_on_server: Dir: %p\n", dir);
	fclose(err_file);

	return dir;
}

static struct dirent *readdir_on_server(DIR *dir)
{
	err_file = fopen(global_config->err_log_path, "a");
	fprintf(err_file, "readdir_on_server: Dir: %p\n", dir);

	char buf[1024];
	sprintf(buf, "1%s", "readdir");
	memcpy(buf + 9, &dir, sizeof(DIR *));
	int status = send_to_server(buf, sizeof(DIR *) + 9, GET);

	if (status == -1) return NULL;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(struct dirent)) == -1) return NULL;

	if (strcmp(buf, "NULL") == 0) return NULL;

	struct dirent *entry = malloc(sizeof(struct dirent));
	memcpy(entry, buf, sizeof(struct dirent));

	fprintf(err_file, "readdir_on_server: Entry { d_ino: %zu, d_type: %d, d_name: %s }\n", entry->d_ino, entry->d_type, entry->d_name);
	fclose(err_file);

	return entry;
}

int closedir_on_server(DIR *dir)
{
	err_file = fopen(global_config->err_log_path, "a");
	fprintf(err_file, "closedir_on_server: Dir: %p\n", dir);

	char buf[1024];
	sprintf(buf, "1%s", "closedir");
	memcpy(buf + 10, &dir, sizeof(DIR *));
	return send_to_server(buf, sizeof(DIR *) + 10, GET);
}

static int raid_1_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	print2("readdir", path);

	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir_on_server(path);
	if (dp == NULL) return -errno;

	while ((de = readdir_on_server(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
		free(de);
	}
	free(de);

	closedir_on_server(dp);
	return 0;
}

static int mknod_on_server(const char *path, mode_t mode, dev_t rdev)
{
	char buf[1024];
	sprintf(buf, "1%s", "mknod");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	memcpy(buf + 8 + strlen(path) + sizeof(mode_t), &rdev, sizeof(dev_t));
	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t) + sizeof(dev_t), SET);

	if (status != 0) return -1;

	if (read(server_fd_1, buf, sizeof(int)) == -1 || read(server_fd_2, buf, sizeof(int)) == -1) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));

	return rv;
}

static int raid_1_mknod(const char *path, mode_t mode, dev_t rdev)
{
	print2("mknod", path);

	int res;

	res = mknod_on_server(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int raid_1_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	print2("create", path);

	int res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;
	
	return 0;
}

static int mkdir_on_server(const char *path, mode_t mode)
{
	char buf[1024];
	sprintf(buf, "1%s", "mkdir");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t), SET);

	if (status != 0) return -1;

	if (read(server_fd_1, buf, sizeof(int)) == -1 || read(server_fd_2, buf, sizeof(int)) == -1) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));

	return rv;
}

static int raid_1_mkdir(const char *path, mode_t mode)
{
	print2("mkdir", path);

	int res;

	res = mkdir_on_server(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

// static int raid_1_opendir(const char *path)
// {
// 	print2("opendir", path);

// 	DIR *dp = opendir_on_server(path);
// 	if (dp == NULL) return -errno;

// 	return 0;
// }

static int raid_1_unlink(const char *path)
{
	print2("unlink", path);

	// int res;

	// res = unlink(path);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_rmdir(const char *path)
{
	print2("rmdir", path);

	// int res;

	// res = rmdir(path);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_symlink(const char *from, const char *to)
{
	print3("symlink", from, to);

	// int res;

	// res = symlink(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_rename(const char *from, const char *to)
{
	print3("rename", from, to);

	// int res;

	// res = rename(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_link(const char *from, const char *to)
{
	print3("link", from, to);

	// int res;

	// res = link(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_chmod(const char *path, mode_t mode)
{
	print2("chmod", path);

	// int res;

	// res = chmod(path, mode);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_chown(const char *path, uid_t uid, gid_t gid)
{
	print2("chown", path);

	// int res;

	// res = lchown(path, uid, gid);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_truncate(const char *path, off_t size)
{
	print2("truncate", path);

	// int res;

	// res = truncate(path, size);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_utimens(const char *path, const struct timespec ts[2])
{
	print2("utimens", path);

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

static int raid_1_open(const char *path, struct fuse_file_info *fi)
{
	print2("open", path);

	// int res;

	// res = open(path, fi->flags);
	// if (res == -1)
	// 	return -errno;

	// close(res);
	return 0;
}

static int raid_1_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	print2("read", path);

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

static int raid_1_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	print2("write", path);

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

static int raid_1_statfs(const char *path, struct statvfs *stbuf)
{
	print2("statfs", path);

	// int res;

	// res = statvfs(path, stbuf);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_1_release(const char *path, struct fuse_file_info *fi)
{
	print2("release", path);

	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) fi;
	return 0;
}

static int raid_1_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	print2("fsync", path);

	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

static int
raid_1_lock(const char *path, struct fuse_file_info *fi, 
	 int cmd, struct flock *lock)
{
	print2("lock", path);

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

static struct fuse_operations raid_1_oper = {
	.getattr	= raid_1_getattr,
	.access		= raid_1_access,
	.readlink	= raid_1_readlink,
	.readdir	= raid_1_readdir,
	.mknod		= raid_1_mknod,
	.create     = raid_1_create,
	.mkdir		= raid_1_mkdir,
	.symlink	= raid_1_symlink,
	.unlink		= raid_1_unlink,
	.rmdir		= raid_1_rmdir,
	.rename		= raid_1_rename,
	.link		= raid_1_link,
	.chmod		= raid_1_chmod,
	.chown		= raid_1_chown,
	.truncate	= raid_1_truncate,
	.utimens	= raid_1_utimens,
	.open		= raid_1_open,
	.read		= raid_1_read,
	.write		= raid_1_write,
	.statfs		= raid_1_statfs,
	.release	= raid_1_release,
	.fsync		= raid_1_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= raid_1_setxattr,
	.getxattr	= raid_1_getxattr,
	.listxattr	= raid_1_listxattr,
	.removexattr	= raid_1_removexattr,
#endif
	.lock		= raid_1_lock, 
};

int mount_raid_1(int argc, char *argv[], struct storage *storage)
{
	// FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "w");
	// fclose(f);
	global_storage = storage;

	err_file = fopen(global_config->err_log_path, "a");

	server_fd_1 = connect_to_server(global_storage->servers);
	server_fd_2 = connect_to_server(global_storage->servers->next_server);

	char *info_msg = (char*)malloc(strlen(argv[2]) + 32);
	sprintf(info_msg, "Mounting directory '%s'", argv[2]);
	nrf_print_info(info_msg);
	free(info_msg);

	umask(0);
	return fuse_main(argc, argv, &raid_1_oper, NULL);
}
