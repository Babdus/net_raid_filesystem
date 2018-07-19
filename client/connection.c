#include "connection.h"

int connect_to_server(struct server *server)
{
	int server_fd;
	struct sockaddr_in addr;
	int ip;
	char *info_msg;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	int status = inet_pton(AF_INET, server->ip, &ip);

	if (status < 1)
	{
		info_msg = (char*)malloc(strlen(server->ip) + 30);
		sprintf(info_msg, "Invalid ip address %s", server->ip);
		nrf_print_error(info_msg);
		free(info_msg);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(server->port);
	addr.sin_addr.s_addr = ip;

	status = connect(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	if (status < 0)
	{
		info_msg = (char*)malloc(strlen(server->ip) + 45);
		sprintf(info_msg, "Can not connect to address %s:%d", server->ip, server->port);
		nrf_print_error(info_msg);
		free(info_msg);
		return -1;
	}

	info_msg = (char*)malloc(strlen(server->ip) + 38);
	sprintf(info_msg, "Server %s:%d is connected", server->ip, server->port);
	nrf_print_success(info_msg);
	free(info_msg);

	return server_fd;
}