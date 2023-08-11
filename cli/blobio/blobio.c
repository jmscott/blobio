/*
 *  Synopsis:
 *	Client to get/put/give/take/eat/wrap/roll blobs from blobio services.
 *  Exit Status:
 *  	0	request succeed (ok).
 *  	1	request denied (no).
 *	2	i/o timeout
 *	3	unexpected error.
 *  Options:
 *	--io-timeout seconds
 *	--service name:end_point
 *	--udig algorithm:digest
 *	--algorithm name
 *	--input-path <path/to/file>
 *	--output-path <path/to/file>
 *	--help
 *  Note:
 *	Replace "-" in options to more variable friendly "_" char.
 *	So, for example, "io-timeout" becomes "io_timeout".
 *
 *	Add query arg for --io_timeout option
 *
 *		?tmo=20
 *
 *	The wrap driver must write the digest to stdout.  that is incorrect
 *	layering.  this level should write the udig, since the behavior
 *	is the same regardless of thhe digest algorithm
 *
 *	brr_write() does not verify an existing *.brr is a symbolic link.
 *
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
 *	SIGPIPE interupted the execution:  blobio does not exists 141.
 *	A SIGPIPE when reading from stdin probably out to be considered a
 *	cancel.  Currently a SIGPIPE generated an error about the input
 *	not matching the digest, which is confusing.
 *
 *  	Options desperately need to be folding into a data structure.
 *  	We refuse to use getopts().
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
#include <limits.h>
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
static int	rm_output_path_error = 1;

/*
 *  The global request.
 */
char	verb[6];
char	algorithm[9] = {0};
char	algo[9] = {0};
char	ascii_digest[129] = {0};
char	*output_path = 0;
char	*input_path = 0;

char	BR[PATH_MAX] = {'.', 0};	//  service query arg BR=/path/to/blobio
char	brr[2];				//  write a brr record: [01]

char	*null_device = "/dev/null";
char	chat_history[10] = {0};
char	transport[129] = {0};
int	io_timeout = -1;		//  read/write() i/o timeout in seconds

unsigned long long	blob_size = 0;

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

static void
ecat(char *buf, int size, char *msg)
{
	jmscott_strcat(buf, size, jmscott_progname);
	if (verb[0])
		jmscott_strcat2(buf, size, ": ", verb);
	if (service)
		jmscott_strcat2(buf, size, ": ", service->name);
	jmscott_strcat2(buf, size, ": ERROR: ", msg);
}

static void
cleanup(int status)
{
	TRACE("entered");

	if (service) {
		service->close();
		service = (struct service *)0;
	}

	if (input_fd > -1) {
		jmscott_close(input_fd);
		input_fd = -1;
	}
	if (output_fd > -1) {
		jmscott_close(output_fd);
		output_fd = -1;
	}

	//  Note: what about closing the digest?

	//  upon error, unlink() file created by --output-path,
	//  grumbling if unlink() fails.
	if (status > 1 && rm_output_path_error && 
	    output_path && output_path != null_device &&
	    jmscott_unlink(output_path))
		if (errno != ENOENT) {
			static char panic[] =
				"PANIC: unlink(output_path) failed: ";
			char buf[MAX_ATOMIC_MSG];
			char *err;

			err = strerror(errno);

			//  assemble error message about failed
			//  unlink() and burp out

			buf[0] = 0;
			ecat(buf, sizeof buf, panic);
			jmscott_strcat2(buf, sizeof buf, err, "\n");

			jmscott_write_all(2, buf, strlen(buf));
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
	--trace		deep trace to standard error\n\
	--io-timeout	read/write() timeouts.\n\
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

void
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

//  a fatal error parsing command line options.

static void
eopt(const char *option, char *why)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	jmscott_strcat2(buf, sizeof buf, "option --", option);

	if (why)
		jmscott_strcat2(buf, sizeof buf, ": ", why);
	else
		jmscott_strcat(buf, sizeof buf, "<null why message>");
	die(buf);
}

//  a fatal error parsing command line options: the two arg version.

static void
eopt2(char *option, char *why1, char *why2)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	if (why1)
		jmscott_strcat(buf, sizeof buf, why1);
	if (why2) {
		if (buf[0])
			jmscott_strcat(buf, sizeof buf, ": ");
		jmscott_strcat(buf, sizeof buf, why2);
	}
	eopt(option, buf);
}

//  a fatal error parsing the BLOBIO_SERVICE option.

static void
eservice2(char *why1, char *why2)
{
	if (why2 && why2[0])
		eopt2("service", why1, why2);
	else
		eopt("service", why1);
}

//  a fatal error parsing the blobio --service option.
static void
eservice(char *why)
{
	eopt("service", why);
}

//  a fatal error when an option is missing.

static void
no_opt(char *option)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	jmscott_strcat2(buf, sizeof buf,
		"missing required option: --",
		option
	);
	die(buf);
}

//  a fatal error when a command line option has been given more than once.

static void
emany(char *option)
{
	char buf[MAX_ATOMIC_MSG];

	buf[0] = 0;
	jmscott_strcat2(buf, sizeof buf,
		"option given more than once: --",
		option
	);
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
		jmscott_write_all(2, usage, strlen(usage));
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
	strcpy(verb, argv[1]);

	//  parse command line arguments after the request verb

	for (i = 2;  i < argc;  i++) {
		char *a = argv[i];
		TRACE2("argv", a);

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
		 *	--io-timeout
		 *	--help
		 */

		//  --udig <udig>

		if (strcmp("udig", a) == 0) {
			//  udig syntax:
			//	^[a-z][a-z0-9]{0,7}:[[:isgraph:]]{32,128}$

			char *ud;
			struct digest *d;

			//  option --udig given more than once.

			if (ascii_digest[0])
				emany("udig");

			// both --udig and --algorithm can't be defined at once
			//
			//  Note: move to xref_argv()?

			if (algorithm[0])
				eopt("udig", "option --algorithm conflicts");
			if (++i == argc)
				eopt("udig", "missing <algorithm:digest>");
			ud = argv[i];

			err = jmscott_frisk_udig(ud);
			if (err)
				eopt2("udig", err, ud);

			//  find the digest module for algorithm.
			int colon = index(ud, ':') - ud;
			algorithm[colon] = 0;
			memmove(algorithm, ud, colon);

			d = find_digest(algorithm);
			if (!d)
				eopt2("udig", "unknown algorithm", algorithm);

			//  verify the digest is syntactically well formed

			ascii_digest[0] = 0;
			jmscott_strcat(
				ascii_digest,
				sizeof ascii_digest,
				&ud[colon + 1]
			);
			if (d->syntax() == 0)
				eopt2(
					"udig",
					"bad syntax for digest",
					ascii_digest
				);
			digest_module = d;

		//  --algorithm [a-z][a-z0-9]{0,7}

		} else if (strcmp("algorithm", a) == 0) {
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
			struct digest *d = find_digest(n);
			if (!d)
				eopt2("algorithm", "unknown algorithm", n);
			strcpy(algorithm, d->algorithm);
			digest_module = d;

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
				eservice("missing <name:end_point>");
			s = argv[i];

			//  extract the ascii service name.

			p = strchr(s, ':');
			if (!p)
				eservice2("no colon in <name:end_point>", s);
			if (p - s > 8)
				eservice2("name > 8 chars", s);
			if (p - s == 0)
				eservice2("empty name", s);

			//  find the service driver

			memcpy(name, s, p - s);
			name[p - s] = 0;
			for (j = 0;  services[j] && sp == NULL;  j++)
				if (strcmp(services[j]->name, name) == 0)
					sp = services[j];
			if (!sp)
				eservice2("unknown service", name);

			/*
			 *  End point > 0 and < 128 characters,
			 *  including query args.
			 */
			p++;
			if (strlen(p) >= 128)
				eservice2("end point >= 128 characters", s);
			if (!*p)
				eservice2("empty <end point>", s);
			if (*p == '?')
				eservice2("empty <end point> before ? char", s);
			endp = p;

			/*
			 *  Verify that the end point are all graphic
			 *  characters.
			 */
			char *query = (char *)0, c;
			while ((c = *p++)) {
				if (!isgraph(c))
					eservice("non graph char in query arg");
				if (c == '?') {
					if (query)
						eservice(
							"multiple \"?\" "
							"are forbidden"
						);
					else
						query = p - 1;
				} else if (c == '&' && !query)
					eservice("missing \"?\" before \"&\"");
			}

			//  check for common error where '&' instead of '?'
			//  is ued to introduce the query string.

			/*
			 *  Extract query arguments from service:
			 *
			 *	BR	path/to/blobio/root
			 *	brr	write a brr record [01]
			 *	algo	algorithm for wrap
			 *	tmo	per i/o timeout
			 */
			if ((query = rindex(endp, '?'))) {
				*query++ = 0;
				sp->query[0] = 0;
				jmscott_strcat(sp->query, sizeof sp->query,
						query
				);

				char *err = BLOBIO_SERVICE_frisk_qargs(query);
				if (err)
					eservice2("query arg: frisk", err);

				BLOBIO_SERVICE_get_algo(query, algo);
				BLOBIO_SERVICE_get_brr(query, brr);
				BLOBIO_SERVICE_get_BR(query, BR);
			}

			//  validate the syntax of the specific end point

			err = sp->end_point_syntax(endp);
			if (err)
				eservice2(err, endp);
			service = sp;
			strcpy(service->end_point, endp);
		} else if (strcmp("trace", a) == 0) {
			if (tracing)
				emany("trace");
			tracing = 1;
		} else if (strcmp("io-timeout", a) == 0) {
			if (io_timeout >= 0)
				emany("io-timeout");
			if (++i == argc)
				emany("io-timeout");
			unsigned long long ull;
			if ((err = jmscott_a2ui63(argv[i], &ull)))
				eopt2(a, "can not parse seconds", err);
			if (ull > 255)
				eopt(a, "seconds > 255");
			io_timeout = ull;
			TRACE2("timeout seconds", argv[i])
		} else if (strcmp("help", a) == 0) {
			help();
		} else
			die2("unknown option", argv[i]);
	}
	if (io_timeout == -1)
		io_timeout = 0;
}

/*
 *  Cross reference command line arguments to insure no conflicts.
 *  For example, the command line arg --output-path is not needed with the
 *  verb "put".
 */
static void
xref_argv()
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
	xref_argv();

	TRACE2("query arg: BR", BR);
	TRACE2("query arg: brr", brr);
	TRACE2("query arg: algo", algo);

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

		if (stat(output_path, &st) == 0) {
			rm_output_path_error = 0;
			eopt2("output-path", "refuse to overwrite file",
								output_path);
		}
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
		TRACE2("service error", err);
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

		fd = jmscott_open(input_path, O_RDONLY, 0);
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
			fd = jmscott_open(output_path, flag, S_IRUSR|S_IRGRP);
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
				int status = jmscott_unlink(input_path);

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

				if ((err = digest_module->eat_input(input_fd)))
					die2("eat(input) failed", err);

				//  write the ascii digest

				buf[0] = 0;
				buf2cat(buf, sizeof buf, ascii_digest, "\n");
				if (
				   jmscott_write_all(output_fd,buf,strlen(buf))
				)
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

	//  we had a successful transactions
	if (brr[0] == '1')
		brr_write(service->name);
	cleanup(exit_status);

	/*NOTREACHED*/
	exit(exit_status);
}
