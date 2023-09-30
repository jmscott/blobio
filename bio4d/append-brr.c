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
 *	0	ok, append succeeded
 *	1	append failed
 *  See:
 *	https://github.com/jmscott/work/blob/master/RFC3339Nano.c
 *  Note:
 *	Need to rewrite using $JMSCOTT_ROOT/lib/libjmscott.a!
 *
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

#include "jmscott/libjmscott.h"

#define EXIT_BAD_ARGC	1
#define EXIT_BAD_BRR	2
#define EXIT_BAD_OPEN	3
#define EXIT_BAD_WRITE	4
#define EXIT_BAD_CLOSE	5

char	*jmscott_progname = "append-brr";

#define MIN_BRR		35
#define MAX_BRR		419

/*
 *  open() a file.
 */
static int
_open(char *path, int oflag, int mode)
{
	int fd;

	fd = jmscott_open(path, oflag, mode);
	if (fd < 0)
		jmscott_die3(
			EXIT_BAD_OPEN,
			"open() failed",
			strerror(errno),
			path
		);
	return fd;
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
		jmscott_die3(EXIT_BAD_BRR, "argument too short", name, arg);
	n = 0;
	src = arg;
	tgt = *brr;
	while (*src && n++ < size)
		*tgt++ = *src++;
	if (n >= size && *src)
		jmscott_die3(
			EXIT_BAD_BRR,
			"arg too big for brr field",
			name,
			arg
		); 
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
			jmscott_die3(
				EXIT_BAD_BRR,
				name,
				"illegal char exists",
				arg
			);
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
		jmscott_die3(EXIT_BAD_BRR, nm, "dash not at pos 5", arg);
	if (arg[7] != '-')
		jmscott_die3(EXIT_BAD_BRR, nm, "dash not at pos 7", arg);
	if (arg[10] != 'T')
		jmscott_die3(EXIT_BAD_BRR, nm, "T not at pos 11", arg);
	if (arg[13] != ':')
		jmscott_die3(EXIT_BAD_BRR, nm, "colon not at pos 14", arg);
	if (arg[16] != ':')
		jmscott_die3(EXIT_BAD_BRR, nm, "colon not at pos 17", arg);
	if (arg[19] != '.')
		jmscott_die3(EXIT_BAD_BRR, nm, "colon not at pos 20", arg);
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
		jmscott_die3(EXIT_BAD_BRR, nm, "protocol not ^[a-z]", arg);
	a++;

	//  check chars in protocol

	while ((c = *a) && c != '~') {
		if (a - arg > 7)
			jmscott_die3(
				EXIT_BAD_BRR,
				nm,
				"protocol > 8 characters",
				arg
			);
		if (!isdigit(c) && !islower(c))
			jmscott_die3(
				EXIT_BAD_BRR,
				nm,
				"protocol not [a-z0-9]",
				arg
			);
		a++;
	}
	if (c == 0)
		jmscott_die3(EXIT_BAD_BRR, nm, "tilda not seen", arg);
	a++;
	if (strlen(a) > 128)
		jmscott_die3(EXIT_BAD_BRR, nm, "flow: length > 128", arg);

	while ((c = *a++))
		if (!isgraph(c) || !isascii(c))
			jmscott_die3(EXIT_BAD_BRR, nm, "flow: bad char", arg);
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
		jmscott_die3(EXIT_BAD_BRR, nm, "algorithm not ^[a-z]", arg);
	a++;

	//  check digits in algorithm

	while ((c = *a) && c != ':') {
		if (a - arg > 7)
			jmscott_die3(
				EXIT_BAD_BRR,
				nm,
				"algorithm > 8 characters",
				arg
			);
		if (!isdigit(c) && !islower(c))
			jmscott_die3(EXIT_BAD_BRR, nm, "algorithm not [a-z0-9]", arg);
		a++;
	}
	if (c == 0)
		jmscott_die3(EXIT_BAD_BRR, nm, "no colon at end of algorithm", arg);

	a++;
	len = strlen(a);
	if (len < 32)
		jmscott_die3(EXIT_BAD_BRR, nm, "digest: length < 32", arg);
	if (len > 128)
		jmscott_die3(EXIT_BAD_BRR, nm, "digest: length > 128", arg);

	while ((c = *a++))
		if (!isgraph(c) || !isascii(c))
			jmscott_die3(EXIT_BAD_BRR, nm, "digest: bad char", arg);
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

static char usage[] =
	"append-brr "
	"<path> "
	"<time> "
	"<transport> "
	"<verb> "
	"<udig> "
	"<chat> "
	"<size> "
	"<wall> "
;

int
main(int argc, char **argv)
{
	// room for brr + newline + null
	char brr[MAX_BRR + 1 + 1], *b;
	char *path;
	int fd;

write(2, "WTF1\n", 5);
	if (argc != 9)
		jmscott_die_argc(EXIT_BAD_ARGC, argc, 9, usage);
write(2, "WTF2\n", 5);

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
	    strcmp("cat", argv[4])					&&
	    strcmp("roll", argv[4]))
		jmscott_die2(EXIT_BAD_BRR, "not verb", argv[4]);

	is_udig(argv[5], &b);

	//  chat history

	arg2brr("chat history", argv[6], 2, 8, &b);
	if (strcmp("ok", argv[6])					&&
	    strcmp("no", argv[6])					&&
	    strcmp("ok,ok", argv[6])					&&
	    strcmp("ok,no", argv[6])					&&
	    strcmp("ok,ok,ok", argv[6])					&&
	    strcmp("ok,ok,no", argv[6]))
		jmscott_die2(EXIT_BAD_BRR, "not chat history", argv[6]);

	is_blob_size(argv[7], &b);

	is_wall_duration(argv[8], &b);

	b[-1] = '\n';		//  zap trailing tab

	fd = _open(path,
		   O_WRONLY | O_APPEND | O_CREAT,
		   S_IRUSR | S_IWUSR | S_IRGRP
	);

	if (b - brr < MIN_BRR)
		jmscott_die(EXIT_BAD_BRR, "brr < 35 bytes");

	// atomically write exactly the number bytes in the blob request record
	if (jmscott_write_all(fd, brr, b - brr))
		jmscott_die2(
			EXIT_BAD_WRITE,
			"write(brr) filed",
			strerror(errno)
	);
	if (jmscott_close(fd))
		jmscott_die2(
			EXIT_BAD_CLOSE,
			"close(brr out) failed",
			strerror(errno)
		);
	_exit(0);
}
