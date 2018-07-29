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
			printf("id: %d", i);
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
	return of->id;
}

static void remove_fd(struct open_file *of, int server_num)
{
	of->fds[server_num] = UNOPENED;
	int res = -1;
	int i = 0;
	for (; i < MAX_SERVERS; i++)
		if (!(of->fds[i] == UNSET_FD) || (of->fds[i] == UNOPENED)) res = 0;
	if (res == -1)
	{
		int id = of->id;
		free(open_files[id]);
		open_files[id] = NULL;
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
	if (strcmp(syscall, "opendir") == 0) return sizeof(DIR *) + sizeof(int);
	if (strcmp(syscall, "readdir") == 0) return sizeof(struct dirent) + sizeof(int);
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

static int pack_readdir(char *buf, DIR *dir)
{
	sprintf(buf, "5%s", "readdir");
	memcpy(buf + 9, &dir, sizeof(DIR *));
	return sizeof(DIR *) + 9;
}

static int pack_closedir(char *buf, DIR *dir)
{
	sprintf(buf, "5%s", "closedir");
	memcpy(buf + 10, &dir, sizeof(DIR *));
	return sizeof(DIR *) + 10;
}

static int pack_close(char *buf, int fd)
{
	sprintf(buf, "5%s", "close");
	memcpy(buf + 7, &fd, sizeof(int));
	return sizeof(int) + 7;
}

static int unpack(const char *syscall, char *buf, struct open_file *of, int server_num, struct stat *stbuf, DIR *dir, struct dirent *de)
{
	int rv;
	memcpy(&rv, buf, sizeof(int));
	errno = 0;
	if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));
	if (strcmp(syscall, "open") == 0) if (rv > -1) rv = add_fd(of, rv, server_num);
	if (strcmp(syscall, "lstat") == 0) memcpy(stbuf, buf + sizeof(int) * 2, sizeof(struct stat));
	if (strcmp(syscall, "opendir") == 0) memcpy(&dir, buf + sizeof(int) * 2, sizeof(DIR *));
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
									 mode_t mode, int flags, struct open_file *of,
									 const char *write_buf, size_t size, off_t offset, dev_t rdev)
{
	char buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "mkdir") == 0) send_num = pack_mkdir(buf, path, mode);
	else if (strcmp(syscall, "mknod") == 0) send_num = pack_mknod(buf, path, mode, rdev);
	else if (strcmp(syscall, "rmdir") == 0) send_num = pack_rmdir(buf, path);
	else if (strcmp(syscall, "open") == 0) send_num = pack_open(buf, path, flags, mode);
	else if (strcmp(syscall, "rename") == 0) send_num = pack_rename(buf, path, to);
	else if (strcmp(syscall, "unlink") == 0) send_num = pack_unlink(buf, path);

	int hotswap_server_num = -1;
	int rvs[servers_count];
	memset(rvs, 0, servers_count);
	int i = 0;
	for (; i < servers_count; i++)
	{
		int sent_num = send_to_server(buf, send_num, i);
		if (sent_num == -1)
		{	
			if (hotswap_server_num != -1) return SERVERS_DOWN;
			hotswap_server_num = i;
			continue;
		}
		int rec_num = 0;
		int should_receive_num = receive_num(syscall);

		char *temp_buf = buf;
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
		rvs[i] = unpack(syscall, buf, of, i, NULL, NULL, NULL);
		if (rvs[i] == -1) break;
	}

	if (hotswap_server_num != -1) hotswap(hotswap_server_num);

	return rvs[i];
}

static int getter_function_on_server(const char *syscall, const char *path, int mask, DIR *dir, int flags, struct open_file *of, int mode, struct stat *stbuf, int *server)
{
	char buf[BUFFER_SIZE];
	int send_num = 0;
	if (strcmp(syscall, "lstat") == 0) send_num = pack_lstat(buf, path);
	else if(strcmp(syscall, "open") == 0) send_num = pack_open(buf, path, flags, mode);
	else if (strcmp(syscall, "access") == 0) send_num = pack_access(buf, path, mask);
	else if (strcmp(syscall, "opendir") == 0) send_num = pack_opendir(buf, path);

	int hotswap_server_num = -1;
	int i = 0;
	int rv = -1;
	for (; i < servers_count; i++)
	{
		int sent_num = send_to_server(buf, send_num, i);
		if (sent_num == -1)
		{	
			if (hotswap_server_num != -1) return SERVERS_DOWN;
			hotswap_server_num = i;
			continue;
		}
		int rec_num = 0;
		int should_receive_num = receive_num(syscall);

		char *temp_buf = buf;
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
		rv = unpack(syscall, buf, of, i, stbuf, dir, NULL);
		if (rv > -1) break;
	}

	*server = i;
	if (hotswap_server_num != -1) hotswap(hotswap_server_num);
	return rv;
}

static int function_on_one_server(const char *syscall, DIR *dir, struct dirent *de, int id, int server)
{
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
	return unpack(syscall, buf, open_files[id], server, NULL, dir, de);
}

// static int lstat_on_server(const char *path, struct stat *stbuf);
// static int access_on_server(const char *path, int mask);

// static DIR *opendir_on_server(const char *path, int *server);
// static struct dirent *readdir_on_server(DIR *dir, int server);
// static int closedir_on_server(DIR *dir, int server);

// static int pread_on_server(int open_fd, char *read_buf, size_t size, off_t offset, int server);
// static int pwrite_on_server(const char *path, struct open_file *of, const char *write_buf, size_t size, off_t offset);
// static int truncate_on_server(const char *path, off_t size);
// static int close_on_server(int close_fd, int server);

// static int lstat_on_server(const char *path, struct stat *stbuf)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "lstat");
// 	memcpy(buf + 7, path, strlen(path) + 1);

// 	int status = send_to_server(buf, strlen(path) + 8, GET);

// 	if (status == -1) return -1;

// 	int read_size = sizeof(struct stat) + sizeof(int) + sizeof(int);
// 	if (read(server_fds[status], buf, read_size) != read_size) return -1;

// 	memcpy(stbuf, buf, sizeof(struct stat));

// 	int rv;
// 	memcpy(&rv, buf + sizeof(struct stat), sizeof(int));
// 	if (rv < 0) memcpy(&errno, buf + sizeof(struct stat) + sizeof(int), sizeof(int));

// 	return rv;
// }

// static int access_on_server(const char *path, int mask)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "access");
// 	memcpy(buf + 8, path, strlen(path) + 1);
// 	memcpy(buf + 9 + strlen(path), &mask, sizeof(int));
// 	int status = send_to_server(buf, strlen(path) + 9 + sizeof(int), GET);

// 	if (status == -1) return -1;

// 	int read_size = sizeof(int) * 2;
// 	if (read(server_fds[status], buf, read_size) != read_size) return -1;

// 	int rv;
// 	memcpy(&rv, buf, sizeof(int));
// 	if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));

// 	return rv;
// }

// static int mknod_on_server(const char *path, mode_t mode, dev_t rdev)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "mknod");
// 	memcpy(buf + 7, path, strlen(path) + 1);
// 	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
// 	memcpy(buf + 8 + strlen(path) + sizeof(mode_t), &rdev, sizeof(dev_t));
// 	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t) + sizeof(dev_t), SET);

// 	if (status != ALL) return -1;

// 	int rv;
// 	int i = 0;
// 	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 	{
// 		status = read(server_fds[i], buf, sizeof(int) * 2);
// 		if (status != sizeof(int) * 2) return -1;
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0)
// 		{
// 			memcpy(&errno, buf + sizeof(int), sizeof(int));
// 			return rv;
// 		}
// 		i++;
// 	}

// 	return rv;
// }

// static int mkdir_on_server(const char *path, mode_t mode)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "mkdir");
// 	memcpy(buf + 7, path, strlen(path) + 1);
// 	memcpy(buf + 8 + strlen(path), &mode, sizeof(mode_t));
// 	int status = send_to_server(buf, strlen(path) + 8 + sizeof(mode_t), SET);

// 	if (status != ALL) return -1;

// 	int rv;
// 	int i = 0;
// 	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 	{
// 		status = read(server_fds[i], buf, sizeof(int) * 2);
// 		if (status != sizeof(int) * 2) return -1;
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0)
// 		{
// 			memcpy(&errno, buf + sizeof(int), sizeof(int));
// 			return rv;
// 		}
// 		i++;
// 	}

// 	return rv;
// }

// static DIR *opendir_on_server(const char *path, int *server)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "opendir");
// 	memcpy(buf + 9, path, strlen(path) + 1);
// 	int status = send_to_server(buf, strlen(path) + 10, GET);

// 	if (status == -1) return NULL;

// 	*server = status;

// 	int read_size = sizeof(DIR *) + sizeof(int);
// 	if (read(server_fds[status], buf, read_size) != read_size) return NULL;

// 	DIR *dir;
// 	memcpy(&dir, buf, sizeof(DIR *));
// 	(if dir == NULL) memcpy(&errno, buf + sizeof(DIR *), sizeof(int));

// 	// err_file = fopen(global_config->err_log_path, "a");
// 	// fprintf(err_file, "opendir_on_server: Dir: %p\n", dir);
// 	// fclose(err_file);

// 	return dir;
// }

// static struct dirent *readdir_on_server(DIR *dir, int server)
// {
// 	// err_file = fopen(global_config->err_log_path, "a");
// 	// fprintf(err_file, "readdir_on_server: Dir: %p\n", dir);

// 	char buf[1024];
// 	sprintf(buf, "5%s", "readdir");
// 	memcpy(buf + 9, &dir, sizeof(DIR *));
// 	int status = send_to_server(buf, sizeof(DIR *) + 9, server);

// 	if (status != server) return NULL;

// 	int read_size = sizeof(struct dirent) + sizeof(int);
// 	if (read(server_fds[status], buf, read_size) != read_size) return NULL;

// 	if (strcmp(buf, "NULL") == 0) 
// 	{
// 		memcpy(&errno, buf + 5, sizeof(int));
// 		return NULL;
// 	}

// 	struct dirent *entry = malloc(sizeof(struct dirent));
// 	memcpy(entry, buf, sizeof(struct dirent));
// 	memcpy(&errno, buf + sizeof(struct dirent), sizeof(int));

// 	// fprintf(err_file, "readdir_on_server: Entry { d_ino: %zu, d_type: %d, d_name: %s }\n", entry->d_ino, entry->d_type, entry->d_name);
// 	// fclose(err_file);

// 	return entry;
// }

// static int closedir_on_server(DIR *dir, int server)
// {
// 	// err_file = fopen(global_config->err_log_path, "a");
// 	// fprintf(err_file, "closedir_on_server: Dir: %p\n", dir);
// 	// fclose(err_file);

// 	char buf[1024];
// 	sprintf(buf, "5%s", "closedir");
// 	memcpy(buf + 10, &dir, sizeof(DIR *));
// 	int status = send_to_server(buf, sizeof(DIR *) + 10, server);

// 	if (status != server) return -1;

// 	int read_size = sizeof(int);
// 	if (read(server_fds[status], buf, read_size) != read_size) return -1;

// 	int errno_temp;
// 	memcpy(&errno_temp, buf, sizeof(int));
// 	if (errno_temp != 0) errno = errno_temp;

// 	return 0;
// }

// static int rmdir_on_server(const char *path)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "rmdir");
// 	memcpy(buf + 7, path, strlen(path) + 1);
// 	int status = send_to_server(buf, strlen(path) + 8, SET);

// 	if (status != ALL) return -1;

// 	int rv;
// 	int i = 0;
// 	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 	{
// 		status = read(server_fds[i], buf, sizeof(int) * 2);
// 		if (status != sizeof(int) * 2) return -1;
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0)
// 		{
// 			memcpy(&errno, buf + sizeof(int), sizeof(int));
// 			return rv;
// 		}
// 		i++;
// 	}

// 	return rv;
// }

// static struct open_file *open_on_server(const char *path, int flags, mode_t mode)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "open");
// 	memcpy(buf + 6, path, strlen(path) + 1);
// 	memcpy(buf + 7 + strlen(path), &flags, sizeof(int));
// 	memcpy(buf + 7 + strlen(path) + sizeof(int), &mode, sizeof(mode_t));
// 	int status;
// 	int rv = 0;

// 	struct open_file *of;

// 	// If open is required on both servers
// 	if (flags & O_CREAT || flags & O_WRONLY)
// 	{
// 		int open_fds[MAX_SERVERS];

// 		status = send_to_server(buf, 7 + strlen(path) + sizeof(int) + sizeof(mode_t), SET);
// 		if (status != ALL) return NULL;

// 		int statuses[MAX_SERVERS];
// 		int need_to_close = 0;

// 		int i = 0;
// 		while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 		{
// 			statuses[i] = read(server_fds[i], buf + sizeof(int) * 2 * i, sizeof(int) * 2);
// 			if (statuses[i] < 0) need_to_close = 1;
// 			else memcpy(open_fds + i, buf + sizeof(int) * 2 * i, sizeof(int));
// 			i++;
// 		}

// 		if (need_to_close == 1)
// 		{
// 			for (i = 0; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
// 				if (statuses[i] > -1 && open_fds[i] > -1) close_on_server(open_fds[i], i);
// 			return NULL;
// 		}

// 		for (i = 0; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
// 			if (open_fds[i] < 0) memcpy(&errno, buf + sizeof(int) * 2 * i + sizeof(int), sizeof(int));

// 		of = add_open_file(path, open_fds, i);
// 	}
// 	else
// 	{
// 		status = send_to_server(buf, 7 + strlen(path) + sizeof(int) + sizeof(mode_t), GET);
// 		if (status == -1) return NULL;
		
// 		int read_size = sizeof(int) * 2;
// 		if (read(server_fds[status], buf, read_size) != read_size) return NULL;
		
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0) memcpy(&errno, buf + sizeof(int), sizeof(int));

// 		int open_fds[MAX_SERVERS];
// 		int i = 0;
// 		for (; server_fds[i] != UNSET_FD || i != MAX_SERVERS; i++)
// 		{
// 			if (i == status) open_fds[i] = rv;
// 			else open_fds[i] = UNOPENED;
// 		}

// 		of = add_open_file(path, open_fds, i);
// 	}

// 	return of;
// }

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

// static int close_on_server(int close_fd, int server)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "close");
// 	memcpy(buf + 7, &close_fd, sizeof(int));
// 	int status = send_to_server(buf, sizeof(int) + 7, server);

// 	if (status != server) return -1;

// 	int read_size = sizeof(int);
// 	if (read(server_fds[server], buf, read_size) != read_size) return -1;

// 	int errno_temp;
// 	memcpy(&errno_temp, buf, sizeof(int));
// 	if (errno_temp != 0) errno = errno_temp;

// 	return 0;
// }

// static int rename_on_server(const char *from, const char *to)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "rename");
// 	memcpy(buf + 8, from, strlen(from) + 1);
// 	memcpy(buf + 9 + strlen(from), to, strlen(to) + 1);
// 	int status = send_to_server(buf, strlen(from) + strlen(to) + 10, SET);

// 	if (status != ALL) return -1;

// 	int rv;
// 	int i = 0;
// 	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 	{
// 		status = read(server_fds[i], buf, sizeof(int) * 2);
// 		if (status != sizeof(int) * 2) return -1;
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0)
// 		{
// 			memcpy(&errno, buf + sizeof(int), sizeof(int));
// 			return rv;
// 		}
// 		i++;
// 	}

// 	return rv;
// }

// static int unlink_on_server(const char *path)
// {
// 	char buf[1024];
// 	sprintf(buf, "5%s", "unlink");
// 	memcpy(buf + 8, path, strlen(path) + 1);
// 	int status = send_to_server(buf, strlen(path) + 9, SET);

// 	if (status != ALL) return -1;

// 	int rv;
// 	int i = 0;
// 	while (server_fds[i] != UNSET_FD || i != MAX_SERVERS)
// 	{
// 		status = read(server_fds[i], buf, sizeof(int) * 2);
// 		if (status != sizeof(int) * 2) return -1;
// 		memcpy(&rv, buf, sizeof(int));
// 		if (rv < 0)
// 		{
// 			memcpy(&errno, buf + sizeof(int), sizeof(int));
// 			return rv;
// 		}
// 		i++;
// 	}

// 	return rv;
// }

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
	if (getter_function_on_server("lstat", path, 0, NULL, 0, NULL, 0, stbuf, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_access(const char *path, int mask)
{
	print2("access", path);
	if (getter_function_on_server("access", path, mask, NULL, 0, NULL, 0, NULL, NULL) == -1) return -errno;
	return 0;
}

static int raid_5_mknod(const char *path, mode_t mode, dev_t rdev)
{
	print2("mknod", path);
	if (setter_function_on_server("mknod", path, "", mode, 0, NULL, NULL, 0, 0, rdev) == -1) return -errno;
	return 0;
}

static int raid_5_mkdir(const char *path, mode_t mode)
{
	print2("mkdir", path);
	if (setter_function_on_server("mkdir", path, "", mode, 0, NULL, NULL, 0, 0, 0) == -1) return -errno;
	return 0;
}

static int raid_5_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	print2("readdir", path);

	DIR *dp = NULL;
	struct dirent *de = NULL;

	(void) offset;
	(void) fi;

	int server;
	if (getter_function_on_server("opendir", path, 0, dp, 0, NULL, 0, NULL, &server) == -1) return -errno;

	function_on_one_server("readdir", dp, de, 0, server);
	while (function_on_one_server("readdir", dp, de, 0, server) != -1)
	{
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0)) break;
		free(de);
	}
	free(de);

	function_on_one_server("closedir", dp, NULL, 0, server);
	return 0;
}

static int raid_5_rmdir(const char *path)
{
	print2("rmdir", path);
	if (setter_function_on_server("rmdir", path, "", 0, 0, NULL, NULL, 0, 0, 0) == -1) return -errno;
	return 0;
}

static int raid_5_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	print2("create", path);
	struct open_file *of = init_open_file();
	int res = setter_function_on_server("open", path, "", mode, fi->flags, of, NULL, 0, 0, 0);
	fi->fh = res;
	if (res < 0) return -errno;
	return 0;
}

static int raid_5_open(const char *path, struct fuse_file_info *fi)
{
	print2("open", path);
	
	struct open_file *of = init_open_file();
	int res;
	if (fi->flags & (O_WRONLY | O_CREAT))
		res = setter_function_on_server("open", path, "", 0, fi->flags, of, NULL, 0, 0, 0);
	else
		res = getter_function_on_server("open", path, 0, NULL, fi->flags, of, 0, NULL, NULL);
	fi->fh = res;
	if (res < 0) return -errno;
	return 0;
	// if (of->fd_1 >= 0 && of->fd_2 >= 0) return 0;
	// if (of->fd_1 >= 0 && of->fd_2 == UNOPENED) return 0;
	// if (of->fd_1 == UNOPENED && of->fd_2 >= 0) return 0;
	// if ((of->fd_1 == UNOPENED || of->fd_2 == UNOPENED) && (of->fd_1 == FILE_DAMAGED || of->fd_2 == FILE_DAMAGED))
	// {
	// 	raid_5_release(path, fi);
	// 	of = open_on_server(path, fi->flags | O_WRONLY, 0000);
	// }		
	// if (of->fd_1 == FILE_DAMAGED && of->fd_2 != FILE_DAMAGED) copy_file(of, 1, 2);
	// else if (of->fd_1 != FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) copy_file(of, 2, 1);
	// else if (of->fd_1 == FILE_DAMAGED && of->fd_2 == FILE_DAMAGED) return -EBADFD;
	// else return -errno;
	// raid_5_release(path, fi);
	// return raid_5_open(path, fi);
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

	int i = 0;
	for (; i < servers_count; i++)
		if (open_files[fi->fh]->fds[i] != UNOPENED)
			function_on_one_server("close", NULL, NULL, fi->fh, i);

	print_open_files();

	return 0;
}

static int raid_5_rename(const char *from, const char *to)
{
	print3("rename", from, to);

	if (setter_function_on_server("raname", from, to, 0, 0, NULL, NULL, 0, 0, 0) == -1) return -errno;
	return 0;
}

static int raid_5_unlink(const char *path)
{
	print2("unlink", path);
	if (setter_function_on_server("unlink", path, "", 0, 0, NULL, NULL, 0, 0, 0) == -1) return -errno;
	return 0;
}

static struct fuse_operations raid_5_oper = {
	.getattr	= raid_5_getattr,
	.access		= raid_5_access,
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

	// char *info_msg = (char*)malloc(strlen(argv[2]) + 32);
	// sprintf(info_msg, "Mounting directory '%s'", argv[2]);
	// nrf_print_info(info_msg);
	// free(info_msg);

	umask(0);
	return fuse_main(argc, argv, &raid_5_oper, NULL);
}
