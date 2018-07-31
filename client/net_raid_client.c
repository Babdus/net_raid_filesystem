#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../nrf_print.h"
#include "config_parser.h"
#include "fuse_raid_1.h"
#include "fuse_raid_5.h"
#include "connection.h"

void print_config(struct config *config)
{
	printf("Config: err_log_path: %s\n", config->err_log_path);
	printf("Config: cache_size: %d\n", config->cache_size);
	printf("Config: crs: %d\n", config->crs);
	printf("Config: timeout: %d\n", config->timeout);
	printf("Config: storage_n: %d\n", config->storage_n);
	printf("Config: storages: %p\n", (void *)config->storages);

	struct storage *st = config->storages;
	while (st != NULL)
	{
		printf("Config: diskname: %s\n", st->diskname);
		printf("Config: mountpoint: %s\n", st->mountpoint);
		printf("Config: raid: %d\n", st->raid);
		printf("Config: server_n: %d\n", st->server_n);
		printf("Config: servers: %p\n", (void *)st->servers);

		struct server *se = st->servers;
		while (se != NULL)
		{
			printf("Config: ip: %s\n", se->ip);
			printf("Config: port: %d\n", se->port);
			printf("Config: next_server: %p\n", (void *)se->next_server);
			se = se->next_server;
		}

		printf("Config: hotswap: %p\n", (void *)st->hotswap);
		printf("Config: ip: %s\n", st->hotswap->ip);
		printf("Config: port: %d\n", st->hotswap->port);
		printf("Config: next_server: %p\n", (void *)st->hotswap->next_server);
		printf("Config: next_storage: %p\n", (void *)st->next_storage);
		st = st->next_storage;
	}
}

// static void mount_raid_1(struct storage *storage, struct config *config)
// {
// 	char *argv[2];
// 	argv[0] = strdup("net_raid_client");
// 	argv[1] = strdup(storage->mountpoint);
// 	int status = mount_raid_1(2, argv);
// 	free(argv[0]);
// 	free(argv[0]);
// }

static void mount(struct storage *storage)
{	

	int argc = 5;
	char *argv[argc];
	argv[0] = strdup("net_raid_client");
	argv[1] = strdup("-f");
	argv[2] = strdup("-o");
	argv[3] = strdup("sync_read");
	argv[4] = strdup(storage->mountpoint);

	int status = -1;
	if(storage->raid == 1)
	{
		status = mount_raid_1(argc, argv, storage);
	}
	else if(storage->raid == 5)
	{
		status = mount_raid_5(argc, argv, storage);
	}
	else
	{
		char *msg = (char*)malloc(45);
		sprintf(msg, "RAID %d is not implemented", storage->raid);
		nrf_print_warning(msg);
		free(msg);
	}

	if (status == 0)
	{
		char *info_msg = (char*)malloc(strlen(argv[4]) + 31);
		sprintf(info_msg, "Mounted directory '%s'", argv[4]);
		nrf_print_success(info_msg);
		free(info_msg);
	}
	else nrf_print_error("Mount failed");

	free(argv[0]);
	free(argv[1]);
	free(argv[2]);
	free(argv[3]);
	free(argv[4]);
}

int main (int argc, char **argv)
{
	if (argc < 2)
	{
		nrf_print_error("Configuration file path is not provided");
		return -1;
	}

	global_config = config_parser_parse(argv[1]);
	if (global_config != NULL)
	{
		nrf_print_success("Configuration file loaded successfully");
		// print_config(config);
		struct storage *storage = global_config->storages;
		while (storage != NULL){
			// mount(storage);
			switch(fork()) {
				case -1:
					exit(100);
				case 0:
					mount(storage);
					exit(0);
				default:
					storage = storage->next_storage;
			}
			// storage = storage->next_storage;
		}
		return 0;
	}
	else{
		nrf_print_error("Could not load configuration file");
		return -1;
	}
}