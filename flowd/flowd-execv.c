/*
 *  Synopsis:
 *	Execute command vectors read from stdin and write summaries on standard out 
 *  Usage:
 *	Invoked as a background worker by flowd server.
 *
 * 	#  to test at command line, do
 *	echo /bin/true | flowd-execv
 *	echo /bin/false | flowd-execv
 *	echo /bin/date | flowd-execv
 *	
 *	A summary of the exit status of the process is written to standard out:
 *	
 *		# normal process exit
 *		EXIT\t<exit-code>\t<user-seconds>\t<system-seconds>
 *
 *		# process was interupted by a signal
 *		SIG\t<signal>\t<user-seconds>\t<system-seconds>
 *
 *		# process got the stop signal and was killed
 *		STOP\t<stop-signal>\t<user-seconds>\t<system-seconds>
 *
 * 		A fatal error occured when execv'ing the process
 *		ERROR\t<error description>
 *	
 *	Fatal errors for flowd-execv are written to standard out and
 *	flowd-execv exits.
 *  Exit Status:
 *  	0	exit ok
 *  	1	exit error (written to standard error)
 *  Note:
 *	On ERROR, flowd-exec does not return user/system times.
 *
 *	Should the process be killed upon receiving a STOP signal?
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

#include <stdlib.h>		//  zap me when done debugging

#ifndef MAX_PIPE
#define MAX_PIPE	512
#endif

#define MAX_X_ARG	256
#define MAX_X_ARGC	64

static int	x_argc;

//  the argv for the execv()
static char	*x_argv[MAX_X_ARGC + 1];

static char	args[MAX_X_ARGC * (MAX_X_ARG + 1)];

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
	static char ERROR[] = "ERROR	";
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
_read(int fd, char *buf)
{
	ssize_t nb;

again:
	nb = read(fd, buf, MAX_PIPE);
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
	if (wait4(pid, statp, 0, ru) == pid)
		return;
	if (errno == EINTR)
		goto again;
	die2("wait4() failed", strerror(errno));
}

static void
_close(int fd)
{
again:
	if (close(fd) < 0) {
		if (errno == EINTR)
			goto again;
		die2("close() failed", strerror(errno));
	}
}

static void
_dup2(int old, int new)
{
again:
	if (dup2(old, new) < 0) {
		if (errno == EINTR)
			goto again;
		die2("dup2() failed", strerror(errno));
	}
}

//  drain child and copy first line of output into buf.

static int
drain(int child_fd, char *child_out) {

	int nb;
	int seen_line = 0;
	char *p, *q;
	char buf[MAX_PIPE + 1];

	p = child_out;
	while ((nb = _read(child_fd, buf)) > 0) {
		char c;

		if (seen_line)
			continue;
		buf[nb] = 0;
		q = buf;
		while ((p - child_out) < MAX_PIPE && (c = *q++)) {
			*p++ = c;
			if (c == '\n') {
				seen_line = 1;
				break;
			}
		}
	}
	*p = 0;
	if (p - child_out == MAX_PIPE)
		p[-1] = '\n';
	return p - child_out;
}

static void
fork_wait() {

	pid_t pid;
	int status;
	char reply[MAX_PIPE + 1], buf[MAX_PIPE + 1];
	struct rusage ru;
	char *xclass = 0;
	int xstatus = 0;
	ssize_t olen = 0;
	int merge[2];

	if (pipe(merge) < 0)
		die2("pipe(merge) failed", strerror(errno));
	pid = fork();
	if (pid < 0)
		die2("fork() failed", strerror(errno));

	//  in the child
	if (pid == 0) {
		_close(0);
		_close(merge[0]);
		_dup2(merge[1], 1);
		_dup2(merge[1], 2);
		_close(merge[1]);
		execv(x_argv[0], x_argv);
		die3("execv() failed", strerror(errno), x_argv[0]);
	}

	//  in parent
	_close(merge[1]);
	olen = drain(merge[0], buf);
	_close(merge[0]);

	//  reap the dead
	_wait4(pid, &status, &ru);

	//  Note: what about core dumps
	if (WIFEXITED(status)) {
		xclass = "EXIT";
		xstatus = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		xclass = "SIG";
		xstatus = WTERMSIG(status);
	} else if (WIFSTOPPED(status)) {
		xclass = "STOP";
		xstatus = WSTOPSIG(status);
	} else
		die("wait() exited with impossible value");

	snprintf(reply, sizeof reply, "%s%s\t%d\t%ld.%06ld\t%ld.%06ld\n",
			xclass,
			olen > 0 ? "+" : "",
			xstatus,
			ru.ru_utime.tv_sec,
			(long)ru.ru_utime.tv_usec,
			ru.ru_stime.tv_sec,
			(long)ru.ru_stime.tv_usec
	);
	_write(reply, strlen(reply));
	if (olen > 0)
		_write(buf, olen);
}

int
main(int argc, char **argv)
{
	char *arg, c = 0;
	char buf[MAX_PIPE + 1];
	int i;

	if (argc != 1)
		die("wrong number of arguments");
	(void)argv;

	for (i = 0;  i < MAX_X_ARGC;  i++)
		x_argv[i] = &args[i * (MAX_X_ARG + 1)];

	x_argc = 0;
	arg = x_argv[0];

	while (_read(0, buf) > 0) {
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
			arg = x_argv[x_argc];
			x_argv[x_argc] = 0;	//  null-terminate vector

			fork_wait();

			x_argv[x_argc] = arg;	//  reset null to arg buffer
			arg = x_argv[0];
			x_argc = 0;
		}
	}
	if (c != 0 && c != '\n')
		die("last read() char not new-line");
	return 0;
}
