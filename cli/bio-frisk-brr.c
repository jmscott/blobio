/*
 *  Synopsis:
 *  	Frisk a set off blob request records read from stdin.
 *  Usage:
 *  	cat $BLOBIO_ROOT/spool/fs.brr | bio-frisk-brr
 *  Exit Status:
 *	0	ok, no erros
 *	1	error in blob request records, offend line written on stderr
 */
#include <stdio.h>
#include "jmscott/libjmscott.h"

char	jmscott_progname[] = "bio-frisk-brr";

#define MIN_BRR		95
#define MAX_BRR		371

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
