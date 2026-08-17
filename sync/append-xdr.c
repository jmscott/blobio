/*
 *  Synopsis:
 *  	Atomic append of a xdr request record to a fifo or file.
 *  Exit Status:
 *	0	ok, append succeeded
 *	1	append failed
 *  See:
 *	README-xdr
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#include "jmscott/libjmscott.h"

#define	MAX_XDR		JMSCOTT_ATOMIC_WRITE_SIZE

static char		xdr[MAX_XDR + 1] = {0};		//  xdr + newline

char	*jmscott_progname = "append-xdr";

static void
die3(char *msg1, char *msg2, char *msg3)
{
	jmscott_die3(1, msg1, msg2, msg3);
	/*NOTREACHED*/
}

static void
die2(char *msg1, char *msg2)
{
	jmscott_die2(1, msg1, msg2);
	/*NOTREACHED*/
}

static void
die(char *msg)
{
	jmscott_die(1, msg);
	/*NOTREACHED*/
}

static char usage[] =
	"append-xdr "
	"<log-path> "
	"<start time> "
	"<flow-seq> "
	"<command-name> "
	"<process-id> "
	"<exit-class> "
	"<blob> "
	"<exit-code> "
	"<wall_duration_seconds> "
	"<ruser_seconds> "
	"<rsys_seconds>"
;

static void
help()
{
	write(2, usage, sizeof usage);
	write(2, "\n", 1);
	_exit(0);
}

static char *
arg_cat(char *arg, char *xp) 
{
	return jmscott_strcat2(xp, MAX_XDR - (xp - xdr), arg, "\t");
}

static char *
arg_uint64(char *what, char *arg, char *xp)
{
	size_t len = strlen(arg);
	if (len == 0)
		die2("empty arg", what);
	if (len > 20)
		die2("arg > 20 chars", what);

	for (char *ap =  arg;  *ap;  ap++)
		if (!isdigit(*ap))
			die2("char not digit", what);

	return arg_cat(arg, xp);
}

static char *
arg_exit_class(char *word, char *arg, char *xp)
{
	if (strcmp(word, arg))
		die2("unknown exit class", arg);
	return arg_cat(arg, xp);
}

static char *
arg_duration(char *what, char *arg, char *xp)
{
	for (char *ap = arg;  *ap;  ap++) {
		char c = *ap;

		if (!isdigit(c) && c != '.')
			die3("can not parse duration", what, arg);
	}
	return arg_cat(arg, xp);
}

int
main(int argc, char **argv)
{
	argc--;
	++argv;

	if (argc == 1 && (!strcmp(*argv, "--help") || !strcmp(*argv, "help")))
		help();

	if (argc != 11)
		jmscott_die_argc(1, argc, 11, usage);

	char *xp = xdr;
	
	//  exclusivly open() xdr file, create if not existing.

	char *log_path = argv[0];
	if (!*log_path)
		die("empty path/to/*.xdr");
	if (isdigit(*log_path))
		die2("log path starts with digit", log_path);

	//  frisk and append rfc3339 start time:
	//
	//	2026-08-16T00:19:00.892394-05:00

	char *start_time = argv[1];
	if (!*start_time)
		die("empty start stime");
	if (*start_time != '2')
		die2("do not recognize start time", start_time);
	xp = arg_cat(start_time, xp); 

	//  frisk and append flow sequence, always > 0

	//  uint63 flow seq

	char *flow_seq = argv[2];
	char *err = jmscott_a2ui63(flow_seq, (unsigned long long *)0);
	if (err)
		die3("can not parse flow sequence", err, flow_seq);
	xp = arg_cat(flow_seq, xp);

	//  command name: [a-z][a-z0-9_]{0,31}

	char *command_name = argv[3];
	if (!*command_name)
		die("empty command name");
	if (strlen(command_name) > 32)
		die2("command name > 32 chars", command_name);
	if (!islower(*command_name))
		die2("first char of command name not lower", command_name);
	for (char *cp = command_name + 1;  *cp;  cp++) {
		char c = *cp;

		if (!isdigit(c) && !islower(c) && !isdigit(c) && c != '_')
			die("char in command name not [a-z0-9_]");
	}
	xp = arg_cat(command_name, xp);

	xp = arg_uint64("process id", argv[4], xp);

	//  exit class: OK, ERR, NOPS, SIG

	char *exit_class = argv[5];
	if (!*exit_class)
		die("empty exit class");
	int len = strlen(exit_class);
	if (len > 5)
		die2("exit class > 5 chars", exit_class);

	switch (len) {

	//  OK
	case 2:
		xp = arg_exit_class("OK", exit_class, xp);
		break;

	//  ERR or SIG
	case 3:
		switch (exit_class[0]) {
		case 'E':
			xp = arg_exit_class("ERR", exit_class, xp);
			break;
		case 'S':
			xp = arg_exit_class("SIG", exit_class, xp);
			break;
		}
		break;
	//  NOPS
	case 4:
		xp = arg_exit_class("NOPS", exit_class, xp);
		break;
	default:
		die2("unknown exit class", exit_class);
		/*NOTREACHED*/
	}

	//  blob/udig

	char *blob = argv[6];
	if ((err = jmscott_frisk_udig(blob)))
		die2("can not parse blob", err);
	xp = arg_cat(blob, xp);

	char *exit_code = argv[7];
	unsigned long long ex;
	if ((err = jmscott_a2ui63(exit_code, &ex)))
		die2("can not parser exit code", exit_code);
	if (ex > 255)
		die2("exit code > 255", exit_code);
	xp = arg_cat(exit_code, xp);

	xp = arg_duration("wall duration", argv[8], xp);
	xp = arg_duration("user seconds", argv[9], xp);
	xp = arg_duration("sys seconds", argv[10], xp);

	*xp++ = '\n';

	//  open log file append only, creatre if not exist

	int fd = open(log_path,
		   O_WRONLY | O_APPEND | O_CREAT,
		   S_IRUSR | S_IWUSR | S_IRGRP
	);
	if (fd < 0)
		die3("open(xdr) failed", strerror(errno), log_path);
	if (jmscott_write_all(fd, xdr, xp - xdr))
		die2("write(xdr) failed", strerror(errno));
	if (jmscott_close(fd))
		die2("close(xdr) failed", strerror(errno));
	
	_exit(0);
}
