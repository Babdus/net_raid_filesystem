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

static char *cur_time()
{
	time_t rawtime;
	struct tm * timeinfo;
	time (&rawtime);
	timeinfo = localtime(&rawtime);
	char *cur_time = asctime(timeinfo);
	cur_time[strlen(cur_time)-1] = '\0';
	return cur_time;
}

void nrf_print_error(const char *msg)
{
	printf("\033[31;1m(NRF)\033[0m\033[31m[%s]\033[31;1m   Error:\033[0m \033[31m%s\033[0m\n", cur_time(), msg);
}

void nrf_print_warning(const char *msg)
{
	printf("\033[33;1m(NRF)\033[0m\033[33m[%s]\033[33;1m Warning:\033[0m \033[33m%s\033[0m\n", cur_time(), msg);
}

void nrf_print_note(const char *msg)
{
	printf("\033[34;1m(NRF)\033[0m\033[34m[%s]\033[34;1m    Note:\033[0m \033[34m%s\033[0m\n", cur_time(), msg);
}

void nrf_print_success(const char *msg)
{
	printf("\033[32;1m(NRF)\033[0m\033[32m[%s]\033[32;1m Success:\033[0m \033[32m%s\033[0m\n", cur_time(), msg);
}

void nrf_print_info(const char *msg)
{
	printf("\033[36;1m(NRF)\033[0m\033[36m[%s]\033[36;1m    Info:\033[0m \033[36m%s\033[0m\n", cur_time(), msg);
}

void nrf_print_struct(void *structure, char *name, int mode, int fd)
{
	if (strcmp(name, "stat") == 0)
		print_stat((struct stat *)structure, mode, fd);
	else if (strcmp(name, "dirent") == 0)
		print_dirent((struct dirent *)structure, mode, fd);
}