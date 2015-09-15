/*
 *  Synopsis:
 *	Execute processes on behalf of the flowd process.
 *  Usage:
 *	Invoked by flowd server.  See Note below.
 *  Exit Status:
 *  	0	exit ok
 *  	1	exit error (written to standard error)
 *  Note:
 *	The read() from stdin assumes final char is \n, since coordination
 *	with the flowd is synchronous.  However, this assumptioon will break
 *	a streaming algorithm.	In other words, the following will not work
 *
 *		flowd-os-exec <multi-line-test-file
 *
 *	This assumption is too strict, especially for testing.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define MAX_SLURP	4096
#define MAX_X_ARG	256
#define MAX_X_ARGC	64

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

/*
 *  read() up to 'nbytes' from standard input, croaking upon error
 */

static int
_read(void *p, ssize_t nbytes)
{
	ssize_t nb;

	again:

	nb = read(0, p, nbytes);
	if (nb >= 0)
		return nb;
	if (errno == EINTR)		//  try read()
		goto again;

	die2("read(0) failed", strerror(errno));

	/*NOTREACHED*/
	return -1;
}

/*
 *  write() exactly nbytes to standard output or croak with error
 */
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

#define ST_READ		0
#define ST_EXEC		1
#define ST_START_ARG	2
#define ST_ARG		3
#define ST_COPY_ARG	4

int
main(int argc, char **argv)
{
	char buf[MAX_SLURP + 1];
	int nread = 0;
	int state = ST_READ;
	char *p, *p_end;
	char *a_start;
	int x_argc = 0;
	char *x_argv[MAX_X_ARGC];

	if (argc != 1)
		die("wrong number of arguments");
	(void)argv;

tick:
	switch (state) {
	case ST_READ:
		nread = _read(buf, MAX_SLURP);
		if (nread == 0) {
			if (x_argc > 0)
				die("read: unexpected end of file");
			_exit(0);
		}
		p = buf;
		p_end = p + nread;
		state = ST_START_ARG;
		break;
	case ST_EXEC:
		if (x_argc == 0)
			die("exec: no exec arguments");
		_write(buf, nread);
		state = ST_START_ARG;
		break;
	case ST_START_ARG:
		if (x_argc++ > MAX_X_ARGC)
			die("start_arg: too many arguments in input");
		if (p == p_end) {
			if (x_argc > 0)
				state = ST_COPY_ARG;  // implies args < 2 reads
			else
				state = ST_READ;
		} else {
			a_start = p;
			state = ST_ARG;
		}
		break;
	case ST_ARG: {
		char c = *p++;

		if (p - a_start > MAX_X_ARG)
			die("arg too long");
		if (!isascii(c))
			die("non-ascii char in arg");
		if (c == '\t' || c == '\n') {
			*p = 0;
			x_argv[x_argc++] = a_start;
			if (c == '\t')
				state = ST_START_ARG;
			else
				state = ST_EXEC;
			break;
		}

		break;
	}
	default:
		die("impossible input state");
	}
	goto tick;
}
