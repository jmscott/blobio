/*
 *  Synopsis:
 *	Client to get/put/give/take/eat/wrap/roll blobs from blobio services.
 *  Exit Status:
 *  	0	request succeed (ok).
 *  	1	request denied (no).
 *	2	timeout
 *	3	unexpected error.
 *  Options:
 *	--service name:end_point
 *	--udig algorithm:digest
 *	--algorithm name
 *	--input-path <path/to/file>
 *	--output-path <path/to/file>
 *	--help
 *  Note:
 *	Investigate linux system calls splice(), sendfile() and
 *	copy_file_range().
 *
 *	The following fails with exit 1 for service fs:/usr/local/blobio
 *	when blob actually exists but output dir does not!
 *
 *		blobio get						\
 *			--udig <exists>					\
 *			--output-path <path/to/non/dir>			\
 *			--service fs:/usr/local/blobio
 *
 *	Under OSX 10.9, an exit status 141 in various shells can indicate a
 *	SIGPIPE interupted the execution.  blobio does not exit 141.
 *	A SIGPIPE when reading from stdin probably out to be considered a
 *	cancel.  Currently a SIGPIPE generated an error about the input
 *	not matching the digest, which is confusing.
 *
 *  	Options desperately need to be folding into a data structure.
 *  	We refuse to use getopts.
 *
 *	Also, the take&give exit statuses ought to reflect the various ok/no
 *	chat histories or perhaps the exit status ought to also store the
 *	verb.  See exit status for child process requests in the blobio
 *	server.  a bit map may be interesting.
 *
 *	Would be nice to eliminate stdio dependencies.  Currently stdio is
 *	required by only trace.c.  perhaps compile time option for --trace.
 *
 *	The service structure only understands ascii [:graph:] chars,
 *	which will be a problem for UTF8 DNS host names.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <strings.h>

#include "blobio.h"

#define BRR_SIZE	370		//  terminating null NOT counted

char	*jmscott_progname = "blobio";

extern int	tracing;

/*
 *  The global request.
 */
char	*verb;
char	algorithm[9] = {0};
char	ascii_digest[129] = {0};
char	*output_path = 0;
char	*input_path = 0;
char	brr_path[64] = {0};
char	*null_device = "/dev/null";
char	chat_history[10] = {0};
char	transport[129] = {0};
int	timeout = 20;

unsigned long long	blob_size;

struct timespec	start_time;
int trust_fd = -1;

// standard in/out must remain open before calling cleanup()

int	input_fd = 0;
int	output_fd = 1;
int	trust_fs = -1;

extern struct digest		*digests[];
extern struct service		*services[];
struct digest			*digest_module;

static struct service		*service = 0;

static char		usage[] =
	"usage: blobio [help | get|put|give|take|eat|wrap|roll|empty] "
	"[options]\n"
;

static char	*brr_format =
	"%04d-%02d-%02dT%02d:%02d:%02d.%09ld+00:00\t"	//  RFC3339Nano time
	"%s\t"						//  transport
	"%s\t"						//  verb
	"%s%s%s\t"					//  algorithm(:digest)?	
	"%s\t"						//  chat history
	"%llu\t"					//  byte count
	"%ld.%09ld\n"					//  wall duration
;

static void
ecat(char *buf, int size, char *msg)
{
	bufcat(buf, size, jmscott_progname);
	if (verb)
		buf2cat(buf, size, ": ", verb);
	if (service)
		buf2cat(buf, size, ": ", service->name);
	buf2cat(buf, size, ": ERROR: ", msg);
}

static void
cleanup(int status)
{
	if (service) {
		service->close();
		service = (struct service *)0;
	}

	uni_close(input_fd);

	//  Note: what about closing the digest?

	//  upon error, unlink() file created by --output-path,
	//  grumbling if unlink() fails.
	if (status > 1 && 
	    output_path && output_path != null_device &&
	    uni_unlink(output_path))
		if (errno != ENOENT) {
			static char panic[] =
				"PANIC: unlink(output_path) failed: ";
			char buf[MAX_ATOMIC_MSG];
			char *err;

			err = strerror(errno);

			//  assemble error message about failed
			//  unlink()

			buf[0] = 0;
			ecat(buf, sizeof buf, panic);
			buf2cat(buf, sizeof buf, err, "\n");

			uni_write(2, buf, strlen(buf));
		}
	TRACE("good bye, cruel world");
}

static void
help()
{
	static char blurb[] = "Usage:\n\
	blobio [get|put|give|take|eat|wrap|roll|empty | help] [options]\n\
Options:\n\
	--service       request blob from service <name:end_point>\n\
	--input-path    put/give/eat source file <default stdin>\n\
	--output-path   get/take target file <default stdout>\n\
	--udig          algorithm:digest for get/put/give/take/eat/empty/roll\n\
	--algorithm     algorithm name for local eat request\n\
	--trace		trace i/o to standard error\n\
	--help\n\
Exit Status:\n\
	0	request succeed\n\
  	1	request denied.  blob may not exist or is not empty\n\
	2	timeout on i/o\n\
	3	unexpected error\n\
Examples:\n\
	S=bio4:localhost:1797\n\
	UD=sha:a0bc76c479b55b5af2e3c7c4a4d2ccf44e6c4e71\n\
\n\
	blobio get --service $S --udig $UD --output-path tmp.blob\n\
\n\
	blobio take --service $S --udig $UD --output-path /dev/null\n\
\n\
	UDIG=$(blobio eat --algorithm sha --input-path resume.pdf)\n\
	blobio put --udig $UD --input-path resume.pdf --service $S\n\
Digest Algorithms:\n\
";
	write(1, blurb, strlen(blurb));

	//  write digest algorithms
	for (int j = 0;  digests[j];  j++) {
		if (j == 0)
			write(1, "\t", 1);
		else
			write(1, ", ", 2);
		write(1, digests[j]->algorithm, strlen(digests[j]->algorithm));
	}
	write(1, "\n", 1);
	cleanup(0);
	exit(0);
}

/*
 *  Format an error message like
 *
 * 	blobio: ERROR: <verb>: <message>\n
 *
 *  and write to standard error and exit.
 */
void
die(char *msg)
{
	cleanup(3);
	jmscott_die(3, msg);
}

void
die2(char *msg1, char *msg2)
{
	cleanup(3);
	jmscott_die2(3, msg1, msg2);
}

static void
die3(char *msg1, char *msg2, char *msg3)
{
	cleanup(2);
	jmscott_die3(3, msg1, msg2, msg3);
}

void
die_timeout(char *msg)
{
	cleanup(2);
	jmscott_die(2, msg);
}

/*
 *  Synopsis:
 *	Parse out the algorithm and digest from a text uniform digest (udig).
 *  Args:
 *	udig:		null terminated udig string to parse
 *	algorithm:	at least 9 bytes for extracted algorithm. 
 *	digest:		at least 129 bytes for extracted digest
 *  Returns:
 *	(char *)0	upon success
 *	error string	upon failure
 */
static char *
parse_udig(char *udig)
{
	char *u, *a, *d;
	int in_algorithm = 1;

	if (!udig[0])
		return "empty udig";

	a = algorithm;
	d = ascii_digest;

	for (u = udig;  *u;  u++) {
		char c = *u;

		if (!isascii(c) || !isgraph(c))
			return "non ascii or graph char";
		/*
		 *  Scanning algorithm.
		 */
		if (in_algorithm) {
			if (c == ':') {
				if (a == algorithm)
					return "missing <algorithm>";
				*a = 0;
				in_algorithm = 0;
			}
			else {
				if (a - algorithm >= 8)
					return "no colon at end of <algorithm>";
				*a++ = c;
			}
		} else {
			/*
			 *  Scanning digest.
			 */
			if (d - ascii_digest > 128)
				return "digest > 128 chars";
			*d++ = c;
		}
	}
	*d = 0;
	if (d - ascii_digest < 32)
		return "digest < 32 characters";
	return (char *)0;
}

//  a fatal error parsing command line options.

static void
eopt(const char *option, char *why)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;

	buf2cat(buf, sizeof buf, "option --", option);

	if (why)
		buf2cat(buf, sizeof buf, ": ", why);
	else
		bufcat(buf, sizeof buf, "<null why message>");
	die(buf);
}

//  a fatal error parsing command line options: the two arg version.

static void
eopt2(char *option, char *why1, char *why2)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	if (why1)
		bufcat(buf, sizeof buf, why1);
	if (why2) {
		if (buf[0])
			bufcat(buf, sizeof buf, ": ");
		bufcat(buf, sizeof buf, why2);
	}
	eopt(option, buf);
}

//  a fatal error parsing the BLOBIO_SERVICE option.

static void
eservice(char *why1, char *why2)
{
	if (why2 && why2[0])
		eopt2("service", why1, why2);
	else
		eopt("service", why1);
}

//  a fatal error when an option is missing.

static void
no_opt(char *option)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	buf2cat(buf, sizeof buf, "missing required option: --", option);

	die(buf);
}

//  a fatal error when a command line option has been given more than once.

static void
emany(char *option)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	buf2cat(buf, sizeof buf, "option given more than once: --", option);
	die(buf);
}

//  a fatal error that a command line option is not needed.

static void
enot(char *option)
{
	eopt(option, "not needed");
}

/*
 *  Parse the command line arguments.
 */
static void
parse_argv(int argc, char **argv)
{
	int i;
	char *err;

	if (argc == 1) {
		uni_write(2, usage, strlen(usage));
		die("no command line arguments");
	}
	if (strcmp("help", argv[1]) == 0 || strcmp("--help", argv[1]) == 0)
		help();

	//  first argument is always the request verb

	if (strcmp("get",   argv[1]) != 0 &&
	    strcmp("put",   argv[1]) != 0 &&
	    strcmp("eat",   argv[1]) != 0 &&
	    strcmp("give",  argv[1]) != 0 &&
	    strcmp("take",  argv[1]) != 0 &&
	    strcmp("wrap",  argv[1]) != 0 &&
	    strcmp("roll",  argv[1]) != 0 &&
	    strcmp("empty", argv[1]) != 0)
	    	die2("unknown verb", argv[1]);

	verb = argv[1];

	//  parse command line arguments after the request verb

	for (i = 2;  i < argc;  i++) {
		char *a = argv[i];

		//  all options start with --

		if (*a++ != '-' || *a++ != '-')
			die2("unknown option", argv[i]);

		/*
		 *  Options:
		 *	--udig algorithm:digest
		 *	--service protocol:end_point
		 *	--algorithm [btc20|sha|bc160]
		 *	--input-path <path/to/file>
		 *	--output-path <path/to/file>
		 *	--trace
		 *	--help
		 */

		//  --udig <udig>

		if (strcmp("udig", a) == 0) {

			//  udig syntax:
			//	^[a-z][a-z0-9]{0,7}:[[:isgraph:]]{32,128}$

			char *ud;
			int j;
			struct digest *d;

			//  option --udig given more than once.

			if (ascii_digest[0])
				emany("udig");

			// both --udig and --algorithm can't be defined at once

			if (algorithm[0])
				eopt("udig", "option --algorithm conflicts");
			if (++i == argc)
				eopt("udig", "missing <algorithm:digest>");
			ud = argv[i];

			err = parse_udig(ud);
			if (err)
				eopt2("udig", err, ud);

			//  find the digest module for algorithm.

			for (j = 0;  digests[j];  j++)
				if (strcmp(algorithm,
				      digests[j]->algorithm) == 0)
					break;
			d = digests[j];
			if (!d)
				eopt2("udig", "unknown algorithm", algorithm);

			//  verify the digest is syntactically well formed

			if (ascii_digest[0] && d->syntax() == 0)
				eopt2(
					"udig",
					"bad syntax for digest",
					ascii_digest
				);
			digest_module = d;

		//  --algorithm [a-z][a-z0-9]{0,7}

		} else if (strcmp("algorithm", a) == 0) {
			int j;
			char *n;

			if (ascii_digest[0])
				eopt("algorithm", "option --udig conflicts");
			if (algorithm[0])
				emany("algorithm");
			if (++i == argc)
				eopt("algorithm", "missing <name>");

			//  find the digest module for udig algorithm
			n = argv[i];
			if (!*n)
				eopt("algorithm", "empty name");
			for (j = 0;  digests[j];  j++)
				if (strcmp(n, digests[j]->algorithm)==0)
					break;
			if (!digests[j])
				eopt2("algorithm", "unknown algorithm", n);
			strcpy(algorithm, digests[j]->algorithm);
			digest_module = digests[j];

		//  --input-path path/to/file

		} else if (strcmp("input-path", a) == 0) {
			if (input_path)
				emany("input-path");

			if (++i == argc)
				eopt("input-path", "requires file path");
			if (!*argv[i])
				eopt("input-path", "empty file path");
			input_path = argv[i];

		//  --ouput-path pth/to/file

		} else if (strcmp("output-path", a) == 0) {
			if (output_path)
				emany("output-path");

			if (++i == argc)
				eopt("output-path", "missing <file path>");
			if (strcmp(argv[i], null_device) == 0)
				output_path = null_device;
			else
				output_path = argv[i];

		//  --service protocol:endpoint[?qargs]

		} else if (strcmp("service", a) == 0) {
			char *s, *p;
			char name[9], *endp;
			struct service *sp = 0;
			int j;

			if (service)
				emany("service");

			if (++i == argc)
				eservice("missing <name:end_point>", "");
			s = argv[i];

			//  extract the ascii service name.

			p = strchr(s, ':');
			if (!p)
				eservice("no colon in <name:end_point>", s);
			if (p - s > 8)
				eservice("name > 8 chars", s);
			if (p - s == 0)
				eservice("empty name", s);

			//  find the service driver

			memcpy(name, s, p - s);
			name[p - s] = 0;
			for (j = 0;  services[j] && sp == NULL;  j++)
				if (strcmp(services[j]->name, name) == 0)
					sp = services[j];
			if (!sp)
				eservice("unknown service", name);

			/*
			 *  End point > 0 and < 128 characters,
			 *  including query args.
			 */
			p++;
			if (strlen(p) >= 128)
				eservice("end point >= 128 characters", s);
			if (!*p)
				eservice("empty <end point>", s);
			if (*p == '?')
				eservice("empty <end point> before ? char", s);
			endp = p;

			/*
			 *  Verify that the end point are all graphic
			 *  characters.
			 */
			while (*p) {
				if (!isgraph(*p))
					eservice("non graph char", name);
				p++;
			}

			/*
			 *  Extract query arguments from service: brr, tmo
			 *
			 *	fs:/home/jmscott/opt/blobio?tmp=20&brr=fs.brr
			 *
			 *  Note:
			 *	can we write to the service string, which is
			 *	part of argv?
			 */
			char *query;
			if ((query = rindex(endp, '?'))) {
				*query++ = 0;

				char *err = BLOBIO_SERVICE_frisk_query(query);
				if (err)
					eservice("query", err);

				err = BLOBIO_SERVICE_get_tmo(query, &timeout);
				if (err)
					eservice("query: timeout", err);

				err = BLOBIO_SERVICE_get_brr_path(
					query,
					brr_path
				);
				if (err)
					eservice("query: brr path", err);
			}

			//  validate the syntax of the specific end point

			err = sp->end_point_syntax(endp);
			if (err)
				eservice(err, endp);
			service = sp;
			strcpy(service->end_point, endp);
		} else if (strcmp("trace", a) == 0) {
			if (tracing)
				emany("trace");
			tracing = 1;
			TRACE("hello, world");
		} else if (strcmp("help", a) == 0) {
			help();
		} else
			die2("unknown option", argv[i]);
	}
}

/*
 *  Cross reference command line arguments to insure no conflicts.
 *  For example, the command line arg --output-path is not needed with the
 *  verb "put".
 */
static void
xref_args()
{
	/*
	 *  verb: get/give/put/take/roll
	 *  	--service required
	 *  	--udig required
	 *
	 *  verb: roll
	 *  	no --{input,output}-path
	 *
	 *  verb: give
	 *  	no --output-path
	 *
	 *  verb: take, get
	 *  	no --input-path
	 *
	 *  verb: eat
	 *  	no --output-path
	 *	--service requires --udig and 
	 *		implies no --algorithm
	 *
	 *  verb: wrap
	 *	no --{input,output}-path, --udig
	 *	--algorithm required
	 *
	 *  Note:
	 *	Convert if/else if/ tests to switch.
	 */
	if (*verb == 'g' || *verb == 'p' || *verb == 't' || *verb == 'r') {
		if (!service)
			no_opt("service");
		if (!ascii_digest[0])
			no_opt("udig");
		if (*verb == 'r') {
			if (input_path)
				enot("input-path");
			else if (output_path)
				enot("output-path");
		} else if (verb[1] == 'i') {		// verb: "give"
			if (output_path)
				enot("output-path");
		} else if (verb[1] == 'e') {		//  verb: "get"
			if (input_path)
				enot("input-path");
		} else if (verb[1] == 'a') {		//  verb: "take"
			if (input_path)
				enot("input-path");
		} else if (*verb == 'p') {		//  verb: "put"
			if (output_path)
				enot("output-path");
		}
	} else if (*verb == 'e') {

		//  verbs: "eat" or "empty"

		if (verb[1] == 'a') {
			if (service) {
				if (input_path)
					enot("input-path");
				if (output_path)
					enot("output-path");
				if (algorithm[0]) {
					if (!ascii_digest[0])
						enot("algorithm");
				} else
					no_opt("udig");
			} else if (ascii_digest[0])
				no_opt("service");
			else if (!algorithm[0])
				no_opt("algorithm");
		} else {
			if (output_path)
				enot("output-path");
			if (input_path)
				enot("input-path");
			if (service)
				enot("service");
			if (!ascii_digest[0] && !algorithm[0])
				no_opt("{udig,algorithm}");
		}
	} else if (*verb == 'w') {
		if (!service)
			no_opt("service");
		if (input_path)
			enot("input-path");
		if (output_path)
			enot("output-path");
		if (ascii_digest[0])
			enot("udig");
		if (algorithm[0])
			enot("algorithm");
	}
}

static void
brr_write()
{
	int fd;
	struct tm *t;
	char brr[BRR_SIZE + 1];
	struct timespec	end_time;
	long int sec, nsec;
	size_t len;

	fd = uni_open_mode(
		brr_path,
		O_WRONLY|O_CREAT|O_APPEND,
		S_IRUSR|S_IWUSR|S_IRGRP
	);
	if (fd < 0)
		die2("open(brr-path) failed", strerror(errno));
	/*
	 *  Build the ascii version of the start time.
	 */
	t = gmtime(&start_time.tv_sec);
	if (!t)
		die2("gmtime() failed", strerror(errno));
	/*
	 *  Record start time.
	 */
	if (clock_gettime(CLOCK_REALTIME, &end_time) < 0)
		die2("clock_gettime(end REALTIME) failed", strerror(errno));
	/*
	 *  Calculate the elapsed time in seconds and
	 *  nano seconds.  The trick of adding 1000000000
	 *  is the same as "borrowing" a one in grade school
	 *  subtraction.
	 */
	if (end_time.tv_nsec - start_time.tv_nsec < 0) {
		sec = end_time.tv_sec - start_time.tv_sec - 1;
		nsec = 1000000000 + end_time.tv_nsec - start_time.tv_nsec;
	} else {
		sec = end_time.tv_sec - start_time.tv_sec;
		nsec = end_time.tv_nsec - start_time.tv_nsec;
	}

	/*
	 *  Verify that seconds and nanoseconds are positive values.
	 *  This test is added until I (jmscott) can determine why
	 *  I peridocally see negative times on Linux hosted by VirtualBox
	 *  under Mac OSX.
	 */
	if (sec < 0)
		sec = 0;
	if (nsec < 0)
		nsec = 0;

	/*
	 *  Format the record buffer.
	 */
	snprintf(brr, BRR_SIZE + 1, brr_format,
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		start_time.tv_nsec,
		transport,
		verb,
		algorithm[0] ? algorithm : "",
		algorithm[0] && ascii_digest[0] ? ":" : "",
		ascii_digest[0] ? ascii_digest : "",
		chat_history,
		blob_size,
		sec, nsec
	);

	len = strlen(brr);
	/*
	 *  Write the entire blob request record in a single write().
	 */
	if (uni_write(fd, brr, len) < 0)
		die2("write(brr-path) failed", strerror(errno));
	if (uni_close(fd) < 0)
		die2("close(brr-path) failed", strerror(errno));
}

int
main(int argc, char **argv)
{
	char *err;
	int exit_status = -1, ok_no;

	if (clock_gettime(CLOCK_REALTIME, &start_time) < 0)
		die2(
			"clock_gettime(start REALTIME) failed",
			strerror(errno)
		);

	parse_argv(argc, argv);

#ifndef COMPILE_TRACE
	tracing = 0;
#endif
	xref_args();

	if (tracing) {
		char buf[MAX_ATOMIC_MSG];

		snprintf(buf, sizeof buf, "timeout: %d", timeout);
		TRACE(buf);

		snprintf(buf, sizeof buf, "brr path: %s", brr_path);
		TRACE(buf);
	}

	//  the input path must always exist in the file system.

	if (input_path) {
		struct stat st;

		if (stat(input_path, &st) != 0) {
			if (errno == ENOENT)
				eopt2("input-path", "no file", input_path);
			eopt2("input-path", strerror(errno), input_path);
		}
	}

	//  the output path must never exist in the file system, unless
	//  /dev/null

	if (output_path && output_path != null_device) {
		struct stat st;

		if (stat(output_path, &st) == 0)
			eopt2("output-path", "refuse to overwrite file",
								output_path);
		if (errno != ENOENT)
			eopt2("output-path", strerror(errno), output_path);
	}

	if (service)
		service->digest = digest_module;

	//  initialize the digest module

	if (digest_module && (err = digest_module->init()))
		die(err);

	//  open the blob service
	if (service && (err = service->open())) {
		char buf[MAX_ATOMIC_MSG];

		*buf = 0;
		buf3cat(
			buf,
			sizeof buf,
			"service open(",
				service->end_point,
			") failed"
		);
		die2(buf, err);
	}

	//  open the input path in the file system

	if (input_path) {
		int fd;

		fd = uni_open(input_path, O_RDONLY);
		if (fd == -1)
			die3("open(input) failed", err, strerror(errno));
		input_fd = fd;
	}

	//  open the output path

	if (output_path) {
		if (service) {
			err = service->open_output();
			if (err)
				die3(
					"service open(output) failed",
					err,
					output_path
				);
		} else {
			int fd;
			int flag = O_WRONLY | O_CREAT;

			if (output_path != null_device)
				flag |= O_EXCL;
			fd = uni_open_mode(output_path, flag, S_IRUSR|S_IRGRP);
			if (fd == -1)
				die3(
					"open(output) failed",
					strerror(errno),
					output_path
				);
			output_fd = fd;
		}
	}


	//  invoke the service callback
	//
	//  Note: Convert if/else if/ tests to switch.

	if (*verb == 'g') {

		//  "get" or "give" a blob

		if (verb[1] == 'e') {
			if ((err = service->get(&ok_no)))
				die2("get failed", err);
			exit_status = ok_no;
		} else {
			if ((err = service->give(&ok_no)))
				die2("give failed", err);

			//  remove input path upon successful "give"

			if (ok_no == 0 && input_path) {
				int status = uni_unlink(input_path);

				if (status == -1 && errno != ENOENT)
					die2(
						"unlink(input) failed",
						strerror(errno)
					);
			}
			exit_status = ok_no;
		}
	} else if (*verb == 'e') {
		/*
		 *  eat the blob
		 */
		if (verb[1] == 'a') {
			if (service) {
				if ((err = service->eat(&ok_no)))
					die2("eat(service) failed", err);
				exit_status = ok_no;
			} else {
				char buf[128 + 1];

				if ((err = digest_module->eat_input()))
					die2("eat(input) failed", err);

				//  write the ascii digest

				buf[0] = 0;
				buf2cat(buf, sizeof buf, ascii_digest, "\n");
				if (uni_write_buf(output_fd, buf, strlen(buf)))
					die2(
						"write(ascii_digest) failed",
						strerror(errno)
					);
				exit_status = 0;
			}
		/*
		 *  is the udig the empty udig?
		 */
		} else if (ascii_digest[0]) {
			exit_status = digest_module->empty() == 1 ? 0 : 1;
		}
		/*
		 *  write the empty ascii digest.
		 */
		else {
			char *e = digest_module->empty_digest();
			write(output_fd, (unsigned char *)e, strlen(e));
			write(output_fd, "\n", 1);
			exit_status = 0;
		}
	} else if (*verb == 'p') {
		if ((err = service->put(&ok_no)))
			die2("put() failed", err);
		exit_status = ok_no;
	} else if (*verb == 't') {
		if ((err = service->take(&ok_no)))
			die2("take() failed", err);
		exit_status = ok_no;
	} else if (*verb == 'w') {
		if ((err = service->wrap(&ok_no)))
			die2("wrap() failed", err);
		exit_status = ok_no;
	} else if (*verb == 'r') {
		if ((err = service->roll(&ok_no)))
			die2("roll() failed", err);
		exit_status = ok_no;
	}

	//  close input file

	if (input_fd > 0 && uni_close(input_fd))
		die2("close(input-path) failed", strerror(errno));

	//  close output file

	if (output_fd > 1 && uni_close(output_fd))
		die2("close(output-path) failed",strerror(errno));

	if (brr_path[0])
		brr_write();

	cleanup(exit_status);

	/*NOTREACHED*/
	exit(exit_status);
}
