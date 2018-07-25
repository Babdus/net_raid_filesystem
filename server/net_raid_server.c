#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>

#include "../nrf_print.h"
#include "raid_1_server.h"

#define BACKLOG 10
#define PATH_LEN 256
#define BUF_SIZE 1024
#define HASH_SIZE 16
#define FILE_DAMAGED -93

char *global_path;

static void print1(const char *syscall)
{
	char *info_msg = (char*)malloc(strlen(syscall) + 28);
	sprintf(info_msg, "We have syscall %s!", syscall);
	nrf_print_note(info_msg);
	free(info_msg);
}

static void print2(const char *syscall, const char *path)
{
	char *info_msg = (char*)malloc(strlen(path) + strlen(syscall) + 28);
	sprintf(info_msg, "raid_1_%s: Path is %s", syscall, path);
	nrf_print_info(info_msg);
	free(info_msg);
}

static void print2p(const char *syscall, void *pointer)
{
	char *info_msg = (char*)malloc(strlen(syscall) + 41);
	sprintf(info_msg, "raid_1_%s: Pointer is %p", syscall, pointer);
	nrf_print_info(info_msg);
	free(info_msg);
}

static void print2i(const char *syscall, int num)
{
	char *info_msg = (char*)malloc(strlen(syscall) + 41);
	sprintf(info_msg, "raid_1_%s: Number is %d", syscall, num);
	nrf_print_value(info_msg);
	free(info_msg);
}

static void print4siii(const char *syscall, int num1, int num2, int num3)
{
	char *info_msg = (char*)malloc(strlen(syscall) + 100);
	sprintf(info_msg, "raid_1_%s: Number 1 is %d, nuber 2 is %d and number 3 is %d", syscall, num1, num2, num3);
	nrf_print_value(info_msg);
	free(info_msg);
}

static void print3(const char *syscall, const char *from, const char *to)
{
	char *info_msg = (char*)malloc(strlen(from) + strlen(to) + strlen(syscall) + 19);
	sprintf(info_msg, "raid_1_%s: From %s to %s", syscall, from, to);
	nrf_print_info(info_msg);
	free(info_msg);
}

int hash_file(char *buf, int fd, int size)
{
	memset(buf, 0, HASH_SIZE);

	printf("checksum at begining: ");
	int j = 0;
	for (;j < HASH_SIZE; j++)
		printf("%0x ", buf[j]);
	printf("\n");

	int offset = 0;
	char *temp_buf = malloc(HASH_SIZE);
	int rv;
	while (size >= HASH_SIZE)
	{
		rv = pread(fd, temp_buf, HASH_SIZE, offset);

		printf("mid, temp: %.16s\n", temp_buf);
		printf("temp at mid: ");
		j = 0;
		for (;j < HASH_SIZE; j++)
			printf("%0x ", temp_buf[j]);
		printf("\n");
		printf("rv %d\n", rv);

		printf("%s\n", strerror(errno));

		size -= HASH_SIZE;
		offset += HASH_SIZE;
		int i = 0;
		for (; i < HASH_SIZE; i++)
			buf[i] ^= temp_buf[i];

		printf("checksum at mid: ");
		j = 0;
		for (;j < HASH_SIZE; j++)
			printf("%0x ", buf[j]);
		printf("\n");
	}

	printf("checksum before end: ");
	j = 0;
	for (;j < HASH_SIZE; j++)
		printf("%0x ", buf[j]);
	printf("\n");

	rv = pread(fd, temp_buf, size, offset);
	int i = 0;
	for (; i < size; i++)
		buf[i] ^= temp_buf[i];

	printf("checksum in the end: ");
	j = 0;
	for (;j < HASH_SIZE; j++)
		printf("%0x ", buf[j]);
	printf("\n");

	free(temp_buf);
	return rv;
}

int check_file(const char *path)
{
	struct stat st;
	stat(path, &st);
	int size = st.st_size;

	if (size == 0) return 0;

	char *checksum = malloc(HASH_SIZE);
	int xattr = lgetxattr(path, "checksum", checksum, HASH_SIZE);

	if (xattr < 0) printf("error: %s\n", strerror(errno));

	printf("checksum: ");
	int i = 0;
	for (;i < HASH_SIZE; i++)
		printf("%0x ", checksum[i]);
	printf("\n");

	

	printf("file size: %d\n", size);

	int fd = open(path, O_RDONLY);
	char *new_checksum = malloc(HASH_SIZE);
	hash_file(new_checksum, fd, size);
	close(fd);

	printf("new checksum: ");
	i = 0;
	for (;i < HASH_SIZE; i++)
		printf("%0x ", new_checksum[i]);
	printf("\n");
	
	int cmp = memcmp(checksum, new_checksum, HASH_SIZE);

	printf("cmp: %d\n", cmp);

	free(checksum);
	free(new_checksum);
	if (cmp == 0) return 0;
	else return -1;
}

int call_raid_5_function(int client_fd, char *buf)
{
	print1(buf);

	if (strcmp(buf, "lstat") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		struct stat *stat = malloc(sizeof(struct stat));
		int rv = lstat(path, stat);
		nrf_print_note("Stat after lstat");
		nrf_print_struct(stat, "stat", 1, 1);
		print2i(buf, rv);
		print2i(buf, errno);
		free(path);

		buf--;

		memcpy(buf, stat, sizeof(struct stat));
		memcpy(buf + sizeof(struct stat), &rv, sizeof(int));
		memcpy(buf + sizeof(struct stat) + sizeof(int), &errno, sizeof(int));

		return sizeof(struct stat) + sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "access") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int mask;
		memcpy(&mask, buf + 8 + strlen(buf + 7), sizeof(int));

		int rv = access(path, mask);

		free(path);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "opendir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';
		
		strcat(path, global_path);
		strcat(path, buf + 8);

		print2(buf, path);

		DIR *dir = opendir(path);

		free(path);

		print2p(buf, dir);

		buf--;
		memcpy(buf, &dir, sizeof(DIR *));
		memcpy(buf + sizeof(DIR *), &errno, sizeof(int));

		print2p("opendir", (DIR *)(*(uint64_t *)buf));

		return sizeof(DIR *) + sizeof(int);
	}
	else if(strcmp(buf, "readdir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 8, sizeof(DIR *));

		print2p(buf, dir);

		struct dirent *de = readdir(dir);

		nrf_print_note("after syscall readdir");

		buf--;

		if (de != NULL)
		{
			nrf_print_struct(de, "dirent", 1, 1);
			memcpy(buf, de, sizeof(struct dirent));
			memcpy(buf + sizeof(struct dirent), &errno, sizeof(int));
		}
		else
		{
			nrf_print_warning("We have \033[31;1mNULL\033[0m");
			sprintf(buf, "NULL");
			memcpy(buf + 5, &errno, sizeof(int));
		}

		return sizeof(struct dirent) + sizeof(int);
	}
	else if (strcmp(buf, "closedir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 9, sizeof(DIR *));

		print2p(buf, dir);

		closedir(dir);

		buf--;

		memcpy(buf, &errno, sizeof(int));

		return sizeof(int);
	}
	else if (strcmp(buf, "mknod") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		mode_t mode;
		dev_t rdev;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));
		memcpy(&rdev, buf + 7 + strlen(buf + 6) + sizeof(mode_t), sizeof(dev_t));

		int rv = mknod(path, mode, rdev);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "mkdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		mode_t mode;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));

		int rv = mkdir(path, mode);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "open") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 5);

		print2(buf, path);

		int flags;
		memcpy(&flags, buf + 6 + strlen(buf + 5), sizeof(int));

		mode_t mode;
		memcpy(&mode, buf + 6 + strlen(buf + 5) + sizeof(int), sizeof(mode_t));

		int rv = open(path, flags, mode);

		// if (rv >= 0 && check_file(path) < 0) rv = FILE_DAMAGED;

		print2i(buf, rv);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "unlink") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int rv = unlink(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "rmdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		int rv = rmdir(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "rename") == 0)
	{
		char *from = malloc(PATH_LEN);
		char *to = malloc(PATH_LEN);
		from[0] = '\0';
		to[0] = '\0';

		strcat(from, global_path);
		strcat(from, buf + 7);

		strcat(to, global_path);
		strcat(to, buf + 8 + strlen(buf + 7));

		print3(buf, from, to);

		int rv = rename(from, to);

		free(from);
		free(to);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "pread") == 0)
	{	
		int fd;
		size_t size;
		off_t offset;
		memcpy(&fd, buf + 6, sizeof(int));
		memcpy(&size, buf + 6 + sizeof(int), sizeof(size_t));
		memcpy(&offset, buf + 6 + sizeof(int) + sizeof(size_t), sizeof(off_t));

		print4siii(buf, fd, size, offset);

		char *read_buf = malloc(size);

		int rv = pread(fd, read_buf, size, offset);

		print2i(buf, rv);
		print2(buf, read_buf);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));

		write(client_fd, buf, sizeof(int) * 2);

		while (rv > 0)
		{
			memcpy(buf, read_buf, BUF_SIZE);
			read_buf += BUF_SIZE;
			rv -= BUF_SIZE;

			write(client_fd, buf, BUF_SIZE);
		}

		return 0;
	}
	else if (strcmp(buf, "pwrite") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int fd;
		size_t size;
		off_t offset;
		memcpy(&fd, buf + 8 + strlen(buf + 7), sizeof(int));
		memcpy(&size, buf + 8 + strlen(buf + 7) + sizeof(int), sizeof(size_t));
		memcpy(&offset, buf + 8 + strlen(buf + 7) + sizeof(int) + sizeof(size_t), sizeof(off_t));

		print4siii(buf, fd, size, offset);

		buf--;
		char *write_buf = malloc(size);
		char *wb_p = write_buf;

		int int_size = (int)size;

		while (int_size > 0)
		{
			int read_size = BUF_SIZE;
			if (int_size < BUF_SIZE) read_size = size;
			read(client_fd, buf, read_size);
			while (strlen(buf) < 1)
				read(client_fd, buf, read_size);
			memcpy(wb_p, buf, read_size);
			wb_p += BUF_SIZE;
			int_size -= BUF_SIZE;
		}

		int rv = pwrite(fd, write_buf, size, offset);

		// struct stat st;
		// fstat(fd, &st);
		// int file_size = st.st_size;

		// printf("file_size %d\n", file_size);

		// int read_fd = open(path, O_RDONLY);
		// char *checksum = malloc(HASH_SIZE);
		// hash_file(checksum, read_fd, file_size);
		// close(read_fd);
		
		// printf("checksum: ");
		// int i = 0;
		// for (;i < HASH_SIZE; i++)
		// 	printf("%x ", checksum[i]);
		// printf("\n");

		// int xattr = fsetxattr(fd, "checksum", checksum, HASH_SIZE, 0);

		// if (xattr < 0) printf("error: %s\n", strerror(errno));

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "truncate") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 9);

		print2(buf, path);

		off_t size;
		memcpy(&size, buf + 10 + strlen(buf + 9), sizeof(off_t));

		int rv = truncate(path, size);

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "close") == 0)
	{
		int fd;
		memcpy(&fd, buf + 6, sizeof(int));

		print2i(buf, fd);

		close(fd);

		buf--;
		memcpy(buf, &errno, sizeof(int));
		return sizeof(int);
	}
	else
	{
		return 0;
	}
}

int call_raid_1_function(int client_fd, char *buf)
{
	print1(buf);

	if (strcmp(buf, "lstat") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		struct stat *stat = malloc(sizeof(struct stat));
		int rv = lstat(path, stat);
		nrf_print_note("Stat after lstat");
		nrf_print_struct(stat, "stat", 1, 1);
		print2i(buf, rv);
		print2i(buf, errno);
		free(path);

		buf--;

		memcpy(buf, stat, sizeof(struct stat));
		memcpy(buf + sizeof(struct stat), &rv, sizeof(int));
		memcpy(buf + sizeof(struct stat) + sizeof(int), &errno, sizeof(int));

		return sizeof(struct stat) + sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "access") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int mask;
		memcpy(&mask, buf + 8 + strlen(buf + 7), sizeof(int));

		int rv = access(path, mask);

		free(path);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "opendir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';
		
		strcat(path, global_path);
		strcat(path, buf + 8);

		print2(buf, path);

		DIR *dir = opendir(path);

		free(path);

		print2p(buf, dir);

		buf--;
		memcpy(buf, &dir, sizeof(DIR *));
		memcpy(buf + sizeof(DIR *), &errno, sizeof(int));

		print2p("opendir", (DIR *)(*(uint64_t *)buf));

		return sizeof(DIR *) + sizeof(int);
	}
	else if(strcmp(buf, "readdir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 8, sizeof(DIR *));

		print2p(buf, dir);

		struct dirent *de = readdir(dir);

		nrf_print_note("after syscall readdir");

		buf--;

		if (de != NULL)
		{
			nrf_print_struct(de, "dirent", 1, 1);
			memcpy(buf, de, sizeof(struct dirent));
			memcpy(buf + sizeof(struct dirent), &errno, sizeof(int));
		}
		else
		{
			nrf_print_warning("We have \033[31;1mNULL\033[0m");
			sprintf(buf, "NULL");
			memcpy(buf + 5, &errno, sizeof(int));
		}

		return sizeof(struct dirent) + sizeof(int);
	}
	else if (strcmp(buf, "closedir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 9, sizeof(DIR *));

		print2p(buf, dir);

		closedir(dir);

		buf--;

		memcpy(buf, &errno, sizeof(int));

		return sizeof(int);
	}
	else if (strcmp(buf, "mknod") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		mode_t mode;
		dev_t rdev;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));
		memcpy(&rdev, buf + 7 + strlen(buf + 6) + sizeof(mode_t), sizeof(dev_t));

		int rv = mknod(path, mode, rdev);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "mkdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		mode_t mode;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));

		int rv = mkdir(path, mode);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "open") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 5);

		print2(buf, path);

		int flags;
		memcpy(&flags, buf + 6 + strlen(buf + 5), sizeof(int));

		mode_t mode;
		memcpy(&mode, buf + 6 + strlen(buf + 5) + sizeof(int), sizeof(mode_t));

		int rv = open(path, flags, mode);

		// if (rv >= 0 && check_file(path) < 0) rv = FILE_DAMAGED;

		print2i(buf, rv);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "unlink") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int rv = unlink(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "rmdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		int rv = rmdir(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "rename") == 0)
	{
		char *from = malloc(PATH_LEN);
		char *to = malloc(PATH_LEN);
		from[0] = '\0';
		to[0] = '\0';

		strcat(from, global_path);
		strcat(from, buf + 7);

		strcat(to, global_path);
		strcat(to, buf + 8 + strlen(buf + 7));

		print3(buf, from, to);

		int rv = rename(from, to);

		free(from);
		free(to);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) + sizeof(int);
	}
	else if (strcmp(buf, "pread") == 0)
	{	
		int fd;
		size_t size;
		off_t offset;
		memcpy(&fd, buf + 6, sizeof(int));
		memcpy(&size, buf + 6 + sizeof(int), sizeof(size_t));
		memcpy(&offset, buf + 6 + sizeof(int) + sizeof(size_t), sizeof(off_t));

		print4siii(buf, fd, size, offset);

		char *read_buf = malloc(size);

		int rv = pread(fd, read_buf, size, offset);

		print2i(buf, rv);
		print2(buf, read_buf);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));

		write(client_fd, buf, sizeof(int) * 2);

		while (rv > 0)
		{
			memcpy(buf, read_buf, BUF_SIZE);
			read_buf += BUF_SIZE;
			rv -= BUF_SIZE;

			write(client_fd, buf, BUF_SIZE);
		}

		return 0;
	}
	else if (strcmp(buf, "pwrite") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int fd;
		size_t size;
		off_t offset;
		memcpy(&fd, buf + 8 + strlen(buf + 7), sizeof(int));
		memcpy(&size, buf + 8 + strlen(buf + 7) + sizeof(int), sizeof(size_t));
		memcpy(&offset, buf + 8 + strlen(buf + 7) + sizeof(int) + sizeof(size_t), sizeof(off_t));

		print4siii(buf, fd, size, offset);

		buf--;
		char *write_buf = malloc(size);
		char *wb_p = write_buf;

		int int_size = (int)size;

		while (int_size > 0)
		{
			int read_size = BUF_SIZE;
			if (int_size < BUF_SIZE) read_size = size;
			read(client_fd, buf, read_size);
			while (strlen(buf) < 1)
				read(client_fd, buf, read_size);
			memcpy(wb_p, buf, read_size);
			wb_p += BUF_SIZE;
			int_size -= BUF_SIZE;
		}

		int rv = pwrite(fd, write_buf, size, offset);

		// struct stat st;
		// fstat(fd, &st);
		// int file_size = st.st_size;

		// printf("file_size %d\n", file_size);

		// int read_fd = open(path, O_RDONLY);
		// char *checksum = malloc(HASH_SIZE);
		// hash_file(checksum, read_fd, file_size);
		// close(read_fd);
		
		// printf("checksum: ");
		// int i = 0;
		// for (;i < HASH_SIZE; i++)
		// 	printf("%x ", checksum[i]);
		// printf("\n");

		// int xattr = fsetxattr(fd, "checksum", checksum, HASH_SIZE, 0);

		// if (xattr < 0) printf("error: %s\n", strerror(errno));

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "truncate") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 9);

		print2(buf, path);

		off_t size;
		memcpy(&size, buf + 10 + strlen(buf + 9), sizeof(off_t));

		int rv = truncate(path, size);

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "close") == 0)
	{
		int fd;
		memcpy(&fd, buf + 6, sizeof(int));

		print2i(buf, fd);

		close(fd);

		buf--;
		memcpy(buf, &errno, sizeof(int));
		return sizeof(int);
	}
	else
	{
		return 0;
	}
}

void client_handler(int client_fd)
{
    char buf[BUF_SIZE];
    int data_size;
    while (1) {
        data_size = read (client_fd, &buf, BUF_SIZE);
        if (data_size <= 0)
            break;

        if (strlen(buf) > 0)
        {
        	if (buf[0] == '1')
        	{
        		data_size = call_raid_1_function(client_fd, buf + 1);

        		printf("\33[35mbuf: %s\n\33[0m", buf);
        		// write(1, buf, data_size);
        		printf("\n");
        	}
        	else if (buf[0] == '5')
        	{
        		data_size = call_raid_5_function(client_fd, buf + 1);

        		printf("\33[35mbuf: %s\n\33[0m", buf);
        		// write(1, buf, data_size);
        		printf("\n");
        	}
        	else
        	{
        		char *info_msg = (char*)malloc(31);
				sprintf(info_msg, "Unknown raid number %c", buf[0]);
				nrf_print_error(info_msg);
				free(info_msg);
        	}
        }
        write (client_fd, &buf, data_size);
    }
    close(client_fd);
}

int main(int argc, char **argv)
{
	char *info_msg;
	if (argc < 4)
	{
		nrf_print_error("Not enough number of arguments has been passed");
		nrf_print_note("./net_raid_server [ip] [port] [path/to/dir]");
		return -1;
	}

	char *ip = argv[1];
	int port = (int)strtol(argv[2], (char **)NULL, 10);

	if (port == 0)
	{
		nrf_print_error("Port is malformatted");
		return -1;
	}

	global_path = argv[3];

	int server_fd, client_fd;
	struct sockaddr_in addr;
	struct sockaddr_in peer_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd < 0)
	{
		nrf_print_error(strerror(errno));
		close(server_fd);
		return -1;
	}
	nrf_print_info("Socket created");

	int optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	int status = inet_aton(ip, &(addr.sin_addr));
	if (status == 0)
	{
		info_msg = (char*)malloc(strlen(ip) + 30);
		sprintf(info_msg, "Invalid ip address %s", ip);
		nrf_print_error(info_msg);
		free(info_msg);
		close(server_fd);
		return -1;
	}

	status = bind(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
	if (status < 0)
	{
		nrf_print_error(strerror(errno));
		close(server_fd);
		return -1;
	}

	info_msg = (char*)malloc(strlen(ip) + 45);
	sprintf(info_msg, "Socket is binded to address %s:%d", ip, port);
	nrf_print_info(info_msg);
	free(info_msg);

	status = listen(server_fd, BACKLOG);
	if (status < 0)
	{
		nrf_print_error(strerror(errno));
		close(server_fd);
		return -1;
	}

	info_msg = (char*)malloc(strlen(ip) + 45);
	sprintf(info_msg, "Socket is listening on address %s:%d", ip, port);
	nrf_print_info(info_msg);
	free(info_msg);
	
	while (1) 
	{
		socklen_t peer_addr_size = sizeof(struct sockaddr_in);
		client_fd = accept(server_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
		if (client_fd < 0)
		{
			nrf_print_error(strerror(errno));
			close(client_fd);
			continue;
		}
		char *client_ip = inet_ntoa(((struct sockaddr_in) peer_addr).sin_addr);
		int client_port = ((struct sockaddr_in) peer_addr).sin_port;

		info_msg = (char*)malloc(strlen(client_ip) + 38);
		sprintf(info_msg, "Client %s:%d is connected", client_ip, client_port);
		nrf_print_success(info_msg);
		free(info_msg);

		switch(fork()) {
			case -1:
				exit(100);
			case 0:
				close(server_fd);
				client_handler(client_fd);
				exit(0);
			default:
				close(client_fd);
		}
	}
	close(server_fd);
}