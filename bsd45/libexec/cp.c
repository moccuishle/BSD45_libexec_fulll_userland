/*
 * cp(1) - copy one file.  Reads /readme.txt from the read-only ISO and
 * writes into the tmpfs mount, which is exactly the S10.3 acceptance path.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int
main(int argc, char **argv)
{
	char buf[512];
	int in, out, n, total = 0;

	if (argc != 3) {
		printf("usage: cp src dst\n");
		return (1);
	}
	in = open(argv[1], O_RDONLY);
	if (in < 0) {
		printf("cp: %s: %s\n", argv[1], strerror(errno));
		return (1);
	}
	out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (out < 0) {
		printf("cp: %s: %s\n", argv[2], strerror(errno));
		close(in);
		return (1);
	}
	while ((n = read(in, buf, sizeof(buf))) > 0) {
		if (write(out, buf, n) != n) {
			printf("cp: write failed\n");
			close(in);
			close(out);
			return (1);
		}
		total += n;
	}
	close(in);
	close(out);
	if (n < 0) {
		printf("cp: read failed\n");
		return (1);
	}
	return (0);
}
