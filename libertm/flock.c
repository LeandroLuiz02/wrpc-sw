#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>

static char ertm_big_lock[] = "/run/libertm/big-ertm-lock";
static int lock = -1;

int open_lock_file(char *filename)
{
	char *dir = dirname(filename);
	int fd;

	/* create dir if not exists */
	if ((access(dir, F_OK) != 0) && (mkdir(dir, 0755) < 0))
		return -1;
	/* dir created, create lock file */
	if ((fd = open(filename, 0755, O_CREAT)) < 0)
		return -1;
	close(fd);
	/* open lock file */
	if ((lock = open(filename, 0755, O_RDONLY)) < 0)
		return lock;
	return lock;
}

int ertm_mutex_acquire(void)
{
	return flock(lock, LOCK_EX);
}

int ertm_mutex_release(void)
{
	return flock(lock, LOCK_UN);
}

int main(int argc, char *argv[])
{
	int c;

	open_lock_file(ertm_big_lock);
	while ((c = getchar()) != EOF)
	switch (c) {
	case 'l':
		printf("lock: ");
		if (ertm_mutex_acquire() < 0) {
			perror("failed, acq");
			continue;
		}
		printf("locked!\n");
		break;
	case 'u':
		printf("unlock: ");
		if (ertm_mutex_release() < 0) {
			perror("failed, release");
			continue;
		}
		printf("unlocked!\n");
		break;
	default:
		break;
	}
	return 0;
}
