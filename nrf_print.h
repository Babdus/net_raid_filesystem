#ifndef NRF_PRINT
#define NRF_PRINT

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <time.h>
#include <sys/time.h>

void nrf_print_error(const char *);
void nrf_print_warning(const char *);
void nrf_print_note(const char *);
void nrf_print_success(const char *);
void nrf_print_info(const char *);

void nrf_print_struct(void *structure, char *name, int mode, int fd);

#endif