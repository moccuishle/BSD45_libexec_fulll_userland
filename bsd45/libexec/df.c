/*
 * df(1) and mount(1) data source: the kernel exposes the S10.4 mount table
 * as the pseudo file /dev/mounts (one line per mount), so both tools are
 * just a cat of that file with a small header.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	char buf[256];
	int fd, n, ismount = 0;
	const char *base = strrchr(argv[0], '/');

	base = (base != 0) ? base + 1 : argv[0];
	if (strcmp(base, "mount") == 0 || (argc > 1 && strcmp(argv[1], "-m") == 0))
		ismount = 1;

	fd = open("/dev/mounts", O_RDONLY);
	if (fd < 0) {
		printf("%s: /dev/mounts: cannot open\n", ismount ? "mount" : "df");
		return (1);
	}
	if (ismount)
		printf("device on mount point type\n");
	else
		printf("Filesystem  Mounted on  Size\n");
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		write(1, buf, n);
	close(fd);
	return (0);
}
