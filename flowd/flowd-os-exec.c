/*
 *  Synopsis:
 *	Execute processes on behalf of the flowd process (experimental).
 *  Usage:
 *	Invoked by flowd server and still experimental.
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

//  execv(&x_argv[1]), since x_argv[0] == flow seq#
static char	*x_argv[MAX_X_ARGC + 2];	//  x_argv[0] == flow seq#

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
	if (nb >= 0)
		return nb;
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
push_arg(char *p, char *a) {

	if (x_argc >= MAX_X_ARG)
		die("too many arguments in execvp()");
	*--p = 0;
	x_argv[x_argc++] = a;
}

static void
_execv() {

	execv(x_argv[0], x_argv);
	die3("execv() failed", strerror(errno), x_argv[0]);
}

static void
vfork_wait() {

	pid_t pid;
	int status;
	char reply[MAX_PIPE + 1];

	pid = vfork();
	if (pid < 0)
		die2("vfork() failed", strerror(errno));
	if (pid == 0)
		_execv();
	wait(&status);

	snprintf(reply, sizeof reply, "%d\n", status);

	_write(reply, strlen(reply));
}

int
main(int argc, char **argv)
{
	if (argc != 1)
		die("wrong number of arguments");
	(void)argv;

	while (_read() > 0) {
		
		char *a, *p, c;
		x_argc = 0;

		a = p = buf;
		while ((c = *p++)) {
			switch (c) {
			//  finished parsing an element of string vector
			case '\t':
				push_arg(p, a);
				a = p;
				continue;

			//  parsed final element of string vector, so exec()
			case '\n':
				push_arg(p, a);
				x_argv[x_argc] = 0;
				break;

			//  partial parse of an attribute in the string vector
			default:
				if (!isascii(c))
					die("non-ascii input");
				if (p - a > MAX_X_ARG)
					die("arg too big");
				continue;
			}
			vfork_wait();
		}
	}
	return 0;
}
