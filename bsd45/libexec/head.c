/*
 * head(1) - print the first N lines of the named files (default 10).
 *
 * Plain userland tool: open/read/write only, so it works on both the read-only
 * ISO9660 root and the tmpfs mount.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int nlines = 10;

static void
headfd(int fd)
{
	char buf[512];
	int n, i, printed = 0;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; i++) {
			if (write(1, &buf[i], 1) != 1)
				return;
			if (buf[i] == '\n' && ++printed >= nlines)
				return;
		}
	}
}

int
main(int argc, char **argv)
{
	int i = 1, fd;

	if (i < argc && argv[i][0] == '-' &&
	    argv[i][1] >= '0' && argv[i][1] <= '9')
		nlines = atoi(argv[i++] + 1);
	if (nlines <= 0) {
		printf("usage: head [-N] [file ...]\n");
		return (1);
	}
	if (i >= argc) {
		headfd(0);
		return (0);
	}
	for (; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			printf("head: %s: cannot open\n", argv[i]);
			continue;
		}
		headfd(fd);
		close(fd);
	}
	return (0);
}
