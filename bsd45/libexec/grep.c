/*
 * grep(1) - print the lines of the named files that contain a pattern.
 * Deliberately simple: plain substring match, no regex.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	char buf[512], line[512];
	int i, fd, n, ll = 0, found = 0;

	if (argc < 2) {
		printf("usage: grep pattern [file ...]\n");
		return (1);
	}
	if (argc == 2) {
		while ((n = read(0, buf, sizeof(buf))) > 0)
			;
		return (1);
	}
	for (i = 2; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			printf("grep: %s: cannot open\n", argv[i]);
			continue;
		}
		ll = 0;
		while ((n = read(fd, buf, sizeof(buf))) > 0) {
			int k;

			for (k = 0; k < n; k++) {
				line[ll++] = buf[k];
				if (buf[k] == '\n' || ll >= (int)sizeof(line) - 1) {
					line[ll] = 0;
					if (strstr(line, argv[1]) != 0) {
						write(1, line, strlen(line));
						found = 1;
					}
					ll = 0;
				}
			}
		}
		close(fd);
	}
	return (found ? 0 : 1);
}
