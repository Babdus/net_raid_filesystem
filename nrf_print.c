#include "nrf_print.h"

static void print_dirent(struct dirent *ent, int mode, int fd)
{
	if (fd != 1)
	{
		FILE *f = fopen("/home/babdus/Documents/OS/NRF/error.log", "a");
		fprintf(f, "struct dirent {\n");
		fprintf(f, "\t%s: %zu\n", "d_ino", (size_t)(ent->d_ino));
		fprintf(f, "\t%s: %d\n", "d_type", (int)(ent->d_type));
		fprintf(f, "\t%s: %s\n", "d_name", ent->d_name);
		fprintf(f, "}\n");
		return;
	}
	if (mode == 0)
	{
		printf("struct dirent {\n");
		printf("\t%s: %zu\n", "d_ino", (size_t)(ent->d_ino));
		printf("\t%s: %d\n", "d_type", (int)(ent->d_type));
		printf("\t%s: %s\n", "d_name", ent->d_name);
		printf("}\n");
	}
	else
	{
		printf("struct dirent { ");
		printf("%s: %zu, ", "d_ino", (size_t)(ent->d_ino));
		printf("%s: %d, ", "d_type", (int)(ent->d_type));
		printf("%s: %s ", "d_name", ent->d_name);
		printf("}\n");
	}
}

static void print_stat(struct stat *stat, int mode, int fd)
{
	if (fd != 1)
	{
		FILE *f = fopen("/home/babdus/Documents/OS/NRF/error.log", "a");
		fprintf(f, "struct stat {\n");
		fprintf(f, "\t%s: %zu\n", "st_dev", (size_t)(stat->st_dev));
		fprintf(f, "\t%s: %zu\n", "st_ino", (size_t)(stat->st_ino));
		fprintf(f, "\t%s: %zu\n", "st_mode", (size_t)(stat->st_mode));
		fprintf(f, "\t%s: %zu\n", "st_nlink", (size_t)(stat->st_nlink));
		fprintf(f, "\t%s: %zu\n", "st_uid", (size_t)(stat->st_uid));
		fprintf(f, "\t%s: %zu\n", "st_gid", (size_t)(stat->st_gid));
		fprintf(f, "\t%s: %zu\n", "st_rdev", (size_t)(stat->st_rdev));
		fprintf(f, "\t%s: %zu\n", "st_size", (size_t)(stat->st_size));
		fprintf(f, "\t%s: %zu\n", "st_blksize", (size_t)(stat->st_blksize));
		fprintf(f, "\t%s: %zu\n\n", "st_blocks", (size_t)(stat->st_blocks));
		fprintf(f, "\tst_atim {\n");
		fprintf(f, "\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_atim).tv_sec));
		fprintf(f, "\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_atim).tv_nsec));
		fprintf(f, "\t}\n");
		fprintf(f, "\tst_mtim {\n");
		fprintf(f, "\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_mtim).tv_sec));
		fprintf(f, "\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_mtim).tv_nsec));
		fprintf(f, "\t}\n");
		fprintf(f, "\tst_ctim {\n");
		fprintf(f, "\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_ctim).tv_sec));
		fprintf(f, "\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_ctim).tv_nsec));
		fprintf(f, "\t}\n");
		fprintf(f, "}\n");
		return;
	}
	if (mode == 0)
	{
		printf("struct stat {\n");
		printf("\t%s: %zu\n", "st_dev", (size_t)(stat->st_dev));
		printf("\t%s: %zu\n", "st_ino", (size_t)(stat->st_ino));
		printf("\t%s: %zu\n", "st_mode", (size_t)(stat->st_mode));
		printf("\t%s: %zu\n", "st_nlink", (size_t)(stat->st_nlink));
		printf("\t%s: %zu\n", "st_uid", (size_t)(stat->st_uid));
		printf("\t%s: %zu\n", "st_gid", (size_t)(stat->st_gid));
		printf("\t%s: %zu\n", "st_rdev", (size_t)(stat->st_rdev));
		printf("\t%s: %zu\n", "st_size", (size_t)(stat->st_size));
		printf("\t%s: %zu\n", "st_blksize", (size_t)(stat->st_blksize));
		printf("\t%s: %zu\n\n", "st_blocks", (size_t)(stat->st_blocks));
		printf("\tst_atim {\n");
		printf("\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_atim).tv_sec));
		printf("\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_atim).tv_nsec));
		printf("\t}\n");
		printf("\tst_mtim {\n");
		printf("\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_mtim).tv_sec));
		printf("\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_mtim).tv_nsec));
		printf("\t}\n");
		printf("\tst_ctim {\n");
		printf("\t\t%s: %zu\n", "tv_sec", (size_t)((stat->st_ctim).tv_sec));
		printf("\t\t%s: %ld\n", "tv_nsec", (long)((stat->st_ctim).tv_nsec));
		printf("\t}\n");
		printf("}\n");
	}
	else
	{
		printf("struct stat { ");
		printf("%s: %zu, ", "st_dev", (size_t)(stat->st_dev));
		printf("%s: %zu, ", "st_ino", (size_t)(stat->st_ino));
		printf("%s: %zu, ", "st_mode", (size_t)(stat->st_mode));
		printf("%s: %zu, ", "st_nlink", (size_t)(stat->st_nlink));
		printf("%s: %zu, ", "st_uid", (size_t)(stat->st_uid));
		printf("%s: %zu, ", "st_gid", (size_t)(stat->st_gid));
		printf("%s: %zu, ", "st_rdev", (size_t)(stat->st_rdev));
		printf("%s: %zu, ", "st_size", (size_t)(stat->st_size));
		printf("%s: %zu, ", "st_blksize", (size_t)(stat->st_blksize));
		printf("%s: %zu, ", "st_blocks", (size_t)(stat->st_blocks));
		printf("st_atim { ");
		printf("%s: %zu, ", "tv_sec", (size_t)((stat->st_atim).tv_sec));
		printf("%s: %ld ", "tv_nsec", (long)((stat->st_atim).tv_nsec));
		printf("}, ");
		printf("st_mtim { ");
		printf("%s: %zu, ", "tv_sec", (size_t)((stat->st_mtim).tv_sec));
		printf("%s: %ld ", "tv_nsec", (long)((stat->st_mtim).tv_nsec));
		printf("}, ");
		printf("st_ctim { ");
		printf("%s: %zu, ", "tv_sec", (size_t)((stat->st_ctim).tv_sec));
		printf("%s: %ld ", "tv_nsec", (long)((stat->st_ctim).tv_nsec));
		printf("} ");
		printf("}\n");
	}
}

static char *weekday(int day)
{
	switch(day)
	{
		case 0: return "Sunday";
		case 1: return "Monday";
		case 2: return "Tuesday";
		case 3: return "Wednesday";
		case 4: return "Thursday";
		case 5: return "Friday";
		case 6: return "Saturday";
		default: return "Day";
	}
}

static char *month(int month)
{
	switch(month)
	{
		case 0: return "January";
		case 1: return "February";
		case 2: return "March";
		case 3: return "April";
		case 4: return "May";
		case 5: return "June";
		case 6: return "July";
		case 7: return "August";
		case 8: return "September";
		case 9: return "October";
		case 10: return "November";
		case 11: return "December";
		default: return "Month";
	}
}

char *cur_time(char *buf)
{
	time_t rawtime;
	struct tm *timeinfo;
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	// char *cur_time = asctime(timeinfo);
	// cur_time[strlen(cur_time)-1] = '\0';

    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    sprintf(buf, "%02d:%02d:%02d:%06ld %s, %02d %s %d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, spec.tv_nsec/1000, weekday(timeinfo->tm_wday), timeinfo->tm_mday, month(timeinfo->tm_mon), timeinfo->tm_year + 1900);

	return buf;
}

void nrf_print_error(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[31;1m(NRF)\033[0m\033[31m[%s]\033[31;1m   Error:\033[0m \033[31m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_warning(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[33;1m(NRF)\033[0m\033[33m[%s]\033[33;1m Warning:\033[0m \033[33m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_note(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[34;1m(NRF)\033[0m\033[34m[%s]\033[34;1m    Note:\033[0m \033[34m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_success(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[32;1m(NRF)\033[0m\033[32m[%s]\033[32;1m Success:\033[0m \033[32m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_info(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[36;1m(NRF)\033[0m\033[36m[%s]\033[36;1m    Info:\033[0m \033[36m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_value(const char *msg)
{
	char *buf = malloc(64);
	printf("\033[35;1m(NRF)\033[0m\033[35m[%s]\033[35;1m   Value:\033[0m \033[35m%s\033[0m\n", cur_time(buf), msg);
	free(buf);
}

void nrf_print_struct(void *structure, char *name, int mode, int fd)
{
	if (strcmp(name, "stat") == 0)
		print_stat((struct stat *)structure, mode, fd);
	else if (strcmp(name, "dirent") == 0)
		print_dirent((struct dirent *)structure, mode, fd);
}

void dumphex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}