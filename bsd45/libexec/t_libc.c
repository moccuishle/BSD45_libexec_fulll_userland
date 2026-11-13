/*
 * t_proc - fork/vfork/wait4 smoke test (built as /test).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

static char *vav[3] = { "echo", "vfork-exec-ok", 0 };
static char *venv[2] = { "PATH=/bin", 0 };

static volatile int sig_got;

static void
on_usr1(int s)
{

	sig_got++;
	write(1, "sig: handler ran\n", 17);
}

static void
say(const char *s, int n)
{
	write(1, s, n);
}

int
main(int argc, char **argv)
{
	pid_t pid;
	int status;

	{
		struct termios t;
		int r = tcgetattr(0, &t);

		printf("tty: isatty=%d/%d/%d tcgetattr=%d\n",
		    isatty(0), isatty(1), isatty(2), r);
		if (r == 0)
			printf("tty: iflag=%x oflag=%x cflag=%x lflag=%x\n",
			    t.c_iflag, t.c_oflag, t.c_cflag, t.c_lflag);
	}


	{
		int fd, n2;
		char tb[64];

		fd = open("/tmp/t", O_WRONLY | O_CREAT | O_TRUNC, 0666);
		printf("tmpfs: open=%d\n", fd);
		if (fd >= 0) {
			n2 = write(fd, "tmpfs-data\n", 11);
			printf("tmpfs: wrote=%d\n", n2);
			close(fd);
		}
		if (stat("/tmp/t", &tb[0]) == 0)
			printf("tmpfs: stat ok\n");
		fd = open("/tmp/t", O_RDONLY);
		if (fd >= 0) {
			n2 = read(fd, tb, sizeof(tb));
			if (n2 > 0) {
				write(1, "tmpfs-read: ", 12);
				write(1, tb, n2);
			}
			close(fd);
		}
	}

	{
		int pfd[2];
		char b[32];
		int n;

		if (pipe(pfd) == 0) {
			write(pfd[1], "pipe-ok\n", 8);
			n = read(pfd[0], b, sizeof(b));
			if (n > 0) {
				write(1, "pipe: ", 6);
				write(1, b, n);
			}
			close(pfd[0]);
			close(pfd[1]);
		} else
			printf("pipe: failed\n");
	}

	say("S:fork\n", 7);
	pid = fork();
	if (pid == 0) {
		say("C:fork\n", 7);
		_exit(5);
	}
	pid = wait4(-1, &status, 0, (struct rusage *)0);
	printf("fork waited pid=%d code=%d\n", pid, WEXITSTATUS(status));

	say("S:vfork\n", 8);
	pid = vfork();
	if (pid == 0) {
		execve("/bin/echo", vav, venv);
		_exit(1);
	}
	say("P:vfork\n", 8);
	pid = wait4(-1, &status, 0, (struct rusage *)0);
	printf("vfork waited pid=%d code=%d\n", pid, WEXITSTATUS(status));

	/* ---------------- S10.2: signals ---------------- */
	{
		unsigned set;
		int st;
		pid_t kid;

		signal(SIGUSR1, on_usr1);
		sig_got = 0;
		kill(getpid(), SIGUSR1);
		printf("sig: handler count=%d\n", sig_got);

		signal(SIGUSR2, SIG_IGN);
		kill(getpid(), SIGUSR2);
		printf("sig: ignore ok\n");

		set = 1u << SIGUSR1;
		sig_got = 0;
		sigprocmask(SIG_BLOCK, (sigset_t *)&set, 0);
		kill(getpid(), SIGUSR1);
		printf("sig: blocked count=%d\n", sig_got);
		sigprocmask(SIG_UNBLOCK, (sigset_t *)&set, 0);
		printf("sig: unblocked count=%d\n", sig_got);

		/* default action: SIGTERM must kill the child */
		kid = fork();
		if (kid == 0) {
			for (;;)
				getpid();
			_exit(0);
		}
		kill(kid, SIGTERM);
		st = 0;
		wait4(kid, &st, 0, (struct rusage *)0);
		printf("sig: term signaled=%d sig=%d\n",
		    WIFSIGNALED(st), WIFSIGNALED(st) ? WTERMSIG(st) : -1);

		/* an ignoring child survives SIGTERM and exits normally */
		kid = fork();
		if (kid == 0) {
			signal(SIGTERM, SIG_IGN);
			kill(getpid(), SIGTERM);
			_exit(7);
		}
		st = 0;
		wait4(kid, &st, 0, (struct rusage *)0);
		printf("sig: ignore-child exited=%d code=%d\n",
		    WIFEXITED(st), WIFEXITED(st) ? WEXITSTATUS(st) : -1);
	}
	/*
	 * S10.1 stress: 50x ISO open/read/close, 50x tmpfs mkdir/rmdir and
	 * 50x "rewrite the same file" must not exhaust the vnode pool or the
	 * physical page pool.  The monitor's vstat shows the counters.
	 */
	{
		int i, bad;
		char buf[256];

		bad = 0;
		for (i = 0; i < 50; i++) {
			int fd = open("/readme.txt", O_RDONLY), n;

			if (fd < 0) {
				bad++;
				continue;
			}
			n = read(fd, buf, sizeof(buf));
			if (n <= 0)
				bad++;
			close(fd);
		}
		printf("s10: iso open/read/close 50x bad=%d\n", bad);

		bad = 0;
		for (i = 0; i < 50; i++) {
			char p[32];

			sprintf(p, "/tmp/d%d", i);
			if (mkdir(p, 0755) != 0)
				bad++;
			else if (rmdir(p) != 0)
				bad++;
		}
		printf("s10: tmpfs mkdir/rmdir 50x bad=%d\n", bad);

		bad = 0;
		for (i = 0; i < 50; i++) {
			int fd = open("/tmp/s10", O_WRONLY | O_CREAT | O_TRUNC,
			    0666);

			if (fd < 0) {
				bad++;
				continue;
			}
			if (write(fd, "x", 1) != 1)
				bad++;
			close(fd);
		}
		printf("s10: tmpfs rewrite 50x bad=%d\n", bad);
		unlink("/tmp/s10");
	}
	printf("done\n");
	exit(0);
}
