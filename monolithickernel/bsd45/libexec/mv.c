/*
 * mv(1) - rename, with a cp+unlink fallback when rename(2) says EXDEV
 * (the ISO9660 root is read-only, so only /tmp -> /tmp works by rename).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int
copyfile(const char *src, const char *dst)
{
	char buf[512];
	int in, out, n;

	in = open(src, O_RDONLY);
	if (in < 0) {
		printf("mv: %s: %s\n", src, strerror(errno));
		return (1);
	}
	out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (out < 0) {
		printf("mv: %s: %s\n", dst, strerror(errno));
		close(in);
		return (1);
	}
	while ((n = read(in, buf, sizeof(buf))) > 0)
		if (write(out, buf, n) != n)
			break;
	close(in);
	close(out);
	return (n < 0);
}

int
main(int argc, char **argv)
{
	if (argc != 3) {
		printf("usage: mv src dst\n");
		return (1);
	}
	if (rename(argv[1], argv[2]) == 0)
		return (0);
	if (copyfile(argv[1], argv[2]) != 0)
		return (1);
	if (unlink(argv[1]) != 0) {
		printf("mv: %s: %s\n", argv[1], strerror(errno));
		return (1);
	}
	return (0);
}
