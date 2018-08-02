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

#define BACKLOG 10
#define PATH_LEN 256
#define BUF_SIZE 1024
#define HASH_SIZE 16
#define FILE_DAMAGED -93

char *global_path;

int hash_file(char *buf, int fd, int size)
{
	memset(buf, 0, HASH_SIZE);

	int offset = 0;
	char *temp_buf = malloc(HASH_SIZE);
	int rv;
	while (size >= HASH_SIZE)
	{
		rv = pread(fd, temp_buf, HASH_SIZE, offset);

		size -= HASH_SIZE;
		offset += HASH_SIZE;
		int i = 0;
		for (; i < HASH_SIZE; i++)
			buf[i] ^= temp_buf[i];
	}

	rv = pread(fd, temp_buf, size, offset);
	int i = 0;
	for (; i < size; i++)
		buf[i] ^= temp_buf[i];

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
	int xattr = lgetxattr(path, "user.checksum", checksum, HASH_SIZE);

	if (xattr < 0) printf("error: %s\n", strerror(errno));

	int fd = open(path, O_RDONLY);
	char *new_checksum = malloc(HASH_SIZE);
	hash_file(new_checksum, fd, size);
	close(fd);
	
	int cmp = memcmp(checksum, new_checksum, HASH_SIZE);

	free(checksum);
	free(new_checksum);
	if (cmp == 0) return 0;
	else return -1;
}

int call_raid_1_function(int client_fd, char *buf)
{
	// print1(buf);

	if (strcmp(buf, "lstat") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		// print2(buf, path);

		struct stat *stat = malloc(sizeof(struct stat));
		int rv = lstat(path, stat);
		// nrf_print_note("Stat after lstat");
		// nrf_print_struct(stat, "stat", 1, 1);
		// print2i(buf, rv);
		// print2i(buf, errno);
		free(path);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		memcpy(buf + sizeof(int) * 2, stat, sizeof(struct stat));

		free(stat);

		return sizeof(struct stat) + sizeof(int) * 2;
	}
	else if (strcmp(buf, "access") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		// print2(buf, path);

		int mask;
		memcpy(&mask, buf + 8 + strlen(buf + 7), sizeof(int));

		int rv = access(path, mask);

		// print2i(buf, rv);
		// print2i(buf, errno);

		free(path);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "opendir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';
		
		strcat(path, global_path);
		strcat(path, buf + 8);

		// print2(buf, path);

		DIR *dir = opendir(path);

		free(path);

		int rv;
		if (dir == NULL) rv = -1;
		else rv = 0;

		// print2p(buf, dir);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		memcpy(buf + sizeof(int) * 2, &dir, sizeof(DIR *));

		// print2p("opendir", (DIR *)(*(uint64_t *)(buf + sizeof(int) * 2)));

		return sizeof(DIR *) + sizeof(int) * 2;
	}
	else if(strcmp(buf, "readdir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 8, sizeof(DIR *));

		// print2p(buf, dir);

		struct dirent *de = readdir(dir);

		// nrf_print_note("after syscall readdir");

		buf--;

		int rv;
		if (de != NULL)
		{
			// nrf_print_struct(de, "dirent", 1, 1);
			rv = 0;
			memcpy(buf, &rv, sizeof(int));
			memcpy(buf + sizeof(int), &errno, sizeof(int));
			memcpy(buf + sizeof(int) * 2, de, sizeof(struct dirent));
		}
		else
		{
			rv = -1;
			// nrf_print_warning("We have \033[31;1mNULL\033[0m");
			memcpy(buf, &rv, sizeof(int));
			memcpy(buf + sizeof(int), &errno, sizeof(int));
			memset(buf + sizeof(int) * 2, 0, sizeof(struct dirent));
		}

		return sizeof(struct dirent) + sizeof(int) * 2;
	}
	else if (strcmp(buf, "closedir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 9, sizeof(DIR *));

		// print2p(buf, dir);

		int rv = closedir(dir);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "mknod") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		// print2(buf, path);

		mode_t mode;
		dev_t rdev;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));
		memcpy(&rdev, buf + 7 + strlen(buf + 6) + sizeof(mode_t), sizeof(dev_t));

		int rv = mknod(path, mode, rdev);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "mkdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		// print2(buf, path);

		mode_t mode;
		memcpy(&mode, buf + 7 + strlen(buf + 6), sizeof(mode_t));

		int rv = mkdir(path, mode);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "open") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 5);

		// print2(buf, path);

		int flags;
		memcpy(&flags, buf + 6 + strlen(buf + 5), sizeof(int));

		mode_t mode;
		memcpy(&mode, buf + 6 + strlen(buf + 5) + sizeof(int), sizeof(mode_t));

		int rv = open(path, flags, mode);

		// print2i(buf, rv);

		free(path);

		buf--;

		if (buf[0] == '1' && rv >= 0 && check_file(path) < 0) rv = FILE_DAMAGED;
		
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "unlink") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		// print2(buf, path);

		int rv = unlink(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "rmdir") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		// print2(buf, path);

		int rv = rmdir(path);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
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

		// print3(buf, from, to);

		int rv = rename(from, to);

		free(from);
		free(to);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "write") == 0)
	{
		char write_buf[BUF_SIZE];

		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		int fd;
		int offset;
		int size;
		memcpy(&fd, buf + 7 + strlen(buf + 6), sizeof(int));
		memcpy(&offset, buf + 7 + strlen(buf + 6) + sizeof(int), sizeof(int));
		memcpy(&size, buf + 7 + strlen(buf + 6) + sizeof(int) * 2, sizeof(int));

		memcpy(write_buf, buf + 7 + strlen(buf + 6) + sizeof(int) * 3, size);

		int rv = pwrite(fd, write_buf, size, offset);

		buf--;

		if (buf[0] == '1')
		{
			struct stat st;
			fstat(fd, &st);
			int file_size = st.st_size;

			int read_fd = open(path, O_RDONLY);
			char *checksum = malloc(HASH_SIZE);
			hash_file(checksum, read_fd, file_size);
			close(read_fd);

			int xattr = fsetxattr(fd, "user.checksum", checksum, HASH_SIZE, 0);

			if (xattr < 0) printf("error: %s\n", strerror(errno));
		}

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "read") == 0)
	{
		char read_buf[BUF_SIZE];

		int fd;
		int offset;
		int size;
		memcpy(&fd, buf + 5, sizeof(int));
		memcpy(&offset, buf + 5 + sizeof(int), sizeof(int));
		memcpy(&size, buf + 5 + sizeof(int) * 2, sizeof(int));

		int rv = pread(fd, read_buf, size, offset);

		buf--;

		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		if (rv > 0)
		{
			memcpy(buf + sizeof(int) * 2, read_buf, rv);
			return sizeof(int) * 2 + rv;
		}
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "truncate") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 9);

		// print2(buf, path);

		off_t size;
		memcpy(&size, buf + 10 + strlen(buf + 9), sizeof(off_t));

		int rv = truncate(path, size);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "close") == 0)
	{
		int fd;
		memcpy(&fd, buf + 6, sizeof(int));

		// print2i(buf, fd);

		int rv = close(fd);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
	}
	else if (strcmp(buf, "utimes") == 0)
	{
		char *path = malloc(PATH_LEN);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		// print2(buf, path);

		struct timeval tv[2];
		memcpy(tv, buf + 8 + strlen(buf + 7), sizeof(struct timeval) * 2);

		int rv = utimes(path, tv);

		free(path);

		buf--;
		memcpy(buf, &rv, sizeof(int));
		memcpy(buf + sizeof(int), &errno, sizeof(int));
		return sizeof(int) * 2;
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
        	if (buf[0] == '1' || buf[0] == '5')
        	{
        		data_size = call_raid_1_function(client_fd, buf + 1);
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