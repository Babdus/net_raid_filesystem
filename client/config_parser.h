#ifndef CONFIG_PARSER
#define CONFIG_PARSER

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../nrf_print.h"

enum cache_repl_strat {FIFO, LIFO, LRU, MRU};

struct server
{
	char *ip;  // May be used some other type
	int port;
	struct server *next_server;
};

struct storage
{
	char *diskname;
	char *mountpoint;
	int raid;
	int server_n;
	struct server *servers;
	struct server *hotswap;
	struct storage *next_storage;
};

struct config
{
	char *err_log_path;
	int cache_size;
	enum cache_repl_strat crs;
	int timeout;
	int storage_n;
	struct storage *storages;
};

struct config *global_config;

struct config *config_parser_parse (const char *);

#endif