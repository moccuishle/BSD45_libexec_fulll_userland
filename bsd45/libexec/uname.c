/* uname(1) - print system information (S14). */
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

int
main(int argc, char **argv)
{
	struct utsname u;
	int all = 0, sflag = 0, nflag = 0, rflag = 0, vflag = 0, mflag = 0;
	int i, first = 1;
	char *p;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			continue;
		for (p = argv[i] + 1; *p; p++) {
			switch (*p) {
			case 'a': all = 1; break;
			case 's': sflag = 1; break;
			case 'n': nflag = 1; break;
			case 'r': rflag = 1; break;
			case 'v': vflag = 1; break;
			case 'm': mflag = 1; break;
			default: break;
			}
		}
	}
	if (all)
		sflag = nflag = rflag = vflag = mflag = 1;
	if (!sflag && !nflag && !rflag && !vflag && !mflag)
		sflag = 1;

	if (uname(&u) < 0) {
		printf("uname: cannot get system name\n");
		return (1);
	}
	if (sflag) {
		printf("%s", u.sysname);
		first = 0;
	}
	if (nflag) {
		printf("%s%s", first ? "" : " ", u.nodename);
		first = 0;
	}
	if (rflag) {
		printf("%s%s", first ? "" : " ", u.release);
		first = 0;
	}
	if (vflag) {
		printf("%s%s", first ? "" : " ", u.version);
		first = 0;
	}
	if (mflag)
		printf("%s%s", first ? "" : " ", u.machine);
	putchar('\n');
	return (0);
}
