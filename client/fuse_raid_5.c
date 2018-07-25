#include "fuse_raid_5.h"

#define SET 34
#define GET 43

#define OPEN_FILES_NUM 64
#define FILE_DAMAGED -93
#define UNOPENED -102
#define UNSET_FD -78
#define MAX_SERVERS 16
#define ALL 961

struct storage *global_storage;

FILE *err_file;

int server_fds[MAX_SERVERS];

struct open_file {
	char *path;
	int fds[MAX_SERVERS];
	int n;
};

struct open_file *open_files[OPEN_FILES_NUM];

static void print2(const char *syscall, const char *path)
{
	// err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(path) + strlen(syscall) + 28);
	sprintf(info_msg, "raid_5_%s: Path is %s", syscall, path);
	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
	nrf_print_info(info_msg);
	free(info_msg);
	// fclose(err_file);
}

static void print3(const char *syscall, const char *from, const char *to)
{
	// err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(from) + strlen(to) + strlen(syscall) + 29);
	sprintf(info_msg, "raid_5_%s: From %s to %s", syscall, from, to);
	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
	nrf_print_info(info_msg);
	free(info_msg);
	// fclose(err_file);
}

// static void print2i(const char *syscall, int num)
// {
// 	// err_file = fopen(global_config->err_log_path, "a");
// 	char *info_msg = (char*)malloc(strlen(syscall) + 41);
// 	sprintf(info_msg, "raid_5_%s: Number is %d", syscall, num);
// 	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
// 	nrf_print_value(info_msg);
// 	free(info_msg);
// 	// fclose(err_file);
// }

// static void print4cssi(char raid, const char *syscall, const char *text, int num)
// {
// 	err_file = fopen(global_config->err_log_path, "a");
// 	char *info_msg = (char*)malloc(strlen(syscall) + 41);
// 	sprintf(info_msg, "raid_%c_%s: %s: %d", raid, syscall, text, num);
// 	fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
// 	nrf_print_value(info_msg);
// 	free(info_msg);
// 	fclose(err_file);
// }

static void print6csssi(char raid, const char *syscall, const char *text, int i, const char *text_2, int num)
{
	// err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(syscall) + strlen(text) + strlen(text_2) + 42);
	sprintf(info_msg, "raid_%c_%s: %s %d: %s:%d", raid, syscall, text, i, text_2, num);
	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
	nrf_print_error(info_msg);
	free(info_msg);
	// fclose(err_file);
}

static void print_open_files()
{
	int i = 0;
	int null_count = 0;
	for (; i < OPEN_FILES_NUM; i++)
	{
		if (open_files[i] != NULL)
		{
			if (null_count > 0) printf("%d NULLs\n", null_count);
			null_count = 0;
			printf("path: %s\n", open_files[i]->path);
			int j = 0;
			for(; open_files[i]->fds[j] != UNSET_FD; j++)
				printf("fd_%d: %d\n", j, open_files[i]->fds[j]);
		}
		else null_count++;
	}
	if (null_count > 0) printf("%d NULLs\n", null_count);
}

static void init_open_files()
{
	int i = 0;
	for (; i < OPEN_FILES_NUM; i++)
		open_files[i] = NULL;
}

static struct open_file *find_fd(const char *path)
{
	int i = 0;
	for (; open_files[i] != NULL; i++)
		if (strcmp(open_files[i]->path, path) == 0)
			return open_files[i];
	return NULL;
}

static struct open_file *add_open_file(const char *path, int fds[], int size)
{
	struct open_file *of = malloc(sizeof(struct open_file));
	of->path = strdup(path);

	int i = 0;
	for (; i < size; i++)
		of->fds[i] = fds[i];

	i = 0;
	while (open_files[i] != NULL) i++;
	open_files[i] = of;
	of->n = i;
	return of;
}

static void copy_file(struct open_file *of, int dest, int source)
{
	return;
}

/* 
	Sends to both server buffer with function and its arguments in it.
	Returns 0 if we should try to receive responses from both servers,
	returns 1 if we should listen only to first server,
	returns 2 if we should listen only to second server and
	returns -1 if no information could be sent to either of servers.
*/
static int send_to_server(char buf[], size_t len, int mode)
{
	int statuses[MAX_SERVERS];

	memset(statuses, -1, MAX_SERVERS);

	struct server *server = global_storage->servers;
	if (mode == GET)
	{
		int i = 0;
		while (server != NULL)
		{
			statuses[i] = 0;
			while (write(server_fds[i], buf, len) == -1)
			{
				if ((server_fds[i] = connect_to_server(server)) == -1){
					print6csssi('5', buf+1, "cannot connect to server", i, server->ip, server->port);
					statuses[i] = -1;
					break;
				}
			}
			if (statuses[i] == 0) return i;
			server = server->next_server;
			i++;
		}
		if (server == NULL) 
		{
			nrf_print_error("All servers are dead");
			return -1;
		}
		return 789;
	}
	else if (mode == SET)
	{
		int i = 0;
		while (server != NULL)
		{
			statuses[i] = 0;
			while (write(server_fds[i], buf, len) == -1)
			{
				if ((server_fds[i] = connect_to_server(server)) == -1){
					print6csssi('5', buf+1, "cannot connect to server", i, server->ip, server->port);
					statuses[i] = -1;
					break;
				}
			}
			if (statuses[i] == -1)
			{
				char *error = malloc(100);
				sprintf(error, "Server %d: %s:%d is dead", i, server->ip, server->port);
				nrf_print_error(error);
				free(error);
				return -1;
			}
			server = server->next_server;
			i++;
		}
		return ALL;
	}
	else
	{
		int i = 0;
		while (server != NULL)
		{
			if (i == mode) break;
			server = server->next_server;
			i++;
		}
		if (server == NULL) return -1;

		statuses[i] = 0;
		while (write(server_fds[i], buf, len) == -1)
		{
			if ((server_fds[i] = connect_to_server(server)) == -1){
				print6csssi('5', buf+1, "cannot connect to server", i, server->ip, server->port);
				statuses[i] = -1;
				break;
			}
		}
		if (statuses[i] == -1)
		{
			char *error = malloc(100);
			sprintf(error, "Server %d: %s:%d is dead", i, server->ip, server->port);
			nrf_print_error(error);
			free(error);
			return -1;
		}
		else return i;
	}
}

static int lstat_on_server(const char *path, struct stat *stbuf);
static int access_on_server(const char *path, int mask);
static int mknod_on_server(const char *path, mode_t mode, dev_t rdev);

static int mkdir_on_server(const char *path, mode_t mode);
static DIR *opendir_on_server(const char *path);
static struct dirent *readdir_on_server(DIR *dir);
static int closedir_on_server(DIR *dir);
static int rmdir_on_server(const char *path);

static struct open_file *open_on_server(const char *path, int flags, mode_t mode);
static int pread_on_server(int open_fd, char *read_buf, size_t size, off_t offset, int server);
static int pwrite_on_server(const char *path, struct open_file *of, const char *write_buf, size_t size, off_t offset);
static int truncate_on_server(const char *path, off_t size);
static int close_on_server(int close_fd, int server);
static int rename_on_server(const char *from, const char *to);
static int unlink_on_server(const char *path);


static int lstat_on_server(const char *path, struct stat *stbuf)
{
	char buf[1024];
	sprintf(buf, "5%s", "lstat");
	memcpy(buf + 7, path, strlen(path) + 1);

	int status = send_to_server(buf, strlen(path) + 8, GET);

	if (status == -1) return -1;

	if (read(server_fds[status], buf, sizeof(struct stat) + sizeof(int) + sizeof(int)) == -1) return -1;

	memcpy(stbuf, buf, sizeof(struct stat));

	int rv;
	memcpy(&rv, buf + sizeof(struct stat), sizeof(int));
	memcpy(&errno, buf + sizeof(struct stat) + sizeof(int), sizeof(int));

	return rv;
}

static int access_on_server(const char *path, int mask)
{
	char buf[1024];
	sprintf(buf, "5%s", "access");
	memcpy(buf + 8, path, strlen(path) + 1);
	memcpy(buf + 9 + strlen(path), &mask, sizeof(int));
	int status = send_to_server(buf, strlen(path) + 9 + sizeof(int), GET);

	if (status == -1) return -1;

	if (read(server_fds[status], buf, sizeof(int) + sizeof(int)) == -1) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));

	return rv;
}

static int mknod_on_server(const char *path, mode_t mode, dev_t rdev)
{
	char buf[1024];
	sprintf(buf, "5%s", "mknod");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	memcpy(buf + 8 + strlen(path) + sizeof(mode_t), &rdev, sizeof(dev_t));
	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t) + sizeof(dev_t), SET);

	if (status != ALL) return -1;

	int rv;
	int i = 0;
	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
	{
		status = read(server_fds[i], buf, sizeof(int) * 2);
		if (status < 0) return -1;
		memcpy(&rv, buf, sizeof(int));
		if (rv < 0)
		{
			memcpy(&errno, buf + sizeof(int), sizeof(int));
			return rv;
		}
		i++;
	}

	return rv;
}

static int mkdir_on_server(const char *path, mode_t mode)
{
	char buf[1024];
	sprintf(buf, "5%s", "mkdir");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t), SET);

	if (status != ALL) return -1;

	int rv;
	int i = 0;
	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
	{
		status = read(server_fds[i], buf, sizeof(int) * 2);
		if (status < 0) return -1;
		memcpy(&rv, buf, sizeof(int));
		if (rv < 0)
		{
			memcpy(&errno, buf + sizeof(int), sizeof(int));
			return rv;
		}
		i++;
	}

	return rv;
}

static DIR *opendir_on_server(const char *path)
{
	char buf[1024];
	sprintf(buf, "5%s", "opendir");
	memcpy(buf + 9, path, strlen(path) + 1);
	int status = send_to_server(buf, strlen(path) + 10, GET);

	if (status == -1) return NULL;

	if (read(server_fds[status], buf, sizeof(DIR *) + sizeof(int)) == -1) return NULL;

	DIR *dir;
	memcpy(&dir, buf, sizeof(DIR *));
	memcpy(&errno, buf + sizeof(DIR *), sizeof(int));

	// err_file = fopen(global_config->err_log_path, "a");
	// fprintf(err_file, "opendir_on_server: Dir: %p\n", dir);
	// fclose(err_file);

	return dir;
}

static struct dirent *readdir_on_server(DIR *dir)
{
	// err_file = fopen(global_config->err_log_path, "a");
	// fprintf(err_file, "readdir_on_server: Dir: %p\n", dir);

	char buf[1024];
	sprintf(buf, "5%s", "readdir");
	memcpy(buf + 9, &dir, sizeof(DIR *));
	int status = send_to_server(buf, sizeof(DIR *) + 9, GET);

	if (status == -1) return NULL;

	if (read(server_fds[status], buf, sizeof(struct dirent) + sizeof(int)) == -1) return NULL;

	if (strcmp(buf, "NULL") == 0) 
	{
		memcpy(&errno, buf + 5, sizeof(int));
		return NULL;
	}

	struct dirent *entry = malloc(sizeof(struct dirent));
	memcpy(entry, buf, sizeof(struct dirent));
	memcpy(&errno, buf + sizeof(struct dirent), sizeof(int));

	// fprintf(err_file, "readdir_on_server: Entry { d_ino: %zu, d_type: %d, d_name: %s }\n", entry->d_ino, entry->d_type, entry->d_name);
	// fclose(err_file);

	return entry;
}

static int closedir_on_server(DIR *dir)
{
	// err_file = fopen(global_config->err_log_path, "a");
	// fprintf(err_file, "closedir_on_server: Dir: %p\n", dir);
	// fclose(err_file);

	char buf[1024];
	sprintf(buf, "5%s", "closedir");
	memcpy(buf + 10, &dir, sizeof(DIR *));
	int status = send_to_server(buf, sizeof(DIR *) + 10, GET);

	if (status == -1) return -1;

	if (read(server_fds[status], buf, sizeof(int)) == -1) return -1;

	memcpy(&errno, buf, sizeof(int));

	return 0;
}

static int rmdir_on_server(const char *path)
{
	char buf[1024];
	sprintf(buf, "5%s", "rmdir");
	memcpy(buf + 7, path, strlen(path) + 1);
	int status = send_to_server(buf, strlen(path) + 8, SET);

	if (status != ALL) return -1;

	int rv;
	int i = 0;
	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
	{
		status = read(server_fds[i], buf, sizeof(int) * 2);
		if (status < 0) return -1;
		memcpy(&rv, buf, sizeof(int));
		if (rv < 0)
		{
			memcpy(&errno, buf + sizeof(int), sizeof(int));
			return rv;
		}
		i++;
	}

	return rv;
}

static struct open_file *open_on_server(const char *path, int flags, mode_t mode)
{
	char buf[1024];
	sprintf(buf, "5%s", "open");
	memcpy(buf + 6, path, strlen(path) + 1);
	memcpy(buf + 7 + strlen(path), &flags, sizeof(int));
	memcpy(buf + 7 + strlen(path) + sizeof(int), &mode, sizeof(mode_t));
	int status;
	int rv = 0;

	struct open_file *of;

	// If open is required on both servers
	if (flags & O_CREAT || flags & O_WRONLY)
	{
		int open_fds[MAX_SERVERS];

		status = send_to_server(buf, 7 + strlen(path) + sizeof(int) + sizeof(mode_t), SET);
		if (status != ALL) return NULL;

		int statuses[MAX_SERVERS];
		int need_to_close = 0;

		int i = 0;
		while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
		{
			statuses[i] = read(server_fds[i], buf + sizeof(int) * 2 * i, sizeof(int) * 2);
			if (statuses[i] < 0) need_to_close = 1;
			else memcpy(open_fds + i, buf + sizeof(int) * 2 * i, sizeof(int));
			i++;
		}

		if (need_to_close == 1)
		{
			for (i = 0; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
				if (statuses[i] > -1 && open_fds[i] > -1) close_on_server(open_fds[i], i);
			return NULL;
		}

		for (i = 0; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
			if (open_fds[i] < 0) memcpy(&errno, buf + sizeof(int) * 2 * i + sizeof(int), sizeof(int));

		of = add_open_file(path, open_fds, i);
	}
	else
	{
		status = send_to_server(buf, 7 + strlen(path) + sizeof(int) + sizeof(mode_t), GET);
		if (status == -1) return NULL;
		
		if (read(server_fds[status], buf, sizeof(int) * 2) == -1) return NULL;
		
		memcpy(&rv, buf, sizeof(int));
		if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));

		int open_fds[MAX_SERVERS];
		int i = 0;
		for (; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
		{
			if (i == status) open_fds[i] = rv;
			else open_fds[i] = UNOPENED;
		}

		of = add_open_file(path, open_fds, i);
	}

	return of;
}

static int pread_on_server(int open_fd, char *read_buf, size_t size, off_t offset, int server)
{
	char buf[1024];
	sprintf(buf, "5%s", "pread");
	memcpy(buf + 7, &open_fd, sizeof(int));
	memcpy(buf + 7 + sizeof(int), &size, sizeof(size_t));
	memcpy(buf + 7 + sizeof(int) + sizeof(size_t), &offset, sizeof(off_t));

	if (server == 0) server = GET;
	int status = send_to_server(buf, 7 + sizeof(int) + sizeof(size_t) + sizeof(off_t), server);

	if (status == -1) return -1;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(int) * 2) == -1) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));
	int num = rv;
	while (num > 0)
	{
		read(fd, buf, 1024);
		num -= 1024;
		memcpy(read_buf, buf, 1024);
		read_buf += 1024;
	}

	return rv;
}

static int pwrite_on_server(const char *path, struct open_file *of, const char *write_buf, size_t size, off_t offset)
{
	char buf[1024];
	sprintf(buf, "5%s", "pwrite");
	memcpy(buf + 8, path, strlen(path) + 1);
	memcpy(buf + 9 + strlen(path), &of->fd_1, sizeof(int));
	memcpy(buf + 9 + strlen(path) + sizeof(int), &size, sizeof(size_t));
	memcpy(buf + 9 + strlen(path) + sizeof(int) + sizeof(size_t), &offset, sizeof(off_t));

	int status_1 = send_to_server(buf, 9 + strlen(path) + sizeof(int) + sizeof(size_t) + sizeof(off_t), 1);

	if(status_1 < 0) return -1;

	memcpy(buf + 9 + strlen(path), &of->fd_2, sizeof(int));
	int status_2 = send_to_server(buf, 9 + strlen(path) + sizeof(int) + sizeof(size_t) + sizeof(off_t), 2);

	if(status_2 < 0) return -1;

	int int_size = (int)size;
	int status;

	while (int_size > 0)
	{
		printf("loop start\n");
		int write_size = 1024;
		if (int_size < 1024) write_size = int_size;
		memcpy(buf, write_buf, write_size);
		int_size -= 1024;
		status = send_to_server(buf, write_size, SET);
		if (status != 0) return -1;
		printf("loop end\n");
	}

	status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	status_2 = read(server_fd_2, buf + sizeof(int) * 2, sizeof(int) * 2);

	if (status_1 < 0 || status_2 < 0) return -1;

	int rv_1, rv_2;
	memcpy(&rv_1, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));
	memcpy(&rv_2, buf + sizeof(int) * 2, sizeof(int));
	if (rv_2 < 0)
	{
		memcpy(&errno, buf + sizeof(int) * 3, sizeof(int));
		return rv_2;
	}

	return rv_1;
}

static int truncate_on_server(const char *path, off_t size)
{
	char buf[1024];
	sprintf(buf, "5%s", "truncate");
	memcpy(buf + 10, path, strlen(path) + 1);
	memcpy(buf + 11 + strlen(path), &size, sizeof(off_t));
	int status = send_to_server(buf, strlen(path) + 11 + sizeof(off_t), SET);

	if (status != 0) return -1;

	int status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	int status_2 = read(server_fd_2, buf, sizeof(int) * 2);

	if (status_1 < 0 || status_2 < 0) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));

	return rv;
}

static int close_on_server(int close_fd, int server)
{
	char buf[1024];
	sprintf(buf, "5%s", "close");
	memcpy(buf + 7, &close_fd, sizeof(int));
	int status = send_to_server(buf, sizeof(int) + 7, server);

	if (status == -1) return -1;

	int fd;
	if (status == 2) fd = server_fd_2;
	else fd = server_fd_1;

	if (read(fd, buf, sizeof(int)) == -1) return -1;

	memcpy(&errno, buf, sizeof(int));

	return 0;
}

static int rename_on_server(const char *from, const char *to)
{
	char buf[1024];
	sprintf(buf, "5%s", "rename");
	memcpy(buf + 8, from, strlen(from) + 1);
	memcpy(buf + 9 + strlen(from), to, strlen(to) + 1);
	int status = send_to_server(buf, strlen(from) + strlen(to) + 10, SET);

	if (status != 0) return -1;

	int status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	int status_2 = read(server_fd_2, buf, sizeof(int) * 2);

	if (status_1 < 0 || status_2 < 0) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));

	return rv;
}

static int unlink_on_server(const char *path)
{
	char buf[1024];
	sprintf(buf, "5%s", "unlink");
	memcpy(buf + 8, path, strlen(path) + 1);
	int status = send_to_server(buf, strlen(path) + 9, SET);

	if (status != 0) return -1;

	int status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	int status_2 = read(server_fd_2, buf, sizeof(int) * 2);

	if (status_1 < 0 || status_2 < 0) return -1;

	int rv;
	memcpy(&rv, buf, sizeof(int));
	memcpy(&errno, buf + sizeof(int), sizeof(int));

	return rv;
}

static int raid_5_getattr(const char *path, struct stat *stbuf);
static int raid_5_access(const char *path, int mask);
static int raid_5_mknod(const char *path, mode_t mode, dev_t rdev);

static int raid_5_mkdir(const char *path, mode_t mode);
static int raid_5_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int raid_5_rmdir(const char *path);

static int raid_5_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int raid_5_open(const char *path, struct fuse_file_info *fi);
static int raid_5_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int raid_5_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int raid_5_truncate(const char *path, off_t size);
static int raid_5_release(const char *path, struct fuse_file_info *fi);
static int raid_5_rename(const char *from, const char *to);
static int raid_5_unlink(const char *path);


static int raid_5_getattr(const char *path, struct stat *stbuf)
{
	print2("getattr", path);
	if (lstat_on_server(path, stbuf) == -1) return -errno;
	return 0;
}

static int raid_5_access(const char *path, int mask)
{
	print2("access", path);
	if (access_on_server(path, mask) == -1) return -errno;
	return 0;
}

static int raid_5_mknod(const char *path, mode_t mode, dev_t rdev)
{
	print2("mknod", path);
	if (mknod_on_server(path, mode, rdev) == -1) return -errno;
	return 0;
}

static int raid_5_mkdir(const char *path, mode_t mode)
{
	print2("mkdir", path);
	if (mkdir_on_server(path, mode) == -1) return -errno;
	return 0;
}

static int raid_5_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	print2("readdir", path);

	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	if ((dp = opendir_on_server(path)) == NULL) return -errno;

	while ((de = readdir_on_server(dp)) != NULL)
	{
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0)) break;
		free(de);
	}
	free(de);

	closedir_on_server(dp);
	return 0;
}

static int raid_5_rmdir(const char *path)
{
	print2("rmdir", path);
	if (rmdir_on_server(path) == -1) return -errno;
	return 0;
}

static int raid_5_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	print2("create", path);
	struct open_file *of = open_on_server(path, fi->flags, mode);
	if (of == NULL) return -errno;

	print_open_files();

	if (of->fd_1 >= 0 && of->fd_2 >= 0) return 0;
	if (of->fd_1 == FILE_DAMAGED && of->fd_2 != FILE_DAMAGED) copy_file(of, 1, 2);
	else if (of->fd_1 != FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) copy_file(of, 2, 1);
	else if (of->fd_1 == FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) return -EBADFD;
	else return -errno;
	raid_5_release(path, fi);
	return raid_5_create(path, mode, fi);
}

static int raid_5_open(const char *path, struct fuse_file_info *fi)
{
	print2("open", path);
	struct open_file *of = open_on_server(path, fi->flags, 0000);
	if (of == NULL) return -errno;

	print_open_files();

	if (of->fd_1 >= 0 && of->fd_2 >= 0) return 0;
	if (of->fd_1 >= 0 && of->fd_2 == UNOPENED) return 0;
	if (of->fd_1 == UNOPENED && of->fd_2 >= 0) return 0;
	if ((of->fd_1 == UNOPENED || of->fd_2 == UNOPENED) && (of->fd_1 == FILE_DAMAGED || of->fd_2 == FILE_DAMAGED))
	{
		raid_5_release(path, fi);
		of = open_on_server(path, fi->flags | O_WRONLY, 0000);
	}		
	if (of->fd_1 == FILE_DAMAGED && of->fd_2 != FILE_DAMAGED) copy_file(of, 1, 2);
	else if (of->fd_1 != FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) copy_file(of, 2, 1);
	else if (of->fd_1 == FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) return -EBADFD;
	else return -errno;
	raid_5_release(path, fi);
	return raid_5_open(path, fi);
}

static int raid_5_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	print2("read", path);

	(void) fi;

	// int fd;
	int res;
	// fd = open_on_server(path, O_RDONLY, 0000);
	// if (fd == -1) return -errno;

	struct open_file *of = find_fd(path);
	if (of == NULL) return -1;

	print_open_files();

	int open_fd;
	int server;
	if (of->fd_1 >= 0)
	{
		open_fd = of->fd_1;
		server = 1;
	}
	else
	{ 
		open_fd = of->fd_2;
		server = 2;
	}

	res = pread_on_server(open_fd, buf, size, offset, server);
	if (res == -1) res = -errno;

	// close_on_server(fd);
	return res;
}

static int raid_5_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	print2("write", path);

	int res;

	struct open_file *of = find_fd(path);
	if (of == NULL) return -1;

	print_open_files();

	// fd = open_on_server(path, O_WRONLY, 0000);
	// if (fd == -1) return -errno;

	res = pwrite_on_server(path, of, buf, size, offset);
	if (res == -1) res = -errno;

	// close_on_server(fd);
	return res;
}

static int raid_5_truncate(const char *path, off_t size)
{
	print2("truncate", path);
	if (truncate_on_server(path, size) == -1) return -errno;
	return 0;
}

static int raid_5_release(const char *path, struct fuse_file_info *fi)
{
	print2("release", path);

	(void) fi;

	struct open_file *of = find_fd(path);
	if (of == NULL) return -1;
	
	int res_1 = 0;
	int res_2 = 0;
	if (of->fd_1 != UNOPENED) res_1 = close_on_server(of->fd_1, 1);
	if (of->fd_2 != UNOPENED) res_2 = close_on_server(of->fd_2, 2);
	if (res_1 == -1 || res_2 == -1) return -errno;

	free(of->path);
	open_files[of->n] = NULL;

	print_open_files();

	return 0;
}

static int raid_5_rename(const char *from, const char *to)
{
	print3("rename", from, to);
	if (rename_on_server(from, to) == -1) return -errno;
	return 0;
}

static int raid_5_unlink(const char *path)
{
	print2("unlink", path);
	if (unlink_on_server(path) == -1) return -errno;
	return 0;
}



static int raid_5_symlink(const char *from, const char *to)
{
	// print3("symlink", from, to);

	// int res;

	// res = symlink(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_5_link(const char *from, const char *to)
{
	// print3("link", from, to);

	// int res;

	// res = link(from, to);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_5_chmod(const char *path, mode_t mode)
{
	print2("chmod", path);

	// int res;

	// res = chmod(path, mode);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_5_chown(const char *path, uid_t uid, gid_t gid)
{
	print2("chown", path);

	// int res;

	// res = lchown(path, uid, gid);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_5_utimens(const char *path, const struct timespec ts[2])
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

static int raid_5_statfs(const char *path, struct statvfs *stbuf)
{
	print2("statfs", path);

	// int res;

	// res = statvfs(path, stbuf);
	// if (res == -1)
	// 	return -errno;

	return 0;
}

static int raid_5_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	// print2("fsync", path);

	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

static int
raid_5_lock(const char *path, struct fuse_file_info *fi, 
	 int cmd, struct flock *lock)
{
	// print2("lock", path);

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

static int raid_5_readlink(const char *path, char *buf, size_t size)
{
	// print2("readlink", path);

	// int res;

	// res = readlink(path, buf, size - 1);
	// if (res == -1)
	// 	return -errno;

	// buf[res] = '\0';
	return 0;
}

static struct fuse_operations raid_5_oper = {
	.getattr	= raid_5_getattr,
	.access		= raid_5_access,
	.readlink	= raid_5_readlink,
	.readdir	= raid_5_readdir,
	.mknod		= raid_5_mknod,
	.create     = raid_5_create,
	.mkdir		= raid_5_mkdir,
	.symlink	= raid_5_symlink,
	.unlink		= raid_5_unlink,
	.rmdir		= raid_5_rmdir,
	.rename		= raid_5_rename,
	.link		= raid_5_link,
	.chmod		= raid_5_chmod,
	.chown		= raid_5_chown,
	.truncate	= raid_5_truncate,
	.utimens	= raid_5_utimens,
	.open		= raid_5_open,
	.read		= raid_5_read,
	.write		= raid_5_write,
	.statfs		= raid_5_statfs,
	.release	= raid_5_release,
	.fsync		= raid_5_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= raid_5_setxattr,
	.getxattr	= raid_5_getxattr,
	.listxattr	= raid_5_listxattr,
	.removexattr	= raid_5_removexattr,
#endif
	.lock		= raid_5_lock, 
};

int mount_raid_5(int argc, char *argv[], struct storage *storage)
{
	// FILE *f = fopen("/home/babdus/Documents/OS/nrf.log", "w");
	// fclose(f);
	global_storage = storage;

	// err_file = fopen(global_config->err_log_path, "a");

	init_open_files();

	struct server *server = global_storage->servers;
	int i = 0;
	while (server != NULL)
	{
		server_fds[i++] = connect_to_server(server);
		server = server->next_server;
	}
	for (; i < MAX_SERVERS; i++) server_fds[i] = UNSET_FD;	

	// char *info_msg = (char*)malloc(strlen(argv[2]) + 32);
	// sprintf(info_msg, "Mounting directory '%s'", argv[2]);
	// nrf_print_info(info_msg);
	// free(info_msg);

	umask(0);
	return fuse_main(argc, argv, &raid_5_oper, NULL);
}
