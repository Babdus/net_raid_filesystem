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
#include "fuse_raid_5.h"
#include "connection.h"

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
	if(storage->raid == 1 || storage->raid == 5)
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
		struct storage *storage = global_config->storages;
		truncate(global_config->err_log_path, 0);
		while (storage != NULL){
			switch(fork()) {
				case -1:
					exit(100);
				case 0:
					printf("\n");
					mount(storage);
					exit(0);
				default:
					storage = storage->next_storage;
			}
		}
		return 0;
	}
	else{
		nrf_print_error("Could not load configuration file");
		return -1;
	}
}