/*
 *  Synopsis:
 *  	Atomic append of a blob request record to a fifo or file.
 *  Usage:
 *  	append-brr							\
 *		/path/to/file.brr					\
 *  		start_request_time					\
 *		transport						\
 *		verb							\
 *		algorithm:digest					\
 *		chat_history						\
 *		blob_size						\
 *		wall_duration						\
 *  Exit Status:
 *	0	ok
 *	1	wrong number of arguments
 *	2	a brr field is too big in the argument list
 *	3	the open() failed
 *	4	the write() failed
 *	5	the close() failed
 *	6	a brr field is wrong syntax.
 *  See:
 *	https://github.com/jmscott/work/blob/master/RFC3339Nano.c
 *  Note:
 *  	No syntax checking is done on the fields of the blob request record.
 *  	Only field sizes are checked.
 *
 *	O_CREAT must be an option on the file open, otherwise rolling the file
 *	is problemmatic.  For example, without O_CREAT, the following
 *	trivial roll will fail for each append()
 *
 *		mv pdf.log pdf-$(date +'%a').log
 *		#  window when an error in append-brr could occur
 *
 *	since pdf.log disappears briefly.
 *
 *	Consider replacing write() with writev().
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#define EXIT_BAD_ARGC	1
#define EXIT_BAD_BRR	2
#define EXIT_BAD_OPEN	3
#define EXIT_BAD_WRITE	4
#define EXIT_BAD_CLOSE	5

static char	progname[] = "append-brr";

#define MIN_BRR		95
#define MAX_BRR		365

/*
 *  Synopsis:
 *	Common, interruptable system calls, die[123]() and _strcat().
 *  Usage:
 *	char *progname = "append-brr";
 *
 *	#define COMMON_NEED_READ
 *	#define COMMON_NEED_DIE3
 *	#include "../../common.c"
 *
 *	#define COMMON_NEED_READ
 *	#define EXIT_BAD_READ 4
 *	#include "../../common.c"
 *  See:
 *	flip-tail and c programs in schema directories.
 *  Note:
 *	Rename common.c to unistd.c?
 */

#ifndef PIPE_MAX
#define PIPE_MAX	512
#endif

//  force a compile time error if the atomic pipe buffer is too small for a brr

#if PIPE_MAX < MAX_BRR
-!(PIPE_MAX < MAX_BRR)
#endif

/*
 * Synopsis:
 *  	Fast, safe and simple string concatenator
 *  Usage:
 *  	buf[0] = 0
 *  	_strcat(buf, sizeof buf, "hello, world");
 *  	_strcat(buf, sizeof buf, ": ");
 *  	_strcat(buf, sizeof buf, "good bye, cruel world");
 */
static void
_strcat(char *tgt, int tgtsize, char *src)
{
	//  find null terminated end of target buffer
	while (*tgt++)
		--tgtsize;
	--tgt;

	//  copy non-null src bytes, leaving room for trailing null
	while (--tgtsize > 0 && *src)
		*tgt++ = *src++;

	// target always null terminated
	*tgt = 0;
}

/*
 *  Write error message to standard error and exit process with status code.
 */
static void
die(int status, char *msg1)
{
	char msg[PIPE_MAX];
	static char ERROR[] = "ERROR: ";
	static char	colon[] = ": ";
	static char nl[] = "\n";

	msg[0] = 0;
	_strcat(msg, sizeof msg, progname);
	_strcat(msg, sizeof msg, colon);
	_strcat(msg, sizeof msg, ERROR);
	_strcat(msg, sizeof msg, msg1);
	_strcat(msg, sizeof msg, nl);

	write(2, msg, strlen(msg));

	_exit(status);
}

static void
die2(int status, char *msg1, char *msg2)
{
	static char colon[] = ": ";
	char msg[PIPE_MAX];

	msg[0] = 0;
	_strcat(msg, sizeof msg, msg1);
	_strcat(msg, sizeof msg, colon);
	_strcat(msg, sizeof msg, msg2);

	die(status, msg);
}

static void
die3(int status, char *msg1, char *msg2, char *msg3)
{
	static char colon[] = ": ";
	char msg[PIPE_MAX];

	msg[0] = 0;
	_strcat(msg, sizeof msg, msg1);
	_strcat(msg, sizeof msg, colon);
	_strcat(msg, sizeof msg, msg2);
	_strcat(msg, sizeof msg, colon);
	_strcat(msg, sizeof msg, msg3);

	die(status, msg);
}

/*
 *  write() exactly nbytes bytes, restarting on interrupt and dieing on error.
 */
static void
_write(int fd, void *p, ssize_t nbytes)
{
	int nb = 0;

	again:

	nb = write(fd, p + nb, nbytes);
	if (nb < 0) {
		if (errno == EINTR)
			goto again;
		die2(EXIT_BAD_WRITE, "write() failed", strerror(errno));
	}
	nbytes -= nb;
	if (nbytes > 0)
		goto again;
}

/*
 *  open() a file.
 */
static int
_open(char *path, int oflag, int mode)
{
	int fd;

	again:

	fd = open(path, oflag, mode);
	if (fd < 0) {
		if (errno == EINTR)
			goto again;
		die3(EXIT_BAD_OPEN, "open() failed", strerror(errno), path);
	}
	return fd;
}

/*
 *  close() a file descriptor.
 */
static void
_close(int fd)
{
	again:

	if (close(fd) < 0) {
		if (errno == EINTR)
			goto again;
		die2(EXIT_BAD_CLOSE, "close() failed", strerror(errno));
	}
}

/*
 *  Build the brr record from a command line argument.
 *  Update the pointer to the position in the buffer.
 *
 *  Note:
 *	Need to be more strict about correctness of brr fields.
 *	For example, a char set per arg would be easy to add.
 */
static void
arg2brr(char *name, char *arg, size_t min_len, int size, char **brr)
{
	char *tgt, *src;
	int n;

	if (strlen(arg) < min_len)
		die3(EXIT_BAD_BRR, "argument too short", name, arg);
	n = 0;
	src = arg;
	tgt = *brr;
	while (*src && n++ < size)
		*tgt++ = *src++;
	if (n >= size && *src)
		die3(EXIT_BAD_BRR, "arg too big for brr field", name, arg); 
	*tgt++ = '\t';
	*brr = tgt;
}

static void
in_char_set(char *name, char *arg, char *set)
{
	char c;
	char *a = arg;

	//  verify all chars exist in the set
	//  Note:  can replace with a table lookup.

	while ((c = *a++)) {
		char *sp = set;
		while (*sp) {
			if (c == *sp)
				break;
			sp++;
		}
		if (*sp == 0)
			die3(EXIT_BAD_BRR, name, "illegal char exists", arg);
	}
}

static void
is_start_time(char *arg, char **brr)
{
	static char start_time_set[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		  '-', 'T', ':', '.', '+', 0
	};
	char *nm = "start time";

	arg2brr(nm, arg, 26,  10+1+8+1+9+1+6, brr);
	if (arg[4] != '-')
		die3(EXIT_BAD_BRR, nm, "dash not at pos 5", arg);
	if (arg[7] != '-')
		die3(EXIT_BAD_BRR, nm, "dash not at pos 7", arg);
	if (arg[10] != 'T')
		die3(EXIT_BAD_BRR, nm, "T not at pos 11", arg);
	if (arg[13] != ':')
		die3(EXIT_BAD_BRR, nm, "colon not at pos 14", arg);
	if (arg[16] != ':')
		die3(EXIT_BAD_BRR, nm, "colon not at pos 17", arg);
	if (arg[19] != '.')
		die3(EXIT_BAD_BRR, nm, "colon not at pos 20", arg);
	in_char_set(nm, arg, start_time_set);
}

static void
is_transport(char *arg, char **brr)
{
	char *nm = "transport";
	char c, *a;

	arg2brr(nm, arg, 3, 8+1+128, brr);
	a = arg;
	if (*a < 'a' || *a > 'z')
		die3(EXIT_BAD_BRR, nm, "protocol not ^[a-z]", arg);
	a++;

	//  check chars in protocol

	while ((c = *a) && c != '~') {
		if (a - arg > 7)
			die3(EXIT_BAD_BRR, nm, "protocol > 8 characters", arg);
		if (!isdigit(c) && !islower(c))
			die3(EXIT_BAD_BRR, nm, "protocol not [a-z0-9]", arg);
		a++;
	}
	if (c == 0)
		die3(EXIT_BAD_BRR, nm, "tilda not seen", arg);
	a++;
	if (strlen(a) > 128)
		die3(EXIT_BAD_BRR, nm, "flow: length > 128", arg);

	while ((c = *a++))
		if (!isgraph(c) || !isascii(c))
			die3(EXIT_BAD_BRR, nm, "flow: bad char", arg);
}

static void
is_udig(char *arg, char **brr)
{
	char *nm = "udig";
	char c, *a;
	size_t len;

	arg2brr(nm, arg, 1 + 1 + 28, 8+1+128, brr);
	a = arg;
	if (*a < 'a' || *a > 'z')
		die3(EXIT_BAD_BRR, nm, "algorithm not ^[a-z]", arg);
	a++;

	//  check digits in algorithm

	while ((c = *a) && c != ':') {
		if (a - arg > 7)
			die3(EXIT_BAD_BRR, nm, "algorithm > 8 characters", arg);
		if (!isdigit(c) && !islower(c))
			die3(EXIT_BAD_BRR, nm, "algorithm not [a-z0-9]", arg);
		a++;
	}
	if (c == 0)
		die3(EXIT_BAD_BRR, nm, "no colon at end of algorithm", arg);

	a++;
	len = strlen(a);
	if (len < 32)
		die3(EXIT_BAD_BRR, nm, "digest: length < 32", arg);
	if (len > 128)
		die3(EXIT_BAD_BRR, nm, "digest: length > 128", arg);

	while ((c = *a++))
		if (!isgraph(c) || !isascii(c))
			die3(EXIT_BAD_BRR, nm, "digest: bad char", arg);
}

static void
is_blob_size(char *arg, char **brr)
{
	static char digit_set[11] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		0
	};

	arg2brr("blob size", arg, 1, 19, brr);
	in_char_set("blob size", arg, digit_set);
}

static void
is_wall_duration(char *arg, char **brr)
{
	static char duration_set[12] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'.', 0
	};
	arg2brr("wall duration", arg, 1, 10+1+9, brr);
	in_char_set("wall duration", arg, duration_set);
}

int
main(int argc, char **argv)
{
	// room for brr + newline + null
	char brr[MAX_BRR + 1 + 1], *b;
	char *path;
	int fd;

	if (argc != 9)
		die(EXIT_BAD_ARGC, "wrong number arguments");
	path = argv[1];

	/*
	 *  Build the blob request record from command line arguments
	 */
	b = brr;

	is_start_time(argv[2], &b);
	is_transport(argv[3], &b);

	//  verb

	arg2brr("verb", argv[4], 3, 8, &b);
	if (strcmp("get", argv[4])					&&
	    strcmp("put", argv[4])					&&
	    strcmp("give", argv[4])					&&
	    strcmp("take", argv[4])					&&
	    strcmp("eat", argv[4])					&&
	    strcmp("wrap", argv[4])					&&
	    strcmp("roll", argv[4]))
		die2(EXIT_BAD_BRR, "not verb", argv[4]);

	is_udig(argv[5], &b);

	//  chat history

	arg2brr("chat history", argv[6], 2, 8, &b);
	if (strcmp("ok", argv[6])					&&
	    strcmp("no", argv[6])					&&
	    strcmp("ok,ok", argv[6])					&&
	    strcmp("ok,no", argv[6])					&&
	    strcmp("ok,ok,ok", argv[6])					&&
	    strcmp("ok,ok,no", argv[6]))
		die2(EXIT_BAD_BRR, "not chat history", argv[6]);

	is_blob_size(argv[7], &b);

	is_wall_duration(argv[8], &b);

	b[-1] = '\n';		//  zap trailing tab

	fd = _open(path,
		   O_WRONLY | O_APPEND | O_CREAT,
		   S_IRUSR | S_IWUSR | S_IRGRP
	);

	if (b - brr < MIN_BRR)
		die(EXIT_BAD_BRR, "brr < 95 bytes");

	// atomically write exactly the number bytes in the blob request record
	_write(fd, brr, b - brr);
	_close(fd);
	_exit(0);
}
