/*
 * sleep.c - /bin/sleep for the BSD45 userland (S10.2).
 *
 * There is no clock syscall yet, so sleep(3) in libc is a busy loop; this
 * tool exists mainly so shell job-control tests have a background job that
 * does not touch the console.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: sleep seconds\n");
		exit(1);
	}
	sleep((unsigned)atoi(argv[1]));
	exit(0);
}
