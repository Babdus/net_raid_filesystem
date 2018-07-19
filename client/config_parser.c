#include "config_parser.h"

static void trim(char *str)
{
	while(str[0] == ' ' || str[0] == '\t' || str[0] == '\n')
		str++;

	int str_len = strlen(str);
	while(str[str_len-1] == ' ' || str[str_len-1] == '\t' || str[str_len-1] == '\n')
		str_len--;

	str[str_len] = '\0';
}

static int parse_key_value(const char *line, char key[], char value[])
{
	int status = -1;
	int i = 0;
	for (; i < strlen(line) - 2; i++)
	{
		if (line[i] == ' ' && line[i+1] == '=' && line[i+2] == ' ')
		{
			status = 0;
			break;
		}
	}
	if (status != 0)
		return status;

	strncpy(key, line, i);
	key[i] = '\0';
	strcpy(value, line + i + 3);

	// trim spaces
	trim(key);
	trim(value);

	return status;
}

static int parse_cache_size(char *str, int **status)
{
	int size = 1;
	int len = strlen(str);
	if (len == 0)
	{
		**status = 1;
		nrf_print_error("Cache size is not provided, or is malformated");
		return 0;
	}

	if (str[len - 1] == 'G')
	{
		size = 1073741824;
		len--;
	}

	if (str[len - 1] == 'M')
	{
		size = 1048576;
		len--;
	}

	if (str[len - 1] == 'K')
	{
		size = 1024;
		len--;
	}

	str[len] = '\0';
	size *= (int)strtol(str, (char **)NULL, 10);

	if (size == 0)
	{
		**status = 1;
		nrf_print_error("Cache size is not provided, or is malformatted");
		return 0;
	}

	return size;
}

static enum cache_repl_strat parse_crs(const char *str, int **status)
{
	enum cache_repl_strat strat;

	if (strcmp(str, "FIFO") == 0 || strcmp(str, "fifo") == 0)
		strat = FIFO;
	else if (strcmp(str, "LIFO") == 0 || strcmp(str, "lifo") == 0)
		strat = LIFO;
	else if (strcmp(str, "LRU") == 0 || strcmp(str, "lru") == 0)
		strat = LRU;
	else if (strcmp(str, "MRU") == 0 || strcmp(str, "mru") == 0)
		strat = MRU;
	else
	{
		**status = 1;
		char *err_msg = (char*)malloc(strlen(str) + 48);
		sprintf(err_msg, "Unknown cache replacement strategy '%s'", str);
		nrf_print_error(err_msg);
		free(err_msg);
		strat = LRU;
		nrf_print_note("Using default 'LRU' strategy");
	}

	return strat;
}

static int parse_int(const char *str, int **status)
{
	if (strlen(str) == 1 && str[0] == '0')
		return 0;

	int res = (int)strtol(str, (char **)NULL, 10);

	if (res == 0)
	{
		**status = -1;
		char *err_msg = (char*)malloc(strlen(str) + 38);
		sprintf(err_msg, "Could not retrieve integer from '%s'", str);
		nrf_print_error(err_msg);
		free(err_msg);
	}

	return res;
}

static struct server *parse_servers(char *str, int **status, int **server_n)
{
	struct server *server = malloc(sizeof(struct server));
	server->next_server = NULL;
	struct server *head = server;
	if (server_n != NULL)
		(**server_n)++;

	int i = 0;
	while (str[i] != '\0')
	{
		if (str[i] == ',')
		{
			str[i] = '\0';
			server->port = parse_int(str, status);
			str[i] = ',';

			if (server_n != NULL)
				(**server_n)++;

			server->next_server = malloc(sizeof(struct server));
			server = server->next_server;
			server->next_server = NULL;

			str += i + 1;
			i = -1;
		}
		else if (str[i] == ':')
		{
			server->ip = strndup(str, i);
			str += i + 1;
			i = -1;
		}
		else if (str[i] == ' ')
		{
			str += i + 1;
			i = -1;
		}
		
		i++;
	}

	server->port = parse_int(str, status);
	
	return head;
}

static int is_general_block(const char *key)
{
	return strcmp(key, "errorlog") == 0 || strcmp(key, "cache_size") == 0 || strcmp(key, "cache_replacment") == 0 || strcmp(key, "timeout") == 0;
}

static int is_storage_block(const char *key)
{
	return strcmp(key, "diskname") == 0 || strcmp(key, "mountpoint") == 0 || strcmp(key, "raid") == 0 || strcmp(key, "servers") == 0 || strcmp(key, "hotswap") == 0;
}

struct config *config_parser_parse(const char *path)
{
	FILE *file;
	char line[128];

	file = fopen(path, "r");
	if (file == NULL)
	{
		char *err_msg = (char*)malloc(strlen(path) + 35);
		sprintf(err_msg, "Can not open the file '%s'", path);
		nrf_print_error(err_msg);
		free(err_msg);
		return NULL;
	}

	struct config *config = malloc(sizeof(struct config));

	int i = 1;
	int storage_n = 1;
	int is_first_line = 1;
	struct storage *last_storage = NULL;
	while (fgets(line, 128, file) != NULL)
	{
		if (strcmp(line, "\n") == 0)
		{
			if (is_first_line == 0)
			{
				storage_n++;
				is_first_line = 1;
			}
			
			continue;
		}

		char key[32];
		char value[96];

		int kv_status = parse_key_value(line, key, value);

		if (kv_status != 0)
		{
			char *err_msg = (char*)malloc(strlen(line) + 54);
			sprintf(err_msg, "Can not parse line '%s' to key and value pair", line);
			nrf_print_error(err_msg);
			free(err_msg);
			return NULL;
		}

		int *status = malloc(sizeof(int));

		*status = 0;

		if (is_general_block(key) == 1)
		{
			if (is_first_line == 1)
				storage_n--;

			if (strcmp(key, "errorlog") == 0)
				config->err_log_path = strdup(value);
			else if (strcmp(key, "cache_size") == 0)
				config->cache_size = parse_cache_size(value, &status);
			else if (strcmp(key, "cache_replacment") == 0)
				config->crs = parse_crs(value, &status);
			else if (strcmp(key, "timeout") == 0)
				config->timeout = parse_int(value, &status);
			else
				nrf_print_warning("Something's gone wrong");
		}
		else if (is_storage_block(key) == 1)
		{
			if (is_first_line == 1)
			{
				struct storage *storage = malloc(sizeof(struct storage));
				if (last_storage != NULL)
					last_storage->next_storage = storage;
				else
					config->storages = storage;

				storage->next_storage = NULL;
				last_storage = storage;
			}

			int *server_n = malloc(sizeof(int));
			*server_n = 0;

			if (strcmp(key, "diskname") == 0)
				last_storage->diskname = strdup(value);
			else if (strcmp(key, "mountpoint") == 0)
				last_storage->mountpoint = strdup(value);
			else if (strcmp(key, "raid") == 0)
				last_storage->raid = parse_int(value, &status);
			else if (strcmp(key, "servers") == 0)
				last_storage->servers = parse_servers(value, &status, &server_n);
			else if (strcmp(key, "hotswap") == 0)
				last_storage->hotswap = parse_servers(value, &status, NULL);
			else
				nrf_print_warning("Something's gone wrong");

			if (*server_n != 0)
				last_storage->server_n = *server_n;

			free(server_n);
		}
		else
		{
			char *warn_msg = (char*)malloc(strlen(key) + 32);
			sprintf(warn_msg, "Unknown config key '%s'", key);
			nrf_print_warning(warn_msg);
			free(warn_msg);
		}

		if (*status == -1)
		{
			free(status);
			return NULL;
		}
		else if (*status == 1)
			nrf_print_warning("Continuing with errors");

		is_first_line = 0;
		i++;
		free(status);
	}
	config->storage_n = storage_n;

	return config;
}