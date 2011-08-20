#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

int	numf;
int verbose  = 0;
unsigned long long tot = 0;
size_t page_size;

static void process_dir(char *filename);
static void process_file(char *filename);

int main(int argc, char *argv[]) 
{
	int i;

	page_size = getpagesize();
	if (strcmp(argv[1], "-v") == 0) {
		argv++, argc--;
		verbose = 1;
	}

	printf("%-8s %9s Filename\n", "Blocks", "Size (MB)");
	printf("%-8s %9s %s\n", "======", "=========", "==============================");
	for (i = 1; i < argc; i++) {
		process_file(argv[i]);
	}
	if (numf > 2) {
		printf("%-8s %9s -----\n", "", "");
		printf("%-8lld %9lld Total\n", 
			tot, (tot * 4096) / (1024 * 1024));
	}
}
static void
process_file(char *filename)
{
	int fd;
	struct stat file_stat;
	unsigned cnt;
	void *file_mmap;
	unsigned char *mincore_vec;
	unsigned int size;
	size_t page_index;

	if (lstat64(filename, &file_stat) == 0 && S_ISLNK(file_stat.st_mode))
		return;
	if ((fd = open64(filename, O_RDONLY)) < 0) {
		perror(filename);
		return;
	}

	fstat(fd, &file_stat);
	if (S_ISDIR(file_stat.st_mode)) {
		close(fd);
		process_dir(filename);
		return;
	}

	size = file_stat.st_size;
	if (size > 3000 * 1024 * 1024)
		size = 3000* 1024 * 1024;

	file_mmap = mmap((void *)0, size, PROT_NONE, MAP_SHARED, fd, 0);
	mincore_vec = calloc(1, (size+page_size-1)/page_size);
	mincore(file_mmap, size, mincore_vec);
	cnt = 0;
	if (verbose)
	    printf("Cached Blocks of %s: ", filename);
	for (page_index = 0; page_index <= size/page_size; page_index++) {
		if (mincore_vec[page_index]&1) {
		    cnt++;
		    if (verbose)
		            printf("%lu ", (unsigned long)page_index);
		}
	}
	munmap(file_mmap, size);
	if (verbose)
	    printf("\n");
	printf("%-8d %9d %s\n", 
		cnt, (cnt * 4096) / (1024 * 1024),
		filename);
	tot += cnt;
	numf++;
	free(mincore_vec);
	close(fd);
}
static void
process_dir(char *filename)
{	DIR *dirp = opendir(filename);
	struct dirent *de;

	if (dirp == NULL)
		return;

	while ((de = readdir(dirp)) != NULL) {
		char name[PATH_MAX];
		if (strcmp(de->d_name, ".") == 0)
			continue;
		if (strcmp(de->d_name, "..") == 0)
			continue;
		snprintf(name, sizeof name, "%s/%s", filename, de->d_name);
		process_file(name);
	}
	closedir(dirp);
}
