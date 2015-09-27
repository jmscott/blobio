/*
 *  Synopsis:
 *	Execute processes on behalf of the flowd process (experimental).
 *  Usage:
 *	Invoked as a background worker by flowd server.
 *  Exit Status:
 *  	0	exit ok
 *  	1	exit error (written to standard error)
 *  Note:
 *	The read() from stdin is assumed to be atomic and terminated by a
 *	new-line char (\n).  This assumption makes sense when invoked by flowd,
 *	since coordination is stricly synchronized on i/o flow. This implies
 *	that the following command line invocation could fail:
 *
 *		flowd-os-exec <multi-line-test-file
 *
 *	This is a sad situation, since trivial testing from the shell will
 *	break.  However, the following command line invocation ought to never
 *	fail:
 *
 *		grep --line-buffered '.*' multi-line-test-file | flowd-os-exec
 *
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include <sys/times.h>
#include <sys/wait.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#ifndef MAX_PIPE
#define MAX_PIPE	512
#endif

#define MAX_X_ARG	256
#define MAX_X_ARGC	64

static char	buf[MAX_PIPE + 1];
static int	x_argc;

static double ticks_per_sec;

//  the argv for the execv()
static char	*x_argv[MAX_X_ARGC + 1];

static char	args[MAX_X_ARGC * (MAX_X_ARG +1 )];

/*
 *  Synopsis:
 *  	Safe & simple string concatenator
 *  Returns:
 * 	Number of non-null bytes consumed by buffer.
 *  Usage:
 *  	buf[0] = 0
 *  	_strcat(buf, sizeof buf, "hello, world");
 *  	_strcat(buf, sizeof buf, ": ");
 *  	_strcat(buf, sizeof buf, "good bye, cruel world");
 *  	write(2, buf, _strcat(buf, sizeof buf, "\n"));
 */

static int
_strcat(char *tgt, int tgtsize, char *src)
{
	char *tp = tgt;

	//  find null terminated end of target buffer
	while (*tp++)
		--tgtsize;
	--tp;

	//  copy non-null src bytes, leaving room for trailing null
	while (--tgtsize > 0 && *src)
		*tp++ = *src++;

	// target always null terminated
	*tp = 0;

	return tp - tgt;
}

static void
die(char *msg1)
{
	static char ERROR[] = "flowd-os-exec: ERROR: ";
	char msg[256] = {0};

	_strcat(msg, sizeof msg, ERROR);
	_strcat(msg, sizeof msg, msg1);

	write(2, msg, _strcat(msg, sizeof msg, "\n"));

	_exit(1);
}

static void
die2(char *msg1, char *msg2)
{
	char msg[256] = {0};

	_strcat(msg, sizeof msg, msg1);
	_strcat(msg, sizeof msg, ": ");
	_strcat(msg, sizeof msg, msg2);

	die(msg);
}

static void
die3(char *msg1, char *msg2, char *msg3)
{
	char msg[256] = {0};

	_strcat(msg, sizeof msg, msg1);
	_strcat(msg, sizeof msg, ": ");
	_strcat(msg, sizeof msg, msg2);

	die2(msg, msg3);
}

/*
 *  read() up to 'nbytes' from standard input, croaking upon error
 */

static int
_read()
{
	ssize_t nb;

	again:

	nb = read(0, buf, MAX_PIPE);
	if (nb >= 0) {
		buf[nb] = 0;
		return nb;
	}
	if (errno == EINTR)		//  try read()
		goto again;

	die2("read(0) failed", strerror(errno));

	/*NOTREACHED*/
	return -1;
}

static void
_write(void *p, ssize_t nbytes)
{
	int nb = 0;

	again:

	nb = write(1, p + nb, nbytes);
	if (nb < 0) {
		if (errno == EINTR)
			goto again;
		die2("write(1) failed", strerror(errno));
	}
	if (nb == 0)
		die("write(1) wrote 0 bytes");
	nbytes -= nb;
	if (nbytes == 0)
		return;
	goto again;
}

static void
_wait4(pid_t pid, int *statp, struct rusage *ru)
{
again:
	if (wait4(pid, statp, WEXITED, ru) == pid)
		return;
	if (errno == EINTR)
		goto again;
	die2("wait4() failed", strerror(errno));
}

static void
_execv() {

	execv(x_argv[0], x_argv);
	die3("execv() failed", strerror(errno), x_argv[0]);
}

static void
fork_wait() {

	pid_t pid;
	int status;
	char reply[MAX_PIPE + 1];
	struct rusage ru;
	char *xclass = 0;
	int xstatus = 0;

	pid = fork();
	if (pid < 0)
		die2("fork() failed", strerror(errno));
	if (pid == 0)
		_execv();

	_wait4(pid, &status, &ru);

	//  Note: what about core dumps
	if (WIFEXITED(status)) {
		xclass = "EXIT";
		xstatus = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		xclass = "SIG";
		xstatus = WTERMSIG(status);
	} else if (WIFSIGNALED(status)) {
		xclass = "STOP";
		xstatus = status;
	} else
		die("wait() exited with impossible value");

	snprintf(reply, sizeof reply, "%s\t%d\t%ld.%06d\t%ld.%06d\n",
			xclass,
			xstatus,
			ru.ru_utime.tv_sec,
			ru.ru_utime.tv_usec,
			ru.ru_stime.tv_sec,
			ru.ru_stime.tv_usec
	);

	_write(reply, strlen(reply));
}

int
main(int argc, char **argv)
{
	char *arg, c = 0;

	if (argc != 1)
		die("wrong number of arguments");
	(void)argv;

	//  fetch ticks per second for user/system times

	{
		long int sv = sysconf(_SC_CLK_TCK);
		if (sv < 0)
			die2("sysconf(_SC_CLK_TCK) failed", strerror(errno));
		ticks_per_sec = (double)sv;
	}

	//  initialize x_argv vector
	{
		int i;

		for (i = 0;  i < MAX_X_ARGC;  i++)
			x_argv[i] = &args[i * (MAX_X_ARG + 1)];
	}

	x_argc = 0;
	arg = x_argv[0];

	while (_read() > 0) {
		char *p;

		p = buf;

		while ((c = *p++)) {
			switch (c) {

			//  finished parsing an element of string vector
			case '\t':
				*arg = 0;
				x_argc++;
				if (x_argc > MAX_X_ARGC)
					die("argc too big");
				arg = x_argv[x_argc];
				continue;

			//  parsed final element of string vector, so exec()
			case '\n':
				*arg = 0;
				x_argc++;
				break;

			//  partial parse of an element in the string vector
			default:
				if (!isascii(c))
					die("non-ascii input");
				if (arg - x_argv[x_argc] > MAX_X_ARG)
					die("arg too big");
				*arg++ = c;
				continue;
			}
			x_argc++;
			arg = x_argv[x_argc];
			x_argv[x_argc] = 0;	//  null-terminate vector
			fork_wait();
			x_argv[x_argc] = arg;	//  reset to arg buffer
			arg = x_argv[0];
			x_argc = 0;
		}
	}
	if (c != 0 && c != '\n')
		die("last read() char not new-line");
	return 0;
}
