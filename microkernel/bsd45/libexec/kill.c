/*
 * kill.c - /bin/kill: send a signal to a process (S10.2).
 *
 * 4.5BSD's sh has jobs/fg/bg builtins but no kill builtin, so the shell
 * needs this little tool.  Usage:
 *
 *	kill [-signal] pid ...
 *	kill -l
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

static struct {
	const char	*name;
	int		num;
} sigs[] = {
	{ "HUP", 1 }, { "INT", 2 }, { "QUIT", 3 }, { "ILL", 4 },
	{ "TRAP", 5 }, { "ABRT", 6 }, { "EMT", 7 }, { "FPE", 8 },
	{ "KILL", 9 }, { "BUS", 10 }, { "SEGV", 11 }, { "SYS", 12 },
	{ "PIPE", 13 }, { "ALRM", 14 }, { "TERM", 15 }, { "URG", 16 },
	{ "STOP", 17 }, { "TSTP", 18 }, { "CONT", 19 }, { "CHLD", 20 },
	{ "TTIN", 21 }, { "TTOU", 22 }, { "IO", 23 }, { "XCPU", 24 },
	{ "XFSZ", 25 }, { "VTALRM", 26 }, { "PROF", 27 }, { "WINCH", 28 },
	{ "INFO", 29 }, { "USR1", 30 }, { "USR2", 31 },
	{ 0, 0 }
};

static int
signum(const char *s)
{
	int i, n;

	if (*s >= '0' && *s <= '9')
		return (atoi(s));
	for (i = 0; sigs[i].name != 0; i++) {
		const char *a = sigs[i].name, *b = s;

		if (*b == 'S' && b[1] == 'I' && b[2] == 'G')
			b += 3;
		while (*a && *b && (*a == *b ||
		    (*a >= 'a' && *a <= 'z' ? *a - 32 : *a) == *b)) {
			a++;
			b++;
		}
		if (*a == 0 && *b == 0)
			return (sigs[i].num);
	}
	n = atoi(s);
	return (n);
}

int
main(int argc, char **argv)
{
	int sig = 15, i, first = 1, status = 0;

	if (argc > 1 && (argv[1][0] == '-' || argv[1][0] == '+') &&
	    argv[1][1] != 0) {
		if (argv[1][1] == 'l') {
			for (i = 0; sigs[i].name != 0; i++)
				printf("%s %d\n", sigs[i].name, sigs[i].num);
			exit(0);
		}
		sig = signum(argv[1] + 1);
		first = 2;
	}
	if (first >= argc) {
		fprintf(stderr, "usage: kill [-sig] pid ...\n");
		exit(1);
	}
	for (i = first; i < argc; i++) {
		pid_t pid = (pid_t)atoi(argv[i]);

		if (kill(pid, sig) < 0) {
			fprintf(stderr, "kill: %s: %s\n", argv[i],
			    strerror(errno));
			status = 1;
		}
	}
	exit(status);
}
