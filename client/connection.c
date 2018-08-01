#include "connection.h"

int connect_to_server(struct server *server, struct storage *storage)
{
	int server_fd;
	struct sockaddr_in addr;
	int ip;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	int status = inet_pton(AF_INET, server->ip, &ip);

	if (status < 1)
	{
		nrf_print_error_x(global_config->err_log_path, storage->diskname, server->ip, server->port, "invalid ip address");
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(server->port);
	addr.sin_addr.s_addr = ip;

	status = connect(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	if (status < 0)
	{
		nrf_print_error_x(global_config->err_log_path, storage->diskname, server->ip, server->port, "can not connect to server");
		return -1;
	}

	nrf_print_success_x(global_config->err_log_path, storage->diskname, server->ip, server->port, "server is connected");

	return server_fd;
}