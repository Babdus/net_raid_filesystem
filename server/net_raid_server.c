#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
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
	char *info_msg = (char*)malloc(sizeof(void *) + strlen(syscall) + 31);
	sprintf(info_msg, "raid_1_%s: Pointer is %p", syscall, pointer);
	nrf_print_info(info_msg);
	free(info_msg);
}

// static void print3(const char *syscall, const char *from, const char *to)
// {
// 	char *info_msg = (char*)malloc(strlen(from) + strlen(to) + strlen(syscall) + 19);
// 	sprintf(info_msg, "raid_1_%s: From %s to %s", syscall, from, to);
// 	nrf_print_info(info_msg);
// 	free(info_msg);
// }

int call_raid_1_function(char *buf)
{
	print1(buf);

	if (strcmp(buf, "lstat") == 0)
	{
		char *path = malloc(256);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		struct stat *stat = malloc(sizeof(struct stat));
		int rv = lstat(path, stat);
		nrf_print_note("Stat after lstat");
		nrf_print_struct(stat, "stat", 1, 1);
		free(path);

		buf--;

		memcpy(buf, stat, sizeof(struct stat));
		memcpy(buf + sizeof(struct stat), &rv, sizeof(int));
		return sizeof(struct stat) + sizeof(int);
	}
	else if (strcmp(buf, "access") == 0)
	{
		char *path = malloc(256);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 7);

		print2(buf, path);

		int mask;
		memcpy(&mask, buf + 8 + strlen(path), sizeof(int));

		int rv = access(path, mask);

		free(path);

		memcpy(buf, &rv, sizeof(int));
		return sizeof(int);
	}
	else if (strcmp(buf, "opendir") == 0)
	{
		char *path = malloc(256);
		path[0] = '\0';
		
		strcat(path, global_path);
		strcat(path, buf + 8);

		print2(buf, path);

		DIR *dir = opendir(path);

		free(path);

		print2p(buf, dir);

		buf--;
		memcpy(buf, &dir, sizeof(DIR *));

		print2p("opendir", (DIR *)(*(uint64_t *)buf));

		return sizeof(DIR *);
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
		}
		else
		{
			nrf_print_warning("We have \033[31;1mNULL\033[0m");
			sprintf(buf, "NULL");
		}

		return sizeof(struct dirent);
	}
	else if (strcmp(buf, "closedir") == 0)
	{
		DIR *dir;
		memcpy(&dir, buf + 9, sizeof(DIR *));

		print2p(buf, dir);

		closedir(dir);

		buf--;
		return 0;
	}
	else if (strcmp(buf, "mknod") == 0)
	{
		char *path = malloc(256);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2(buf, path);

		mode_t mode;
		dev_t rdev;
		memcpy(&mode, buf + 7 + strlen(path), sizeof(mode_t));
		memcpy(&rdev, buf + 7 + strlen(path) + sizeof(mode_t), sizeof(dev_t));

		int rv = mknod(path, mode, rdev);

		free(path);

		memcpy(buf, &rv, sizeof(int));
		return sizeof(int);
	}
	else if (strcmp(buf, "mkdir") == 0)
	{
		char *path = malloc(256);
		path[0] = '\0';

		strcat(path, global_path);
		strcat(path, buf + 6);

		print2p(buf, path);

		mode_t mode;
		memcpy(&mode, buf + 7 + strlen(path), sizeof(mode_t));

		int rv = mkdir(path, mode);

		printf("\33[35mmkdir rv & errno: %d, %s\n\33[0m", rv, strerror(errno));

		free(path);

		memcpy(buf, &rv, sizeof(int));
		return sizeof(int);
	}
	else
	{
		return 0;
	}
}

void client_handler(int client_fd)
{
    char buf[1024];
    int data_size;
    while (1) {
        data_size = read (client_fd, &buf, 1024);
        if (data_size <= 0)
            break;

        if (strlen(buf) > 0)
        {
        	if (buf[0] == '1')
        	{
        		data_size = call_raid_1_function(buf + 1);

        		printf("\33[35mbuf: %s\n\33[0m", buf);
        		// write(1, buf, data_size);
        		printf("\n");
        	}
        	else if (buf[0] == '5')
        	{

        	}
        	else
        	{
        		char *info_msg = (char*)malloc(31);
				sprintf(info_msg, "Unknown raid number %s", buf);
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