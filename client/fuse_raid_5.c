#include "fuse_raid_5.h"

#define BUFFER_SIZE 1024

#define SET 34
#define GET 43

#define OPEN_FILES_NUM 64
#define FILE_DAMAGED -93
#define UNOPENED -102
#define UNSET_FD -78
#define MAX_SERVERS 16
#define ALL 961

#define SERVERS_DOWN -5622

struct storage *global_storage;

FILE *err_file;

int server_fds[MAX_SERVERS];
struct server *servers[MAX_SERVERS];
int servers_count;

struct open_file {
	int fds[MAX_SERVERS];
	int id;
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

static void print2i(const char *syscall, int num)
{
	// err_file = fopen(global_config->err_log_path, "a");
	char *info_msg = (char*)malloc(strlen(syscall) + 41);
	sprintf(info_msg, "raid_5_%s: Number is %d", syscall, num);
	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
	nrf_print_value(info_msg);
	free(info_msg);
	// fclose(err_file);
}

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

// static void print6csssi(char raid, const char *syscall, const char *text, int i, const char *text_2, int num)
// {
// 	// err_file = fopen(global_config->err_log_path, "a");
// 	char *info_msg = (char*)malloc(strlen(syscall) + strlen(text) + strlen(text_2) + 42);
// 	sprintf(info_msg, "raid_%c_%s: %s %d: %s:%d", raid, syscall, text, i, text_2, num);
// 	// fprintf(err_file, "[%s] %s\n", cur_time(), info_msg);
// 	nrf_print_error(info_msg);
// 	free(info_msg);
// 	// fclose(err_file);
// }

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
			printf("id: %d\n", i);
			int j = 0;
			for(; j < servers_count; j++)
				printf("fd_%d: %d\n", j, open_files[i]->fds[j]);
		}
		else null_count++;
	}
	if (null_count > 0) printf("%d NULLs\n", null_count);
}

static void init_open_files()
{
	int i = 0;
	for (; i < OPEN_FILES_NUM; i++) open_files[i] = NULL;
}

static struct open_file *init_open_file()
{
	struct open_file *of = malloc(sizeof(struct open_file));
	int i = 0;
	for (; i < servers_count; i++) of->fds[i] = UNOPENED;
	for (; i < MAX_SERVERS; i++) of->fds[i] = UNSET_FD;
	of->id = -1;
	return of;
}

static int add_fd(struct open_file *of, int fd, int server_num)
{
	if (of->id == -1)
	{
		int i = 0;
		while (open_files[i] != NULL) i++;
		open_files[i] = of;
		of->id = i;
	}
	of->fds[server_num] = fd;
	printf("of: id: %d\n", of->id);
	int j = 0;
	for(; j < servers_count; j++)
		printf("fd_%d: %d\n", j, of->fds[j]);
	return of->id;
}

static void remove_fd(struct open_file *of, int server_num)
{
	of->fds[server_num] = UNOPENED;
	int res = -1;
	int i = 0;
	for (; i < MAX_SERVERS; i++)
		if (of->fds[i] != UNSET_FD && of->fds[i] != UNOPENED) res = 0;

	printf("\033[33mres in remove fd: %d\033[0m\n", res);

	if (res == -1)
	{
		int id = of->id;
		printf("\033[30mbefore free\033[0m\n");
		free(open_files[id]);
		printf("\033[30mafter free\033[0m\n");
		open_files[id] = NULL;
		printf("\033[30mafter null\033[0m\n");
	}
}

// static void copy_file(struct open_file *of, int dest, int source)
// {
// 	return;
// }

static void hotswap(int server_num)
{	
	char *error = malloc(48);
	sprintf(error, "HOTSWAP %d", server_num);
	nrf_print_error(error);
	free(error);
}

static int receive_num(const char *syscall)
{
	if (strcmp(syscall, "lstat") == 0) return sizeof(struct stat) + sizeof(int) * 2;
	if (strcmp(syscall, "opendir") == 0) return sizeof(DIR *) + sizeof(int) * 2;
	if (strcmp(syscall, "readdir") == 0) return sizeof(struct dirent) + sizeof(int) * 2;
	return sizeof(int) * 2;
}

static int pack_mkdir(char *buf, const char *path, mode_t mode)
{
	sprintf(buf, "5%s", "mkdir");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	return strlen(path) + 8 + sizeof(mode_t);
}

static int pack_mknod(char *buf, const char *path, mode_t mode, dev_t rdev)
{
	sprintf(buf, "5%s", "mknod");
	memcpy(buf + 7, path, strlen(path) + 1);
	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
	memcpy(buf + 8 + strlen(path) + sizeof(mode_t), &rdev, sizeof(dev_t));
	return strlen(path) + 8 + sizeof(mode_t) + sizeof(dev_t);
}

static int pack_rmdir(char *buf, const char *path)
{
	sprintf(buf, "5%s", "rmdir");
	memcpy(buf + 7, path, strlen(path) + 1);
	return strlen(path) + 8;
}

static int pack_open(char *buf, const char *path, int flags, mode_t mode)
{
	sprintf(buf, "5%s", "open");
	memcpy(buf + 6, path, strlen(path) + 1);
	memcpy(buf + 7 + strlen(path), &flags, sizeof(int));
	memcpy(buf + 7 + strlen(path) + sizeof(int), &mode, sizeof(mode_t));
	return 7 + strlen(path) + sizeof(int) + sizeof(mode_t);
}

static int pack_rename(char *buf, const char *from, const char *to)
{
	sprintf(buf, "5%s", "rename");
	memcpy(buf + 8, from, strlen(from) + 1);
	memcpy(buf + 9 + strlen(from), to, strlen(to) + 1);
	printf("hjk %d\n", (int)(10 + strlen(from) + strlen(to)));
	return 10 + strlen(from) + strlen(to);
}

static int pack_unlink(char *buf, const char *path)
{
	sprintf(buf, "5%s", "unlink");
	memcpy(buf + 8, path, strlen(path) + 1);
	return strlen(path) + 9;
}

static int pack_utimes(char *buf, const char *path, struct timeval *tv)
{
	sprintf(buf, "5%s", "utimes");
	memcpy(buf + 8, path, strlen(path) + 1);
	memcpy(buf + 9 + strlen(path), tv, sizeof(struct timeval) * 2);
	return strlen(path) + 9 + sizeof(struct timeval) * 2;
}

static int pack_lstat(char *buf, const char *path)
{
	sprintf(buf, "5%s", "lstat");
	memcpy(buf + 7, path, strlen(path) + 1);
	return strlen(path) + 8;
}

static int pack_access(char *buf, const char *path, int mask)
{
	sprintf(buf, "5%s", "access");
	memcpy(buf + 8, path, strlen(path) + 1);
	memcpy(buf + 9 + strlen(path), &mask, sizeof(int));
	return strlen(path) + 9;
}

static int pack_opendir(char *buf, const char *path)
{
	sprintf(buf, "5%s", "opendir");
	memcpy(buf + 9, path, strlen(path) + 1);
	return strlen(path) + 10;
}

static int pack_readdir(char *buf, DIR **dir)
{
	sprintf(buf, "5%s", "readdir");
	memcpy(buf + 9, dir, sizeof(DIR *));
	printf("Is it an end of pack? \n");
	return sizeof(DIR *) + 9;
}

static int pack_closedir(char *buf, DIR **dir)
{
	sprintf(buf, "5%s", "closedir");
	memcpy(buf + 10, dir, sizeof(DIR *));
	return sizeof(DIR *) + 10;
}

static int pack_close(char *buf, int fd)
{
	sprintf(buf, "5%s", "close");
	memcpy(buf + 7, &fd, sizeof(int));
	return sizeof(int) + 7;
}

static int unpack(const char *syscall, char *buf, struct open_file *of, int server_num, struct stat *stbuf, DIR **dir, struct dirent *de)
{
	int rv;
	memcpy(&rv, buf, sizeof(int));
	errno = 0;
	if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));

	print2i("rv", rv);
	print2i("errno", errno);

	if (strcmp(syscall, "open") == 0) if (rv > -1) rv = add_fd(of, rv, server_num);
	if (strcmp(syscall, "lstat") == 0) memcpy(stbuf, buf + sizeof(int) * 2, sizeof(struct stat));
	if (strcmp(syscall, "opendir") == 0) memcpy(dir, buf + sizeof(int) * 2, sizeof(DIR *));
	if (strcmp(syscall, "readdir") == 0) memcpy(de, buf + sizeof(int) * 2, sizeof(struct dirent));
	if (strcmp(syscall, "close") == 0) if (rv > -1) remove_fd(of, server_num);
	return rv;
}

static int send_to_server(char buf[], size_t send_num, int server_num)
{	
	int sent_num = 0;
	while ((sent_num = write(server_fds[server_num], buf, send_num)) == -1)
	{
		if ((server_fds[server_num] = connect_to_server(servers[server_num])) == -1)
		{
			char *error = malloc(64);
			sprintf(error, "Cannot connect to server %d", server_num);
			nrf_print_error(error);
			free(error);
			return -1;
		}
	}
	if (sent_num != send_num) return -1;
	return sent_num;
}

static int setter_function_on_server(const char *syscall, const char *path, const char *to,
									 mode_t mode, int flags, struct open_file *of, dev_t rdev, struct timeval *tv)
{
	printf("setter start\n");
	char info[1024];
	sprintf(info, "%s path: %s, to: %s, mode: %d, flags: %d, of: %p, rdev: %d", syscall, path, to, (int)mode, flags, of, (int)rdev);
	nrf_print_info(info);

	char send_buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "mkdir") == 0) send_num = pack_mkdir(send_buf, path, mode);
	else if (strcmp(syscall, "mknod") == 0) send_num = pack_mknod(send_buf, path, mode, rdev);
	else if (strcmp(syscall, "rmdir") == 0) send_num = pack_rmdir(send_buf, path);
	else if (strcmp(syscall, "open") == 0) send_num = pack_open(send_buf, path, flags, mode);
	else if (strcmp(syscall, "rename") == 0) {
		// printf("\033[30;1maq mainc?\033[0m\n");
		send_num = pack_rename(send_buf, path, to);
	}
	else if (strcmp(syscall, "unlink") == 0) send_num = pack_unlink(send_buf, path);
	else if (strcmp(syscall, "utimes") == 0) send_num = pack_utimes(send_buf, path, tv);

	char receive_buf[BUFFER_SIZE];
	int hotswap_server_num = -1;
	int rv;
	int i = 0;
	for (; i < servers_count; i++)
	{
		// printf("AAAAAAAAAAAA send_num %d\n", send_num);
		int sent_num = send_to_server(send_buf, send_num, i);
		if (sent_num == -1)
		{	
			if (hotswap_server_num != -1) return SERVERS_DOWN;
			hotswap_server_num = i;
			continue;
		}
		// printf("BBBBBBBBBBBBB sent_num %d\n", sent_num);
		int rec_num = 0;
		int should_receive_num = receive_num(syscall);

		char *temp_buf = receive_buf;
		while (rec_num < should_receive_num)
		{
			should_receive_num -= rec_num;
			temp_buf += rec_num;

			// printf("BCBCBCBCBCB should_receive_num %d\n", should_receive_num);

			rec_num = read(server_fds[i], temp_buf, should_receive_num);

			// printf("\033[30;1mrec_num: %d\n", rec_num);
			// dumphex(temp_buf, 128);
			// printf("\033[0m\n");

			if (rec_num <= 0)
			{
				if (hotswap_server_num != -1) return SERVERS_DOWN;
				hotswap_server_num = i;
				break;
			}
		}

		// printf("CCCCCCCCCCCCCC\n");

		if (hotswap_server_num == i) continue;
		rv = unpack(syscall, receive_buf, of, i, NULL, NULL, NULL);
		if (rv == -1) break;
	}

	printf("setter end\n");

	if (hotswap_server_num != -1) hotswap(hotswap_server_num);

	return rv;
}

static int getter_function_on_server(const char *syscall, const char *path, int mask, DIR **dir, int flags, struct open_file *of, int mode, struct stat *stbuf, int *server)
{	
	printf("getter start\n");
	char info[1024];
	sprintf(info, "%s path: %s, mask: %d, flags: %d, of: %p, mode: %d, stbuf: %p, server: %p", syscall, path, mask, flags, of, mode, stbuf, server);
	nrf_print_info(info);
	
	char send_buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "lstat") == 0) send_num = pack_lstat(send_buf, path);
	else if(strcmp(syscall, "open") == 0) send_num = pack_open(send_buf, path, flags, mode);
	else if (strcmp(syscall, "access") == 0) send_num = pack_access(send_buf, path, mask);
	else if (strcmp(syscall, "opendir") == 0) send_num = pack_opendir(send_buf, path);

	nrf_print_value(send_buf);
	nrf_print_value(send_buf + strlen(send_buf) + 1);

	char receive_buf[BUFFER_SIZE];
	int hotswap_server_num = -1;
	int i = 0;
	int rv = -1;
	for (; i < servers_count; i++)
	{
		int sent_num = send_to_server(send_buf, send_num, i);
		if (sent_num == -1)
		{
			if (hotswap_server_num != -1) return SERVERS_DOWN;
			hotswap_server_num = i;
			continue;
		}
		int rec_num = 0;
		int should_receive_num = receive_num(syscall);

		char *temp_buf = receive_buf;
		while (rec_num < should_receive_num)
		{
			should_receive_num -= rec_num;
			temp_buf += rec_num;
			rec_num = read(server_fds[i], temp_buf, should_receive_num);
			if (rec_num <= 0)
			{
				if (hotswap_server_num != -1) return SERVERS_DOWN;
				hotswap_server_num = i;
				break;
			}
		}
		if (hotswap_server_num == i) continue;
		rv = unpack(syscall, receive_buf, of, i, stbuf, dir, NULL);
		printf("loop end %d\n", i);
		break;
	}

	*server = i;
	if (hotswap_server_num != -1) hotswap(hotswap_server_num);
	return rv;
}

static int function_on_one_server(const char *syscall, DIR **dir, struct dirent *de, int id, int server)
{
	printf("oner start\n");
	char info[1024];
	sprintf(info, "%s de: %p, id: %d, server: %d", syscall, de, id, server);
	nrf_print_info(info);

	char buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "readdir") == 0) send_num = pack_readdir(buf, dir);
	else if (strcmp(syscall, "closedir") == 0) send_num = pack_closedir(buf, dir);
	else if (strcmp(syscall, "close") == 0) send_num = pack_close(buf, open_files[id]->fds[server]);

	int sent_num = send_to_server(buf, send_num, server);
	if (sent_num == -1)
	{	
		hotswap(server);
		return -1;
	}
	int rec_num = 0;
	int should_receive_num = receive_num(syscall);

	char *temp_buf = buf;
	while (rec_num < should_receive_num)
	{
		should_receive_num -= rec_num;
		temp_buf += rec_num;
		rec_num = read(server_fds[server], temp_buf, should_receive_num);
		if (rec_num <= 0)
		{
			hotswap(server);
			return -1;
		}
	}

	int rv = unpack(syscall, buf, open_files[id], server, NULL, dir, de);
	return rv;
}

static int pread_on_server(int open_fd, char *read_buf, size_t size, off_t offset, int server)
{
	// char buf[1024];
	// sprintf(buf, "5%s", "pread");
	// memcpy(buf + 7, &open_fd, sizeof(int));
	// memcpy(buf + 7 + sizeof(int), &size, sizeof(size_t));
	// memcpy(buf + 7 + sizeof(int) + sizeof(size_t), &offset, sizeof(off_t));

	// if (server == 0) server = GET;
	// int status = send_to_server(buf, 7 + sizeof(int) + sizeof(size_t) + sizeof(off_t), server);

	// if (status == -1) return -1;

	// int fd;
	// if (status == 2) fd = server_fd_2;
	// else fd = server_fd_1;

	// if (read(fd, buf, sizeof(int) * 2) == -1) return -1;

	// int rv;
	// memcpy(&rv, buf, sizeof(int));
	// memcpy(&errno, buf + sizeof(int), sizeof(int));
	// int num = rv;
	// while (num > 0)
	// {
	// 	read(fd, buf, 1024);
	// 	num -= 1024;
	// 	memcpy(read_buf, buf, 1024);
	// 	read_buf += 1024;
	// }

	// return rv;
	return 0;
}

static int pwrite_on_server(const char *path, struct open_file *of, const char *write_buf, size_t size, off_t offset)
{
	// char buf[1024];
	// sprintf(buf, "5%s", "pwrite");
	// memcpy(buf + 8, path, strlen(path) + 1);
	// memcpy(buf + 9 + strlen(path), &of->fd_1, sizeof(int));
	// memcpy(buf + 9 + strlen(path) + sizeof(int), &size, sizeof(size_t));
	// memcpy(buf + 9 + strlen(path) + sizeof(int) + sizeof(size_t), &offset, sizeof(off_t));

	// int status_1 = send_to_server(buf, 9 + strlen(path) + sizeof(int) + sizeof(size_t) + sizeof(off_t), 1);

	// if(status_1 < 0) return -1;

	// memcpy(buf + 9 + strlen(path), &of->fd_2, sizeof(int));
	// int status_2 = send_to_server(buf, 9 + strlen(path) + sizeof(int) + sizeof(size_t) + sizeof(off_t), 2);

	// if(status_2 < 0) return -1;

	// int int_size = (int)size;
	// int status;

	// while (int_size > 0)
	// {
	// 	printf("loop start\n");
	// 	int write_size = 1024;
	// 	if (int_size < 1024) write_size = int_size;
	// 	memcpy(buf, write_buf, write_size);
	// 	int_size -= 1024;
	// 	status = send_to_server(buf, write_size, SET);
	// 	if (status != 0) return -1;
	// 	printf("loop end\n");
	// }

	// status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	// status_2 = read(server_fd_2, buf + sizeof(int) * 2, sizeof(int) * 2);

	// if (status_1 < 0 || status_2 < 0) return -1;

	// int rv_1, rv_2;
	// memcpy(&rv_1, buf, sizeof(int));
	// memcpy(&errno, buf + sizeof(int), sizeof(int));
	// memcpy(&rv_2, buf + sizeof(int) * 2, sizeof(int));
	// if (rv_2 < 0)
	// {
	// 	memcpy(&errno, buf + sizeof(int) * 3, sizeof(int));
	// 	return rv_2;
	// }

	// return rv_1;
	return 0;
}

static int truncate_on_server(const char *path, off_t size)
{
	// char buf[1024];
	// sprintf(buf, "5%s", "truncate");
	// memcpy(buf + 10, path, strlen(path) + 1);
	// memcpy(buf + 11 + strlen(path), &size, sizeof(off_t));
	// int status = send_to_server(buf, strlen(path) + 11 + sizeof(off_t), SET);

	// if (status != 0) return -1;

	// int status_1 = read(server_fd_1, buf, sizeof(int) * 2);
	// int status_2 = read(server_fd_2, buf, sizeof(int) * 2);

	// if (status_1 < 0 || status_2 < 0) return -1;

	// int rv;
	// memcpy(&rv, buf, sizeof(int));
	// memcpy(&errno, buf + sizeof(int), sizeof(int));

	// return rv;
	return 0;
}

static int raid_5_getattr(const char *path, struct stat *stbuf);
// static int raid_5_access(const char *path, int mask);
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
	int server;
	if (getter_function_on_server("lstat", path, 0, NULL, 0, NULL, 0, stbuf, &server) == -1) return -errno;
	return 0;
}

// static int raid_5_access(const char *path, int mask)
// {
// 	print2("access", path);
// 	int server;
// 	if (getter_function_on_server("access", path, mask, NULL, 0, NULL, 0, NULL, &server) == -1) return -errno;
// 	return 0;
// }

static int raid_5_mknod(const char *path, mode_t mode, dev_t rdev)
{
	print2("mknod", path);
	if (setter_function_on_server("mknod", path, "", mode, 0, NULL, rdev, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_mkdir(const char *path, mode_t mode)
{
	print2("mkdir", path);
	if (setter_function_on_server("mkdir", path, "", mode, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	print2("readdir", path);

	DIR *dp = NULL;
	struct dirent *de = malloc(sizeof(struct dirent));

	(void) offset;
	(void) fi;

	int server;
	if (getter_function_on_server("opendir", path, 0, &dp, 0, NULL, 0, NULL, &server) == -1) return -errno;

	while (function_on_one_server("readdir", &dp, de, 0, server) != -1)
	{
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0)) break;
		free(de);
		de = malloc(sizeof(struct dirent));
	}
	free(de);

	function_on_one_server("closedir", &dp, NULL, 0, server);
	return 0;
}

static int raid_5_rmdir(const char *path)
{
	print2("rmdir", path);
	if (setter_function_on_server("rmdir", path, "", 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	print2("create", path);
	struct open_file *of = init_open_file();
	int res = setter_function_on_server("open", path, "", mode, fi->flags, of, 0, NULL);
	fi->fh = res;

	printf("\033[30;1mfi->fh: %d\033[0m\n", (int)fi->fh);

	print_open_files();

	if (res < 0) return -errno;
	return 0;
}

static int raid_5_open(const char *path, struct fuse_file_info *fi)
{
	print2("open", path);
	
	struct open_file *of = init_open_file();
	int res;
	int server;
	if (fi->flags & (O_WRONLY | O_CREAT))
		res = setter_function_on_server("open", path, "", 0, fi->flags, of, 0, NULL);
	else
		res = getter_function_on_server("open", path, 0, NULL, fi->flags, of, 0, NULL, &server);
	fi->fh = res;

	print_open_files();

	if (res < 0) return -errno;
	return 0;
}

static int raid_5_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	print2("read", path);

	(void) fi;
	int res;

	struct open_file *of = open_files[fi->fh];
	if (of == NULL) return -1;

	print_open_files();

	int open_fd;
	int server;

	int i = 0;
	for (; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++){
		if (of->fds[i] >= 0){
			server = i;
			open_fd = of->fds[i];
			break;
		}
	}

	res = pread_on_server(open_fd, buf, size, offset, server);
	if (res == -1) res = -errno;

	return res;
}

static int raid_5_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	print2("write", path);

	int res;

	struct open_file *of = open_files[fi->fh];
	if (of == NULL) return -1;

	print_open_files();

	res = pwrite_on_server(path, of, buf, size, offset);
	if (res == -1) res = -errno;

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
	printf("\033[30;1mfi->fh: %d\033[0m\n", (int)fi->fh);

	int i = 0;
	for (; i < servers_count; i++)
		if (open_files[fi->fh] != NULL && open_files[fi->fh]->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, fi->fh, i);

	print_open_files();

	return 0;
}

static int raid_5_rename(const char *from, const char *to)
{
	print3("rename", from, to);

	if (setter_function_on_server("rename", from, to, 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_unlink(const char *path)
{
	print2("unlink", path);
	if (setter_function_on_server("unlink", path, "", 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_utimens(const char *path, const struct timespec ts[2])
{
	// print2("utimens", path);

	// struct timeval tv[2];

	// tv[0].tv_sec = ts[0].tv_sec;
	// tv[0].tv_usec = ts[0].tv_nsec / 1000;
	// tv[1].tv_sec = ts[1].tv_sec;
	// tv[1].tv_usec = ts[1].tv_nsec / 1000;

	// if (setter_function_on_server("unlink", path, "", 0, 0, NULL, 0, tv) == -1) return -errno;
	return 0;
}

static struct fuse_operations raid_5_oper = {
	.getattr	= raid_5_getattr,
	// .access		= raid_5_access,
	.readdir	= raid_5_readdir,
	.mknod		= raid_5_mknod,
	.create     = raid_5_create,
	.mkdir		= raid_5_mkdir,
	.unlink		= raid_5_unlink,
	.rmdir		= raid_5_rmdir,
	.rename		= raid_5_rename,
	.truncate	= raid_5_truncate,
	.open		= raid_5_open,
	.read		= raid_5_read,
	.write		= raid_5_write,
	.release	= raid_5_release,
	.utimens	= raid_5_utimens,
};

int mount_raid_5(int argc, char *argv[], struct storage *storage)
{
	global_storage = storage;

	init_open_files();

	struct server *server = global_storage->servers;
	int i = 0;
	while (server != NULL)
	{
		server_fds[i] = connect_to_server(server);
		servers[i] = server;
		server = server->next_server;
		i++;
	}
	servers_count = i;
	for (; i < MAX_SERVERS; i++)
	{
		server_fds[i] = UNSET_FD;
		servers[i] = NULL;	
	}

	umask(0);
	return fuse_main(argc, argv, &raid_5_oper, NULL);
}
