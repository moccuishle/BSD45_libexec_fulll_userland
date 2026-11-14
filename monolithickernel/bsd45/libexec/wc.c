/*
 * wc(1) - count lines, words and bytes of the named files (or stdin).
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

static void
count(int fd, unsigned *lines, unsigned *words, unsigned *bytes)
{
	char buf[512];
	int n, i, inword = 0;

	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		*bytes += n;
		for (i = 0; i < n; i++) {
			if (buf[i] == '\n')
				(*lines)++;
			if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n') {
				inword = 0;
			} else if (!inword) {
				inword = 1;
				(*words)++;
			}
		}
	}
}

int
main(int argc, char **argv)
{
	unsigned l = 0, w = 0, b = 0;
	int i, fd;

	if (argc < 2) {
		count(0, &l, &w, &b);
	} else {
		for (i = 1; i < argc; i++) {
			fd = open(argv[i], O_RDONLY);
			if (fd < 0) {
				printf("wc: %s: cannot open\n", argv[i]);
				return (1);
			}
			count(fd, &l, &w, &b);
			close(fd);
		}
	}
	printf("%d %d %d\n", l, w, b);
	return (0);
}
