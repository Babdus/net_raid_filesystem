#include "fuse_raid_5.h"

#define BUFFER_SIZE 1024
#define CHUNK_SIZE 256

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
	char *path;
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
			printf("path: %s\n", open_files[i]->path);
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

static int add_fd(struct open_file *of, int fd, int server_num, const char *path)
{
	if (of->id == -1)
	{
		int i = 0;
		while (open_files[i] != NULL) i++;
		open_files[i] = of;
		of->id = i;
		of->path = strdup(path);
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
		free(of->path);
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

static int pack_write(char *buf, int fd, int offset, int size, const char *write_buf)
{
	sprintf(buf, "5%s", "write");
	memcpy(buf + 7, &fd, sizeof(int));
	memcpy(buf + 7 + sizeof(int), &offset, sizeof(int));
	memcpy(buf + 7 + sizeof(int) * 2, &size, sizeof(int));
	memcpy(buf + 7 + sizeof(int) * 3, write_buf, size);
	return 7 + sizeof(int) * 3 + size;
}

static int pack_read(char *buf, int fd, int offset, int size)
{
	sprintf(buf, "5%s", "read");
	memcpy(buf + 6, &fd, sizeof(int));
	memcpy(buf + 6 + sizeof(int), &offset, sizeof(int));
	memcpy(buf + 6 + sizeof(int) * 2, &size, sizeof(int));
	return 6 + sizeof(int) * 3;
}

static int pack_truncate(char *buf, const char *path, off_t size)
{
	sprintf(buf, "5%s", "truncate");
	memcpy(buf + 10, path, strlen(path) + 1);
	memcpy(buf + 11 + strlen(path), &size, sizeof(off_t));
	return 11 + strlen(path) + sizeof(off_t);
}

static int pack_close(char *buf, int fd)
{
	sprintf(buf, "5%s", "close");
	memcpy(buf + 7, &fd, sizeof(int));
	return sizeof(int) + 7;
}

static int unpack(const char *syscall, const char *path, char *buf, struct open_file *of, int server_num, struct stat *stbuf, DIR **dir, struct dirent *de)
{
	int rv;
	memcpy(&rv, buf, sizeof(int));
	errno = 0;
	if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));

	print2i("rv", rv);
	print2i("errno", errno);

	if (strcmp(syscall, "open") == 0) if (rv > -1) rv = add_fd(of, rv, server_num, path);
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
	else if (strcmp(syscall, "rename") == 0) send_num = pack_rename(send_buf, path, to);
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
		rv = unpack(syscall, path, receive_buf, of, i, NULL, NULL, NULL);
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
		rv = unpack(syscall, path, receive_buf, of, i, stbuf, dir, NULL);
		printf("loop end %d\n", i);
		break;
	}

	*server = i;
	if (hotswap_server_num != -1) hotswap(hotswap_server_num);
	return rv;
}

static int function_on_one_server(const char *syscall, DIR **dir, struct dirent *de, int id, int server, int offset, int size, const char *write_buf, char *read_buf, struct stat *stbuf, const char *path)
{
	printf("oner start\n");
	char info[1024];
	sprintf(info, "%s de: %p, id: %d, server: %d, offset: %d, size: %d, path: %s", syscall, de, id, server, offset, size, path);
	nrf_print_info(info);

	char buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "readdir") == 0) send_num = pack_readdir(buf, dir);
	else if (strcmp(syscall, "closedir") == 0) send_num = pack_closedir(buf, dir);
	else if (strcmp(syscall, "write") == 0) send_num = pack_write(buf, open_files[id]->fds[server], offset, size, write_buf);
	else if (strcmp(syscall, "read") == 0) send_num = pack_read(buf, open_files[id]->fds[server], offset, size);
	else if (strcmp(syscall, "close") == 0) send_num = pack_close(buf, open_files[id]->fds[server]);
	else if (strcmp(syscall, "lstat") == 0) send_num = pack_lstat(buf, path);
	else if (strcmp(syscall, "truncate") == 0) send_num = pack_truncate(buf, path, size);

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

	int rv = unpack(syscall, NULL, buf, open_files[id], server, stbuf, dir, de);

	if (rv > 0 && strcmp(syscall, "read") == 0)
	{
		rv = read(server_fds[server], read_buf, size);
	}

	return rv;
}

static void hash(char *xor, char *read_buf, int size)
{
	int i = 0;
	for (; i < size; i++)
		xor[i] ^= read_buf[i];
}

static int xor_stripe(int id, int stripe)
{
	int xor_server = servers_count - (stripe % servers_count) - 1;
	char read_buf[CHUNK_SIZE];
	char xor[CHUNK_SIZE];
	memset(xor, 0, CHUNK_SIZE);
	int i = 0;

	struct open_file *of = init_open_file();
	int res = setter_function_on_server("open", open_files[id]->path, "", 0, O_RDONLY, of, 0, NULL);

	printf("open res: %d\n", res);

	for(; i < servers_count; i++)
	{
		if (i != xor_server)
		{
			memset(read_buf, 0, CHUNK_SIZE);
			res = function_on_one_server("read", NULL, NULL, of->id, i, stripe * CHUNK_SIZE, CHUNK_SIZE, NULL, read_buf, NULL, NULL);

			printf("read_buf: %s\n", read_buf);
			printf("res %d\n", res);

			hash(xor, read_buf, res);
		}
	}

	for (i = 0; i < servers_count; i++)
		if (of != NULL && of->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, of->id, i, 0, 0, NULL, NULL, NULL, NULL);

	return function_on_one_server("write", NULL, NULL, id, xor_server, stripe * CHUNK_SIZE, CHUNK_SIZE, xor, NULL, NULL, NULL);
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
	int res;
	int i = 0;
	off_t size = 0;
	for (; i < servers_count; i++)
	{
		res = function_on_one_server("lstat", NULL, NULL, 0, i, 0, 0, NULL, NULL, stbuf, path);
		if (res == -1) return -errno;
		if (S_ISREG(stbuf->st_mode))
		{
			stbuf->st_size -= ((stbuf->st_size/CHUNK_SIZE + i)/servers_count)*CHUNK_SIZE;
			size += stbuf->st_size;
		}
		else size = stbuf->st_size;
	}
	stbuf->st_size = size;

	// if (getter_function_on_server("lstat", path, 0, NULL, 0, NULL, 0, stbuf, &server) == -1) return -errno;
	return 0;
}

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

	while (function_on_one_server("readdir", &dp, de, 0, server, 0, 0, NULL, NULL, NULL, NULL) != -1)
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

	function_on_one_server("closedir", &dp, NULL, 0, server, 0, 0, NULL, NULL, NULL, NULL);
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
	// int server;
	// if (fi->flags & (O_WRONLY | O_CREAT))
	res = setter_function_on_server("open", path, "", 0, fi->flags, of, 0, NULL);
	// else
	// 	res = getter_function_on_server("open", path, 0, NULL, fi->flags, of, 0, NULL, &server);
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
	int res = 0;

	struct open_file *of = open_files[fi->fh];
	if (of == NULL) return -1;

	print_open_files();

	int chunk_n = offset / CHUNK_SIZE;
	int local_offset = offset % CHUNK_SIZE;
	int local_size = CHUNK_SIZE - local_offset;
	int server;
	int stripe = -1;
	int temp_res;
	while (size > local_size)
	{
		server = chunk_n % servers_count;
		stripe = chunk_n / (servers_count - 1);
		temp_res = function_on_one_server("read", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, local_size, NULL, buf, NULL, NULL);
		printf("\033[30;1mBUFFER:\n%s\033[0m\n", buf);
		printf("res: %d\n", res);
		if (temp_res == -1) return -errno;
		if (temp_res == 0) return res;
		res += temp_res;
		size -= local_size;
		buf += local_size;
		local_size = CHUNK_SIZE;
		chunk_n++;
		local_offset = 0;
	}
	server = chunk_n % servers_count;
	stripe = chunk_n / (servers_count - 1);
	temp_res = function_on_one_server("read", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, size, NULL, buf, NULL, NULL);
	printf("\033[30;1mBUFFER:\n%s\033[0m\n", buf);
	if (temp_res == -1) return -errno;
	if (temp_res == 0) return res;
	res += temp_res;
	printf("res: %d\n", res);
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

	int chunk_n = offset / CHUNK_SIZE;
	int local_offset = offset % CHUNK_SIZE;
	int local_size = CHUNK_SIZE - local_offset;
	int server;
	int stripe = -1;
	int last_stripe = -1;
	while (size > local_size)
	{
		server = chunk_n % servers_count;
		last_stripe = stripe;
		stripe = chunk_n / (servers_count - 1);
		if (stripe > last_stripe && last_stripe != -1)
			if (xor_stripe(fi->fh, last_stripe) == -1) return -errno;
		res = function_on_one_server("write", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, local_size, buf, NULL, NULL, NULL);
		if (res == -1) return -errno;
		size -= local_size;
		buf += local_size;
		local_size = CHUNK_SIZE;
		chunk_n++;
		local_offset = 0;
	}
	server = chunk_n % servers_count;
	last_stripe = stripe;
	stripe = chunk_n / (servers_count - 1);
	if (stripe > last_stripe && last_stripe != -1)
		if (xor_stripe(fi->fh, last_stripe) == -1) return -errno;
	res = function_on_one_server("write", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, size, buf, NULL, NULL, NULL);
	if (stripe > last_stripe && stripe != -1)
		if (xor_stripe(fi->fh, stripe) == -1) return -errno;

	if (res == -1) return -errno;
	return size;
}

static int raid_5_truncate(const char *path, off_t size)
{
	print2("truncate", path);

	int res;
	printf("\033[30;1msize %d\033[0m\n", (int)size);
	int chunks = size / CHUNK_SIZE;
	printf("\033[30;1mchunks %d\033[0m\n", chunks);
	int reminder = size % CHUNK_SIZE;
	printf("\033[30;1mreminder %d\033[0m\n", reminder);
	int xor_stripe = chunks / (servers_count - 1);
	printf("\033[30;1mxor_stripe %d\033[0m\n", xor_stripe);
	int xor_server = servers_count - xor_stripe % servers_count - 1;
	printf("\033[30;1mxor_server %d\033[0m\n", xor_server);
	int is_larger = 0;
	printf("\033[30;1mis_larger %d\033[0m\n", is_larger);
	int last_chunk_server = chunks % servers_count;
	printf("\033[30;1mlast_chunk_server %d\033[0m\n", last_chunk_server);
	int i;
	for (i = (xor_server - 1 + servers_count) % servers_count; i != xor_server; i = (i - 1 + servers_count) % servers_count){
		printf("\033[30;1mi %d\033[0m\n", i);
		if (i == last_chunk_server)
		{
			res = function_on_one_server("truncate", NULL, NULL, 0, i, 0, xor_stripe * CHUNK_SIZE + reminder, NULL, NULL, NULL, path);
			if (res == -1) return -errno;
			if ((i - 1 + servers_count) % servers_count == xor_server && reminder == 0) is_larger = 0;
			else is_larger = 1;
			continue;
		}
		if (is_larger == 0)
		{
			res = function_on_one_server("truncate", NULL, NULL, 0, i, 0, xor_stripe * CHUNK_SIZE, NULL, NULL, NULL, path);
			if (res == -1) return -errno;
		}
		else
		{
			res = function_on_one_server("truncate", NULL, NULL, 0, i, 0, (xor_stripe + 1) * CHUNK_SIZE, NULL, NULL, NULL, path);
			if (res == -1) return -errno;
		}
	}
	printf("\033[30;1mis_larger %d\033[0m\n", is_larger);
	res = function_on_one_server("truncate", NULL, NULL, 0, xor_server, 0, (xor_stripe + is_larger) * CHUNK_SIZE, NULL, NULL, NULL, path);
	if (res == -1) return -errno;
	return 0;
}

static int raid_5_release(const char *path, struct fuse_file_info *fi)
{
	print2("release", path);
	printf("\033[30;1mfi->fh: %d\033[0m\n", (int)fi->fh);

	int i = 0;
	for (; i < servers_count; i++)
		if (open_files[fi->fh] != NULL && open_files[fi->fh]->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, fi->fh, i, 0, 0, NULL, NULL, NULL, NULL);

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
