/*
 * rm(1) - remove files and (with -r) directory trees.
 *
 * Exercises unlink(2)/rmdir(2) plus the S7 opendir/readdir path when
 * recursing.  The ISO9660 root is read-only, so rm on /bin gives EROFS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

static int recursive;

static int
rmone(const char *path)
{
	DIR *d;
	struct dirent *de;
	char sub[256];
	int rc = 0;

	if (!recursive)
		return (unlink(path) == 0 ? 0 : (printf("rm: %s: %s\n", path, strerror(errno)), 1));
	d = opendir(path);
	if (d == 0) {
		if (unlink(path) == 0)
			return (0);
		printf("rm: %s: %s\n", path, strerror(errno));
		return (1);
	}
	while ((de = readdir(d)) != 0) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
		rc |= rmone(sub);
	}
	closedir(d);
	if (rmdir(path) != 0) {
		printf("rm: %s: %s\n", path, strerror(errno));
		return (1);
	}
	return (rc);
}

int
main(int argc, char **argv)
{
	int i, rc = 0, n = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-rf") == 0) {
			recursive = 1;
			continue;
		}
		if (argv[i][0] == '-')
			continue;
		rc |= rmone(argv[i]);
		n++;
	}
	if (n == 0) {
		printf("usage: rm [-r] file ...\n");
		return (1);
	}
	return (rc);
}
