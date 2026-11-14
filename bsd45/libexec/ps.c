/*
 * ps(1) - list the processes.  The kernel renders its process table into the
 * /dev/ps pseudo file (one line per live process), so this is a cat with a
 * header, same idea as df(1) over /dev/mounts.
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
	int fd, n;

	fd = open("/dev/ps", O_RDONLY);
	if (fd < 0) {
		printf("ps: /dev/ps: cannot open\n");
		return (1);
	}
	printf("PID PPID PGRP STAT COMM\n");
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		write(1, buf, n);
	close(fd);
	return (0);
}
