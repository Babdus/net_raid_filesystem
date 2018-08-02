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
#define SERVER_NOT_CONNECTED -758

struct storage *global_storage;

FILE *err_file;

int server_fds[MAX_SERVERS];
struct server *servers[MAX_SERVERS];
int servers_count;

int hotswaped_server = -1;

char temp_chunk[CHUNK_SIZE];

struct open_file {
	int fds[MAX_SERVERS];
	char *path;
	int id;
};

struct open_file *open_files[OPEN_FILES_NUM];

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
	return of->id;
}

static void remove_fd(struct open_file *of, int server_num)
{
	of->fds[server_num] = UNOPENED;
	int res = -1;
	int i = 0;
	for (; i < MAX_SERVERS; i++)
		if (of->fds[i] != UNSET_FD && of->fds[i] != UNOPENED) res = 0;

	if (res == -1)
	{	
		free(of->path);
		int id = of->id;
		free(open_files[id]);
		open_files[id] = NULL;
	}
}

static void hotswap(int server_num)
{
	if (hotswaped_server != server_num)
	{
		nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[server_num]->ip, servers[server_num]->port, "server declared as lost");
		hotswaped_server = server_num;
	}
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
		if ((server_fds[server_num] = connect_to_server(servers[server_num], global_storage)) == -1)
			break;
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
		if (server_fds[i] == -1)
		{
			if (hotswap_server_num != -1)
			{
				nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
				exit(143);
			}
			hotswap_server_num = i;
			continue;
		}
		int sent_num = send_to_server(send_buf, send_num, i);
		if (sent_num == -1)
		{	
			if (hotswap_server_num != -1)
			{
				nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
				exit(143);
			}
			hotswap_server_num = i;
			server_fds[i] = -1;
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
				if (hotswap_server_num != -1)
				{
					nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
					exit(143);
				}
				hotswap_server_num = i;
				server_fds[i] = -1;
				break;
			}
		}
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

	char receive_buf[BUFFER_SIZE];
	int hotswap_server_num = -1;
	int i = 0;
	int rv = -1;
	for (; i < servers_count; i++)
	{
		if (server_fds[i] == -1)
		{
			if (hotswap_server_num != -1)
			{
				nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
				exit(143);
			}
			hotswap_server_num = i;
			continue;
		}
		int sent_num = send_to_server(send_buf, send_num, i);
		if (sent_num == -1)
		{
			if (hotswap_server_num != -1)
			{
				nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
				exit(143);
			}
			hotswap_server_num = i;
			server_fds[i] = -1;
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
				if (hotswap_server_num != -1)
				{
					nrf_print_error_x(global_config->err_log_path, global_storage->diskname, servers[i]->ip, servers[i]->port, "more than one server is dead!");
					exit(143);
				}
				hotswap_server_num = i;
				server_fds[i] = -1;
				break;
			}
		}
		if (hotswap_server_num == i) continue;
		rv = unpack(syscall, path, receive_buf, of, i, stbuf, dir, NULL);
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

	if (server_fds[server] == -1)
		return SERVER_NOT_CONNECTED;

	int sent_num = send_to_server(buf, send_num, server);
	if (sent_num == -1)
	{	
		hotswap(server);
		return SERVER_NOT_CONNECTED;
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
			server_fds[server] = -1;
			return SERVER_NOT_CONNECTED;
		}
	}

	int rv = unpack(syscall, NULL, buf, open_files[id], server, stbuf, dir, de);

	printf("\033[30;1mrv: %d\n", rv);
	if (write_buf != NULL)
	{
		int i = 0;
		for (;i < rv; i++)
		{
			printf("%c", write_buf[i]);
		}
	}
	printf("\033[0m\n");

	if (rv > 0 && strcmp(syscall, "read") == 0)
	{
		rv = read(server_fds[server], read_buf, size);
	}

	return rv;
}

static void hash(char *xor, const char *read_buf, int size)
{
	int i = 0;
	for (; i < size; i++)
		xor[i] ^= read_buf[i];
}

// static int xor_stripe(int id, int stripe)
// {
// 	printf("\033[33;1mxor\033[0m\n");
// 	int xor_server = servers_count - (stripe % servers_count) - 1;
// 	char read_buf[CHUNK_SIZE];
// 	char xor[CHUNK_SIZE];
// 	memset(xor, 0, CHUNK_SIZE);
// 	int i = 0;

// 	struct open_file *of = init_open_file();
// 	int res = setter_function_on_server("open", open_files[id]->path, "", 0, O_RDONLY, of, 0, NULL);

// 	for(; i < servers_count; i++)
// 	{
// 		if (i != xor_server)
// 		{
// 			memset(read_buf, 0, CHUNK_SIZE);
// 			res = function_on_one_server("read", NULL, NULL, of->id, i, stripe * CHUNK_SIZE, CHUNK_SIZE, NULL, read_buf, NULL, NULL);

// 			hash(xor, read_buf, res);
// 			printf("res:%d\n", res);
// 			printf("read_buf:\n%s\n", read_buf);
// 			printf("xor:\n%s\n", xor);
// 		}

// 	}

// 	for (i = 0; i < servers_count; i++)
// 		if (of != NULL && of->fds[i] != UNOPENED)
// 			function_on_one_server("close", NULL, NULL, of->id, i, 0, 0, NULL, NULL, NULL, NULL);

// 	return function_on_one_server("write", NULL, NULL, id, xor_server, stripe * CHUNK_SIZE, CHUNK_SIZE, xor, NULL, NULL, NULL);
// }

static int set_old_data(int id, char *old_data, int server, int stripe, int offset, int size)
{
	int res;
	if (server_fds[server] == -1)
	{
		char xor[size];
		
		int i;
		for (i = 0; i < servers_count; i++)
		{
			if (i != server)
			{
				memset(xor, 0, size);
				res = function_on_one_server("read", NULL, NULL, id, i, stripe * CHUNK_SIZE + offset, size, NULL, xor, NULL, NULL);
				hash(old_data, xor, size);
			}
		}
	}
	else
	{
		res = function_on_one_server("read", NULL, NULL, id, server, stripe * CHUNK_SIZE + offset, size, NULL, old_data, NULL, NULL);
	}
	return res;
}

static int write_on_parity(int server, int stripe, int id, int offset, int size, const char *new_data)
{
	int xor_server = servers_count - (stripe % servers_count) - 1;
	char old_data[size];
	memset(old_data, 0, size);

	struct open_file *of = init_open_file();
	int res = setter_function_on_server("open", open_files[id]->path, "", 0, O_RDONLY, of, 0, NULL);

	res = set_old_data(of->id, old_data, server, stripe, offset, size);

	hash(old_data, new_data, size);

	char parity[size];
	memset(parity, 0, size);

	res = function_on_one_server("read", NULL, NULL, of->id, xor_server, stripe * CHUNK_SIZE + offset, size, NULL, parity, NULL, NULL);

	int i;
	for (i = 0; i < servers_count; i++)
		if (of != NULL && of->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, of->id, i, 0, 0, NULL, NULL, NULL, NULL);

	hash(parity, old_data, size);

	res = function_on_one_server("write", NULL, NULL, id, xor_server, stripe * CHUNK_SIZE + offset, size, parity, NULL, NULL, NULL);
	return res;
}

static int raid_5_getattr(const char *path, struct stat *stbuf);
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
	int res;
	int i = 0;
	off_t size = 0;
	off_t total_size = 0;
	int not_connected = -1;
	for (; i < servers_count; i++)
	{
		res = function_on_one_server("lstat", NULL, NULL, 0, i, 0, 0, NULL, NULL, stbuf, path);

		printf("res: %d\n", res);

		if (res == -1) return -errno;
		if (res == SERVER_NOT_CONNECTED){
			not_connected = i;
			continue;
		}
		if (S_ISREG(stbuf->st_mode))
		{
			total_size += stbuf->st_size;
			stbuf->st_size -= ((stbuf->st_size/CHUNK_SIZE + i)/servers_count)*CHUNK_SIZE;
			size += stbuf->st_size;
		}
		else size = stbuf->st_size;
		printf("size %lu\n", size);
	}
	stbuf->st_size = size;
	if (not_connected > -1)
	{
		int one_server_size = ((total_size + (CHUNK_SIZE - 1) * (servers_count - 1)) / CHUNK_SIZE / (servers_count - 1)) * CHUNK_SIZE;
		one_server_size -= ((one_server_size/CHUNK_SIZE + not_connected)/servers_count)*CHUNK_SIZE;
		stbuf->st_size += one_server_size;
	}

	// if (getter_function_on_server("lstat", path, 0, NULL, 0, NULL, 0, stbuf, &server) == -1) return -errno;
	return 0;
}

static int raid_5_mknod(const char *path, mode_t mode, dev_t rdev)
{
	if (setter_function_on_server("mknod", path, "", mode, 0, NULL, rdev, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_mkdir(const char *path, mode_t mode)
{
	if (setter_function_on_server("mkdir", path, "", mode, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
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
	if (setter_function_on_server("rmdir", path, "", 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	struct open_file *of = init_open_file();
	int res = setter_function_on_server("open", path, "", mode, fi->flags, of, 0, NULL);
	fi->fh = res;

	if (res < 0) return -errno;
	return 0;
}

static int raid_5_open(const char *path, struct fuse_file_info *fi)
{
	struct open_file *of = init_open_file();
	int res;
	// int server;
	// if (fi->flags & (O_WRONLY | O_CREAT))
	res = setter_function_on_server("open", path, "", 0, fi->flags, of, 0, NULL);
	// else
	// 	res = getter_function_on_server("open", path, 0, NULL, fi->flags, of, 0, NULL, &server);
	fi->fh = res;

	if (res < 0) return -errno;
	return 0;
}

static int raid_5_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;

	struct open_file *of = open_files[fi->fh];
	if (of == NULL) return -1;

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
		if (server_fds[server] == -1)
		{
			printf("This is lost server\n");
			char *lost_buf = malloc(CHUNK_SIZE);
			int i = 0;
			memset(buf, 0, CHUNK_SIZE);
			for(; i < servers_count; i++)
			{
				if (i != server)
				{
					printf("server %d for xor\n", i);
					memset(lost_buf, 0, CHUNK_SIZE);
					temp_res = function_on_one_server("read", NULL, NULL, fi->fh, i, stripe * CHUNK_SIZE + local_offset, local_size, NULL, lost_buf, NULL, NULL);
					
					printf("lost_buf: \n%s\n", lost_buf);

					if (temp_res == -1) return -errno;
					hash(buf, lost_buf, temp_res);

					printf("buf: \n%s\n", buf);
				}
			}
			free(lost_buf);
		}
		else temp_res = function_on_one_server("read", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, local_size, NULL, buf, NULL, NULL);
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
	if (server_fds[server] == -1)
	{
		printf("This is lost server\n");
		char *lost_buf = malloc(CHUNK_SIZE);
		int i = 0;
		memset(buf, 0, CHUNK_SIZE);
		for(; i < servers_count; i++)
		{
			if (i != server)
			{
				printf("server %d for xor\n", i);
				memset(lost_buf, 0, CHUNK_SIZE);
				temp_res = function_on_one_server("read", NULL, NULL, fi->fh, i, stripe * CHUNK_SIZE + local_offset, size, NULL, lost_buf, NULL, NULL);
				
				printf("lost_buf: \n%s\n", lost_buf);

				if (temp_res == -1) return -errno;
				hash(buf, lost_buf, temp_res);

				printf("buf: \n%s\n", buf);
			}
		}
		free(lost_buf);
	}
	else temp_res = function_on_one_server("read", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, size, NULL, buf, NULL, NULL);
	if (temp_res == -1) return -errno;
	if (temp_res == 0) return res;
	res += temp_res;
	return res;
}

static int raid_5_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	printf("\033[31;1mwrite\033[0m\n");
	int res;
	int res_size = size;

	struct open_file *of = open_files[fi->fh];
	if (of == NULL) return -1;

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
		printf("\033[32mstripe: %d, last_stripe: %d\033[0m\n", stripe, last_stripe);
		
		res = write_on_parity(server, stripe, fi->fh, local_offset, local_size, buf);
		if (res == -1) return -errno;
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
	printf("\033[32mstripe: %d, last_stripe: %d\033[0m\n", stripe, last_stripe);

	res = write_on_parity(server, stripe, fi->fh, local_offset, local_size, buf);
	if (res == -1) return -errno;
	res = function_on_one_server("write", NULL, NULL, fi->fh, server, (stripe * CHUNK_SIZE) + local_offset, size, buf, NULL, NULL, NULL);
	printf("\033[32mstripe: %d, last_stripe: %d\033[0m\n", stripe, last_stripe);
	printf("res: %d\n", res);
	if (res == -1) return -errno;
	printf("\033[31;1m%d\n\033[0m", (int)size);
	return res_size;
}

static int raid_5_truncate(const char *path, off_t size)
{
	int res;
	int chunks = size / CHUNK_SIZE;
	int reminder = size % CHUNK_SIZE;
	int xor_stripe = chunks / (servers_count - 1);
	int xor_server = servers_count - xor_stripe % servers_count - 1;
	int is_larger = 0;
	int last_chunk_server = chunks % servers_count;
	int i;
	for (i = (xor_server - 1 + servers_count) % servers_count; i != xor_server; i = (i - 1 + servers_count) % servers_count){
		if (server_fds[i] == -1) continue;
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
	if (server_fds[xor_server] != -1)
		res = function_on_one_server("truncate", NULL, NULL, 0, xor_server, 0, (xor_stripe + is_larger) * CHUNK_SIZE, NULL, NULL, NULL, path);
	if (res == -1) return -errno;
	return 0;
}

static int raid_5_release(const char *path, struct fuse_file_info *fi)
{
	int i = 0;
	for (; i < servers_count; i++)
		if (open_files[fi->fh] != NULL && open_files[fi->fh]->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, fi->fh, i, 0, 0, NULL, NULL, NULL, NULL);
	return 0;
}

static int raid_5_rename(const char *from, const char *to)
{
	if (setter_function_on_server("rename", from, to, 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_unlink(const char *path)
{
	if (setter_function_on_server("unlink", path, "", 0, 0, NULL, 0, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_utimens(const char *path, const struct timespec ts[2])
{
	return 0;
}

static struct fuse_operations raid_5_oper = {
	.getattr	= raid_5_getattr,
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
		server_fds[i] = connect_to_server(server, global_storage);
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
