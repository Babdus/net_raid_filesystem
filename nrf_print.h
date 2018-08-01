#ifndef NRF_PRINT
#define NRF_PRINT

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <time.h>
#include <sys/time.h>

void nrf_print_error_x(char *err_path, char *name, char *ip, int port, const char *msg);
void nrf_print_success_x(char *err_path, char *name, char *ip, int port, const char *msg);
void nrf_print_error(const char *);
void nrf_print_warning(const char *);
void nrf_print_note(const char *);
void nrf_print_success(const char *);
void nrf_print_info(const char *);
void nrf_print_value(const char *);

void nrf_print_struct(void *structure, char *name, int mode, int fd);

void dumphex(const void *, size_t);

char *cur_time();

#endif