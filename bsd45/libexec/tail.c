/*
 * tail(1) - print the last N lines of a file (default 10).
 *
 * The whole file is slurped into a bounded buffer (4 KB, which covers the
 * files on this image), then the start of the last N lines is found by
 * scanning backwards for newlines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static char buf[4096];

static int
tailfd(int fd, int n)
{
	int total = 0, r, i, cnt = 0;

	while (total < (int)sizeof(buf) - 1 &&
	    (r = read(fd, buf + total, (int)sizeof(buf) - 1 - total)) > 0)
		total += r;
	buf[total] = 0;
	if (total == 0)
		return (0);
	/* find the start of the last n lines */
	for (i = total - 1; i >= 0; i--) {
		if (buf[i] == '\n') {
			if (i == total - 1)
				continue;	/* trailing newline */
			if (++cnt >= n) {
				i++;
				break;
			}
		}
	}
	if (i < 0)
		i = 0;
	write(1, buf + i, total - i);
	return (0);
}

int
main(int argc, char **argv)
{
	int i = 1, fd, n = 10;

	if (i < argc && argv[i][0] == '-' &&
	    argv[i][1] >= '0' && argv[i][1] <= '9')
		n = atoi(argv[i++] + 1);
	if (n <= 0) {
		printf("usage: tail [-N] file\n");
		return (1);
	}
	if (i >= argc) {
		printf("usage: tail [-N] file\n");
		return (1);
	}
	fd = open(argv[i], O_RDONLY);
	if (fd < 0) {
		printf("tail: %s: cannot open\n", argv[i]);
		return (1);
	}
	tailfd(fd, n);
	close(fd);
	return (0);
}
