/*
 *  Synopsis:
 *	Client to get/put/give/take/eat/wrap/roll blobs via blobio protcol.
 *  Exit Status:
 *	get:
 *		0	blob read back successfully: ok
 *		1	blob not read back: no
 *	put:
 *		0	blob accepted by remote: ok
 *		1	blob rejected by remote: no
 *	give:
 *		0	blob given to remote: ok,ok
 *		1	remote rejected give: no
 *	take:
 *		0	blob taken by remote: ok,ok,ok
 *		1	blob rejected by remote: no
 *	eat:
 *		0	blob verified on remote: ok
 *		1	blob not verified: no
 *	wrap:
 *		0	wrap set generated and blob udig written back: ok
 *		1	request rejected: no
 *	roll:
 *		0	wrap set rolled/forgotton: ok
 *		1	roll rejected (usually no wrap set for given blob): no
 *	empty:
 *		0	udig describes the well known empty blob for udig
 *		1	udig is syntactically correct but not empty
 *	all else
 *		2	missing or invalid command line argument
 *		64	unexpected error
 *
 *	Under OSX 10.9, an exit status 141 in various shells can indicate a
 *	SIGPIPE interupted the execution.  blobio does not exit 141.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 *  Note:
 *	Add the exit status codes to the usage output (dork).
 *
 *	Also, the take&give exit statuses ought to reflect the various ok/no
 *	chat histories or perhaps the exit status ought to also store the
 *	verb.  See exit status for child process requests in the blobio
 *	server.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "blobio.h"

/*
 *  Why isn't PATH_MAX always define in <unistd.h>?
 *  Uggh.  It never ends.
 */
#ifndef PATH_MAX
#define PATH_MAX	255
#endif

#define REMOTE_TIMEOUT	60		/* seconds */

int			tracing = 0;
int			trace_fd = 2;

extern int		h_errno;
extern int		errno;
extern struct module	*modules[];

static int		timeout = 60;
static char		*prog = "blobio";
static char		usage[] =
   "usage: blobio [--help] [get|put|give|take|eat|wrap|roll|empty] [options]\n";
static char		output_path[PATH_MAX] = {0};

static void		error2(char *, char *);
static char		*verb = 0;

static void
leave(int status)
{
	/*
	 *  Zap any created output file on error.
	 */
	if (output_path[0] && status && unlink(output_path)) {
		int e;
		char buf[BUFSIZ];

		e = errno;
		snprintf(buf, sizeof buf, "unlink(%s) failed", output_path);

		error2(buf, strerror(e));
		status = 64;
	}
	exit(status);
}

static void
help()
{
	fputs(usage, stdout);
	fputs("\n\
        where options are\n\
\n\
	--service       ip4 host:port [required]\n\
	--input-path    put/give/eat source file [default stdin]\n\
	--output-path   get/take target file [default stdout]\n\
	--udig          algorithm:digest for get/put/give/take/eat/empty/roll\n\
	--algorithm     algorithm name for local digest\n\
\n\
	and exit status are:\n\
\n\
	get:\n\
		0	blob read back successfully: ok\n\
		1	blob not read back: no\n\
	put:\n\
		0	blob accepted by remote: ok\n\
		1	blob rejected by remote: no\n\
	give:\n\
		0	blob given to remote: ok,ok\n\
		1	remote rejected give: no\n\
	take:\n\
		0	blob taken by remote: ok,ok,ok\n\
		1	blob rejected by remote: no\n\
	eat:\n\
		0	blob verified on remote: ok\n\
		1	blob not verified: no\n\
	wrap:\n\
		0	wrap set generated and blob udig written back: ok\n\
		1	request rejected: no\n\
	roll:\n\
		0	wrap set rolled/forgotton: ok\n\
		1	roll rejected (usually no wrap set for given blob): no\n\
	empty:\n\
		0	udig describes the well known empty blob for udig\n\
		1	udig is syntactically correct but not empty\n\
	all else\n\
		2	missing or invalid command line argument\n\
		3	unexpected system error\n\
",	stdout);
	leave(0);
}

static void
error(char *msg)
{
	char *m = "null";

	if (msg)
		m = msg;

	fputs(prog, stderr);
	fputs(": ERROR: ", stderr);
	fputs(m, stderr);
	fputc('\n', stderr);
	fputs(usage, stderr);
}

static void
error2(char *msg1, char *msg2)
{
	char *m1, *m2;
	char buf[BUFSIZ];

	m1 = m2 = "null";
	if (msg1)
		m1 = msg1;
	if (msg2)
		m2 = msg2;
	snprintf(buf, sizeof buf, "%s: %s", m1, m2);
	error(buf);
}

static void
die(char *msg)
{
	error(msg);
	leave(3);
}

static void
die2(char *msg1, char *msg2)
{
	char *m1, *m2;
	char buf[BUFSIZ];

	m1 = m2 = "null";
	if (msg1)
		m1 = msg1;
	if (msg2)
		m2 = msg2;
	snprintf(buf, sizeof buf, "%s: %s", m1, m2);
	die(buf);
}

static void
die3(char *msg1, char *msg2, char *msg3)
{
	char *m1, *m2, *m3;
	char buf[BUFSIZ];

	m1 = m2 = m3 = "null";
	if (msg1)
		m1 = msg1;
	if (msg2)
		m2 = msg2;
	if (msg3)
		m3 = msg3;
	snprintf(buf, sizeof buf, "%s: %s: %s", m1, m2, m3);
	die(buf);
}

static void
die4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	char *m1, *m2, *m3, *m4;
	char buf[BUFSIZ];

	m1 = m2 = m3 = m4 = "null";

	if (msg1)
		m1 = msg1;
	if (msg2)
		m2 = msg2;
	if (msg3)
		m3 = msg3;
	if (msg4)
		m4 = msg4;
	snprintf(buf, sizeof buf, "%s: %s: %s: %s", m1, m2, m3, m4);
	die(buf);
}

void
trace(char *msg)
{
	write(trace_fd, "TRACE: ", 7);
	write(trace_fd, msg, strlen(msg));
	write(trace_fd, "\n", 1);
}

void
trace2(char *msg1, char *msg2)
{
	char buf[2048];

	if (msg1)
		if (msg2) {
			snprintf(buf, sizeof buf, "%s: %s", msg1, msg2);
			trace(buf);
		} else
			trace(msg1);
	else if (msg2)
		trace(msg2);
}

/*
 *  Synopsis:
 *	Parse out the algorithm and digest from a text uniform digest (udig).
 *  Args:
 *	udig:		null terminated udig string to parse
 *	algorithm:	at least 9 bytes for extracted algorithm. 
 *	digest:		at least 129 bytes for extracted digest
 *  Returns:
 *	0	upon success
 *	EINVAL	upon failure.
 */
static int
parse_udig(char *udig, char *algorithm, char *digest)
{
	char *u, *a, *d;
	int in_algorithm = 1;

	if (!udig)
		return EINVAL;
	a = algorithm;
	d = digest;

	for (u = udig;  *u;  u++) {
		char c = *u;

		if (!isascii(c) || !isgraph(c))
			return EINVAL;
		/*
		 *  Scanning algorithm.
		 */
		if (in_algorithm) {
			if (c == ':') {
				if (a == algorithm)
					return EINVAL;
				*a = 0;
				in_algorithm = 0;
			}
			else {
				int alen = a - algorithm;

				if (alen >= 8)
					return EINVAL;
				if (alen == 0) {
					if (!isalpha(c) || !islower(c))
						return EINVAL;
				} else if (!isalnum(c) || !islower(c))
					return EINVAL;
				*a++ = c;
			}
			continue;
		}
		/*
		 *  Scanning digest.
		 */
		if (d - digest >= 128)
			return EINVAL;
		*d++ = c;
	}
	*d = 0;
	if (d - digest < 32)
		return EINVAL;
	return 0;
}

/*
 *  Synopsis:
 *	Open a client socket connection to a blobio server.
 *  Returns:
 *	0	success and populates *p_server_fd;
 *	errno	if an error occured
 *
 *	Map the gethostbyname() error thusly:
 *
 *		TRY_AGAIN	->	EAGAIN, after 5 trys
 *
 *		HOST_NOT_FOUND
 *		NO_DATA		->	ENOENT
 *		NO_ADDRESS
 *
 *		NO_RECOVERY	->	EREMOTEIO
 */
static int
_connect(char *host, int port, int *p_server_fd)
{
	struct hostent *h;
	struct sockaddr_in s;
	int trys;
	int fd;

	if (tracing)
		trace2("_connect: remote host", host);
	/*
	 *  Look up the host name, trying up to 5 times.
	 */
	trys = 0;
again1:
	h = gethostbyname(host);
	trys++;
	if (!h) {
		if (tracing)
			trace2("_connect: gethostbyname() failed", host);
		/*
		 *  Map the h_errno value set by gethostbyname() onto
		 *  something reasonable.
		 */
		switch (h_errno) {
		case TRY_AGAIN:
			if (trys < 5)
				goto again1;
			return EAGAIN;
		case HOST_NOT_FOUND:
		case NO_DATA:
#ifdef NO_ADDRESS
#if NO_ADDRESS != NO_DATA
		case NO_ADDRESS:
#endif
#endif
			/*
			 *  Uggh.  Map an unknown host onto ENOENT.
			 *
			 *  I (jmscott) simply do not want to create an entire
			 *  error code set for a few single cases.
			 */
			return ENOENT;
		case NO_RECOVERY:
			return EIO;
		}
		return EINVAL;			/* wtf?? */
	}

	if (tracing)
		trace("_connect: connecting to remote ...");
	/*
	 *  Open and connect to the remote blobio server.
	 */
again2:
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		int e = errno;

		if (e == EAGAIN || e == EINTR)
			goto again2;
		if (tracing)
			trace2("_connect: socket() failed", strerror(e));
		return e;
	}
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	memmove((void *)&s.sin_addr.s_addr, (void *)h->h_addr, h->h_length);
	s.sin_port = htons((short)port);

	/*
	 *  Need to timeout connect() !!
	 */
again3:
	if (connect(fd, (const struct sockaddr *)&s, sizeof s) < 0) {
		int e = errno;

		if (e == EINTR || e == EAGAIN)
			goto again3;

		if (tracing)
			trace2("_connect() failed", strerror(e));

		close(fd);
		return e;
	}
	*p_server_fd = fd;
	if (tracing) {
		char tbuf[1024];

		snprintf(tbuf, sizeof tbuf, 
			"_connect: ... ok, connected to remote, file #%d", fd);
		trace(tbuf);
	}
	return 0;
}

void
_close_socket(int fd)
{
	if (fd > -1)
		close(fd);
}

void
_disconnect(struct request *rp)
{
	if (tracing)
		trace("disconnecting from remote server ...");
	if (!rp)
		return;
	_close_socket(rp->server_fd);
	rp->server_fd = -1;

	/*
	 *  Shutdown input/output.
	 */
	if (rp->in_fd != 0)
		close(rp->in_fd);
	if (rp->out_fd != 0)
		close(rp->out_fd);

	free(rp);
	if (tracing)
		trace("... ok, disconnected cleanly");
}

static void
catch_write_ALRM(int sig)
{
	char buf[BUFSIZ];

	UNUSED_ARG(sig);

	snprintf(buf, sizeof buf, "remote write timed out after %d seconds",
								timeout);
	die(buf);
}

/*
 *  Synopsis:
 *	Do a full write() of all bytes, restarting when interrupted.
 *  Returns:
 *	0	success
 *	errno	on error
 */
static int
_write(int fd, unsigned char *buf, int buf_size)
{
	unsigned char *p, *p_end;

	if (tracing) {
		char tbuf[1024];

		snprintf(tbuf, sizeof tbuf, 
			"request to write() %d bytes to file #%d",
								buf_size, fd);
		trace(tbuf);
	}
	p = buf;
	p_end = buf + buf_size;
again:
	while (p < p_end) {
		int status;
		int e;

		if (signal(SIGALRM, catch_write_ALRM) == SIG_ERR)
			die2("_write: signal(ALRM) failed", strerror(errno));

		alarm(timeout);
		status = write(fd, p, p_end - p);
		e = errno;
		alarm(0);

		if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
			die2("_write: signal(IGN) failed", strerror(errno));

		if (tracing) {
			char tbuf[1024];

			snprintf(tbuf, sizeof tbuf, "write() returned %d",
								status);
			trace(tbuf);
			if (status > 0)
				dump(p, status, '>');
		}

		if (status == p_end - p)
			return 0;

		if (status > 0)
			p += status;
		else if (status < 0) {
			if (e == EAGAIN || e == EINTR)
				goto again;
			return e;
		} else
			die("PANIC: _write: write() returned 0 bytes");
	}
	return 0;
}

static void
catch_read_ALRM(int sig)
{
	char buf[BUFSIZ];
	
	UNUSED_ARG(sig);

	snprintf(buf, sizeof buf, "remote read timed out after %d seconds",
							timeout);
	die(buf);
}

/*
 *  Synopsis:
 *	Do a partial read() of all bytes, restarting when interrupted.
 *  Returns:
 *	>0	number of bytes read
 *	-errno	on error
 *  Note:
 *	We assume errno is always greater than 0.
 *	NEED TO ADD TIMEOUT!!!!!
 */
static int
_read(int fd, unsigned char *buf, int buf_size)
{
	int status;

	if (tracing) {
		char tbuf[1024];

		snprintf(tbuf, sizeof tbuf,
			"request to read() %d bytes from file #%d",
								buf_size, fd);
		trace(tbuf);
	}
again:
	if (signal(SIGALRM, catch_read_ALRM) == SIG_ERR)
		die2("_read: signal(ALRM) failed", strerror(errno));

	alarm(timeout);
	status = read(fd, buf, buf_size);
	alarm(0);

	if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
		die2("_read: signal(IGN) failed", strerror(errno));

	if (status > 0) {
		if (tracing) {
			char tbuf[1024];

			snprintf(tbuf, sizeof tbuf, "read %d bytes", status);
			trace(tbuf);
			dump(buf, status, '<');
		}
		return status;
	}
	if (status < 0) {
		int e = errno;

		if (e == EAGAIN || e == EINTR)
			goto again;
		if (e < 0) {
			char pbuf[1024];

			snprintf(pbuf, sizeof pbuf, "_read: errno < 0: %d", e);
			die(pbuf);
		}
		if (tracing) {
			char tbuf[1024];

			snprintf(tbuf, sizeof tbuf, "errno=%d: %s", e,
								strerror(e));
			trace(tbuf);
		}
		return -e;
	}
	if (tracing)
		trace("read() returned 0 bytes");
	return 0;
}

/*
 *  Read a response from a server.  Return 0 if 'ok' returned,
 *  return 1 if 'no', or  die() upon error.
 */
static int
read_response(struct request *rp)
{
	char buf[3];
	int nread = 0;

	/*
	 *  Read a 3 character response from the server.
	 */
	while (nread < 3) {
		int status = _read(rp->server_fd, (unsigned char *)buf,
					3 - nread);
		if (status < 0)
			die4(rp->verb, rp->algorithm, "_read(remote) failed",
							strerror(-status));
		if (status == 0)
			die3(rp->verb, rp->algorithm,
				"read_response: _read(remote) premature eof");
		nread += status;
	}

	/*
	 *  Look for "ok\n" or "no\n" from the server.
	 */
	if (buf[2] == '\n') {
		if (buf[0] == 'o' && buf[1] == 'k')
			return 0;
		if (buf[0] == 'n' && buf[1] == 'o')
			return 1;
	}
	die3(rp->verb, rp->algorithm, "unexpected response from server");
	/*NOTREACHED*/
	return -1;
}

/*
 *  Assemble the command sent to the server.
 *
 *	get/put/give/take/eat/wrap/roll algorithm:digest
 *	wrap algorithm
 */
static int
copy_command(char *command, char *verb, char *algorithm, char *digest)
{
	char *c, *p;

	c = command;
	*c++ = verb[0];
	*c++ = verb[1];
	*c++ = verb[2];
	if (verb[3])
		*c++ = verb[3];
	*c++ = ' ';

	p = algorithm;
	while (*p)
		*c++ = *p++;
	if (digest[0]) {
		*c++ = ':';

		p = digest;
		while (*p)
			*c++ = *p++;
	}
	*c++ = '\n';
	*c = 0;
	return c - command;
}

/*
 *  Send a get/put/give/take/wrap/roll command to the remote server and
 *  read the reply.  Return 0 if 'ok' is reply, 1 if 'no' is reply,
 *  -1 otherwise.
 */
static int
send_request(struct request *rp, int expect_reply)
{
	int len, status;
	char command[4 + 8 + 1 + 256 + 1 + 1];

	/*
	 *  Write the request to the server as
	 *
	 *	wrap algorithm
	 *	or
	 *	verb algorithm:digest\n
	 */
	len = copy_command(command, rp->verb, rp->algorithm, rp->digest);

	if ((status = _write(rp->server_fd, (unsigned char *)command, len)))
		die4(rp->verb, rp->algorithm, "write(request) failed",
							strerror(status));
	if (!expect_reply)
		return 0;
	/*
	 *  Read response from server.  We expect either.
	 *
	 *	ok\n
	 *	no\n
	 */
	return read_response(rp);
}

/*
 *  Get a blob from the server.
 */
static int
get(struct request *rp)
{
	int status;
	unsigned char buf[64 * 1024];
	struct module *m = (struct module *)rp->module;
	int more = 1;

	if (send_request(rp, 1) == 1)
		return 1;			/* blob not found */

	/*
	 *  Read the blob back from the server, incrementally
	 *  verifying the signature.
	 */
	while (more && (status = _read(rp->server_fd, buf, sizeof buf)) > 0) {
		int nread = status;

		more = m->get_update(rp, buf, nread);

		if (more < 0)
			die3(rp->verb, rp->algorithm, "bad local signature");

		/*
		 *  Partial blob verified, so write() to output.
		 */
		if ((status = _write(rp->out_fd, buf, nread)) < 0)
			die4(rp->verb, rp->algorithm, "write(out) failed",
							strerror(status));
		if (tracing) {
			if (more)
				trace2(rp->verb,
					"partial blob seen, reading more");
			else
				trace2(rp->verb, "blob digested");
		}
	}
	/*
	 *  Not handling empty blob correctly.
	 */
	if (more && m->is_digest(rp->digest) != 2)
		die2(rp->verb, "remote closed, complete blob not read");
	if (status < 0)
		die3(rp->verb, rp->algorithm, "read(remote) failed");
	return 0;
}

/*
 *  Get a wrapped set of udigs from the remote.
 */
static int
wrap(struct request *rp)
{
	int status;
	unsigned char buf[64 * 1024];

	if (send_request(rp, 1) == 1)
		return 1;			/* blob not found */

	/*
	 *  Read the udig back from the server and write to output.
	 *
	 *  Note: Need to verify the udig of returned server matches the udig
	 *        of the algorithm.
	 */
	while ((status = _read(rp->server_fd, buf, sizeof buf)) > 0)
		if (_write(rp->out_fd, buf, status))
			die("wrap: write(1) failed");
	if (status < 0)
		die3(rp->verb, rp->algorithm, "read(remote) failed");
	return 0;
}

/*
 *  Roll/forget a wrap set.
 */
static int
roll(struct request *rp)
{
	int status;
	unsigned char buf[64 * 1024];

	if (send_request(rp, 1) == 1)
		return 1;				/* blob not found */

	/*
	 *  Read the udig back from the server and write to output.
	 *
	 *  Note: Need to verify the udig of returned server matches the udig
	 *        of the algorithm.
	 */
	while ((status = _read(rp->server_fd, buf, sizeof buf)) > 0)
		if (_write(rp->out_fd, buf, status))
			die("wrap: write(1) failed");
	if (status < 0)
		die3(rp->verb, rp->algorithm, "read(remote) failed");
	return 0;
}

/*
 *  Put a blob to the server.
 */
static int
put(struct request *rp)
{
	int status;
	unsigned char buf[64 * 1024];
	struct module *m = (struct module *)rp->module;
	int more = 1;

	/*
	 *  Send request to put the blob to the server.
	 *  Return of 1 means remote server replied with 'no'.
	 */
	if (send_request(rp, 0) == 1)
		return 1;

	/*
	 *  Read the blob from standard in and write incrementally to the
	 *  blob server, verifying the signature.
	 */
	while (more && (status = _read(rp->in_fd, buf, sizeof buf)) > 0) {
		int nread = status;

		more = m->put_update(rp, buf, nread);

		if (more < 0)
			die3(rp->verb, rp->algorithm, "bad local signature");
		/*
		 *  Partial blob verified, so write() to output.
		 */
		status = _write(rp->server_fd, buf, nread);
		if (status)
			die4(rp->verb, rp->algorithm, "write(remote) failed",
							strerror(status));
		if (tracing) {
			if (more)
				trace("put: partial blob seen, reading more");
			else
				trace("put: entire blob digested");
		}
	}
	if (status < 0)
		die3(rp->verb, rp->algorithm, "read(in) failed");
	if (more && m->is_digest(rp->digest) != 2)
		die2(rp->verb, "unexpected eof: incorrect digest, perhaps");
	/*
	 *  Read response from server.  We expect either.
	 *
	 *	ok\n
	 *	no\n
	 */
	return read_response(rp);
}

/*
 *  Give a blob to a server.
 *  Note: the local file is NOT removed.
 */
static int
give(struct request *rp)
{
	if (put(rp) == 1)
		return 1;
	if (_write(rp->server_fd, (unsigned char *)"ok\n", 3)) {
		die3(rp->verb, rp->algorithm, "_write(remote reply) failed");
		return 1;
	}
	return 0;
}

/*
 *  Take a blob from a server.
 */
static int
take(struct request *rp)
{
	int status;
	unsigned char buf[64 * 1024];
	struct module *m = (struct module *)rp->module;
	int more, is_empty;

	if (send_request(rp, 1) == 1) {
		if (tracing)
			trace2(rp->verb, "blob not found");
		return 1;
	}
	is_empty = m->is_digest(rp->digest) == 2;
	more = is_empty ? 0 : 1;

	/*
	 *  Read the blob back from the server, incrementally
	 *  verifying the signature.
	 */
	while (more && (status = _read(rp->server_fd, buf, sizeof buf)) > 0) {
		int nread = status;

		more = m->take_update(rp, buf, nread);

		if (more < 0)
			die3(rp->verb, rp->algorithm, "bad signature");

		/*
		 *  Partial blob verified, so write() to output.
		 */
		if ((status = _write(rp->out_fd, buf, nread)) < 0)
			die4(rp->verb, rp->algorithm, "write(out) failed",
							strerror(status));
		if (tracing) {
			if (more)
				trace("take: partial blob seen, reading more");
			else
				trace("take: entire blob seen, done");
		}
	}
	/*
	 *  An i/o or unexpected input on take.
	 */
	if (status || (more && !is_empty)) {
		_write(rp->server_fd, (unsigned char *)"no\n", 3);
		if (tracing) {
			if (status)
				error("take: io error to remote service");
			if (more)
				error("take: premature end of blob");
				error(
			"take: unexpected eof: incorrect digest, perhaps");
		}
		return -1;
	}
	/*
	 *  We successfully read the blob, so tell the remote
	 *  that we have the server and read the reply from the remote.
	 */
	if (tracing)
		trace("take: successfully read blob");
	if (_write(rp->server_fd, (unsigned char *)"ok\n", 3))
		die2(rp->verb, "_write(reply:ok) failed");
	status = read_response(rp);
	if (status == -1)
		die2(rp->verb, "read_response() failed");
	return m->took(rp, status ? "ok" : "no");
}

static int
eat_service(struct request *rp)
{
	if (tracing)
		trace("eat_service: entered");
	return send_request(rp, 1);
}

/*
 *  Eat/Digest a blob on the local, client file system.
 *
 *  Note:
 *	Needs to be in separate program, not blobio.
 */
static int
eat_client(struct request *rp)
{
	struct module *m = (struct module *)rp->module;
	char digest[256];
	int status;

	if (tracing)
		trace("eat_client: entered");

	strcpy(digest, rp->digest);
	digest[sizeof digest - 1] = 0;
	if (m->eat(rp))
		die3("eat", m->algorithm, "failed");
	/*
	 *  If user passed --udig, then verify the eaten digest with
	 *  what the user thinks the digest should be.
	 */
	if (digest[0])
		return strcmp(digest, rp->digest) == 0 ? 0 : 1;
	if ((status = _write(rp->out_fd, (unsigned char *)rp->digest,
				strlen(rp->digest))) ||
	    (status = _write(rp->out_fd, (unsigned char *)"\n", 1))) {
		error2("eat: write() failed", strerror(status));
			return -1;
	}
	return 0;
}

/*
 *  Generate or validate a digest for either a local file/standard input
 *  or send an eat request to a blobio server.
 */
static int
eat(struct request *rp)
{
	return rp->host[0] ?  eat_service(rp) : eat_client(rp);
}

/*
 *  Generate or validate a digest for either a local file/standard input
 *  or send an eat request to a blobio server.
 */
static int
empty(struct request *rp)
{
	struct module *m = (struct module *)rp->module;

	return m->is_empty(rp->digest) ? 0 : 1;
}

static void
open_service(struct request *rp, char *verb, char *host, int port)
{
	int status;
	int server_fd;

	/*
	 *  Connect to service.
	 */
	switch (status = _connect(host, port, &server_fd)) {
	case 0:
		break;
	case ENOENT:
		die3(verb, "--service: can't resolve host", host);
	default: {
		char msg[BUFSIZ];

		snprintf(msg, sizeof msg, "_connect(%s:%d) failed: %s",
					host, port, strerror(status));
		die2(verb, msg);
	}}
	rp->server_fd = server_fd;
	strcpy(rp->host, host);
	rp->port = port;
}

static void
earg(const char *option, char *why)
{
	char ebuf[1024];

	snprintf(ebuf, sizeof ebuf, "%s: option --%s", verb, option);
	error2(ebuf, why);
	leave(2);
}

static void
earg2(char *option, char *why1, char *why2)
{
	char why[1024];

	snprintf(why, sizeof why, "%s: %s", why1, why2);
	earg(option, why);
}

static void
earg3(char *option, char *why1, char *why2, char *why3)
{
	char why[1024];

	snprintf(why, sizeof why, "%s: %s: %s", why1, why2, why3);
	earg(option, why);
}

static void
no_arg1(char *option)
{
	char ebuf[1024];

	snprintf(ebuf, sizeof ebuf, "%s: missing required option: --%s", verb,
								option);
	error(ebuf);
	leave(126);
}

static void
no_arg2(char *option1, char *option2)
{
	char ebuf[1024];

	snprintf(ebuf, sizeof ebuf, "%s: missing required option: --%s or --%s",
						verb, option1, option2);
	error(ebuf);
	leave(126);
}

static void
emany(char *opt)
{
	char ebuf[1024];

	snprintf(ebuf, sizeof ebuf, "option given more than once: --%s", opt);
	error2(verb, ebuf);
	leave(126);
}

int
main(int argc, char **argv)
{
	int status;
	int (*verb_cb)(struct request *);
	char **ap;
	int i;
	struct request *rp;
	char host[NI_MAXHOST + 1];
	unsigned int port;
	char algorithm[16];
	char digest[256];
	int server_fd = -1;
	int in_fd = 0;
	int out_fd = 1;
	struct module *m = 0;

	/*
	 *  Copy argv[].
	 *
	 *  Global options like --service and --input-path, --output-path
	 *  will be be processed before calling get/put/give/take()
	 *  and removed from argv[].
	 */
	if (argc <= 1) {
		error("no command line arguments");
		leave(126);
	}
	ap = argv + 1;

	/*
	 *  First argument is always the request.
	 */
	verb = strdup(*ap++);
	if (strcmp(verb, "get") == 0)
		verb_cb = get;
	else if (strcmp(verb, "put") == 0)
		verb_cb = put;
	else if (strcmp(verb, "give") == 0)
		verb_cb = give;
	else if (strcmp(verb, "take") == 0)
		verb_cb = take;
	else if (strcmp(verb, "eat") == 0)
		verb_cb = eat;
	else if (strcmp(verb, "wrap") == 0)
		verb_cb = wrap;
	else if (strcmp(verb, "roll") == 0)
		verb_cb = roll;
	else if (strcmp(verb, "empty") == 0)
		verb_cb = empty;
	else if (strcmp(verb, "--help") == 0)
		help();
	else {
		error2("unknown verb", verb);
		leave(126);
	}

	algorithm[0] = digest[0] = host[0] = 0;

	/*
	 *  Parse command line arguments, stripping out global arguments
	 *  and rebuilding argv[] with only args specific to 
	 *  get/put/give/take/eat/wrap/roll.
	 */
	for (i = 2;  i < argc;  i++) {
		char *a = *ap++;

		/*
		 *  Global Options:
		 *
		 *	--service host:port
		 *	--udig algorithm:digest
		 *	--algorithm name
		 *	--input-path file-path
		 *	--output-path file-path
		 */
		if (strcmp("--udig", a) == 0) {
			char *ud;
			int j;

			if (algorithm[0]) {
				if (digest[0])
					emany("udig");
				earg("udig", "option --algorithm conflicts");
			}
			if (++i == argc)
				earg("udig", "missing algorithm:digest");
			/*
			 *  Bust the udig into algorithm and digest.
			 */
			ud = *ap++;
			switch (parse_udig(ud, algorithm, digest)) {
			case 0:
				break;
			case EINVAL:
				earg2("udig", "can't parse udig", ud);
			default:
				die("PANIC: impossible parse_udig() failure");
			}
			/*
			 *  Locate the module for algorithm.
			 */
			m = 0;
			for (j = 0;  (m = modules[j]);  j++)
				if (strcmp(algorithm, m->algorithm) == 0)
					break;
			if (!m) {
				earg2("udig", "unknown algorithm", algorithm);
			}
			/*
			 *  Verify the digest is syntacally correct 
			 *  for this particular algorithm.
			 */
			if (strcmp("wrap", verb) && m->is_digest(digest) == 0) {
				char ebuf[1024];

				snprintf(ebuf, sizeof ebuf,
						"incorrect digest %s: %s",
						m->algorithm,
						digest
				);
				earg("udig", ebuf);
			}
		} else if (strcmp("--algorithm", a) == 0) {
			int j;
			char *p;

			if (digest[0])
				earg("algorithm", "option --udig conflicts");
			if (algorithm[0])
				emany("algorithm");
			if (++i == argc)
				earg("algorithm", "missing name");

			p = *ap++;
			if (!*p)
				earg("algorithm", "empty name");
			for (j = 0;  (m = modules[j]);  j++)
				if (strcmp(p, m->algorithm) == 0) {
					strcpy(algorithm, m->algorithm);
					break;
				}
			if (!algorithm[0])
				earg2("algorithm", "unknown algorithm", p);
		} else if (strcmp("--input-path", a) == 0) {
			char *p;

			if (in_fd != 0)
				earg("input-path", "given twice");
			if (++i == argc)
				earg("input-path", "requires file path");
			p = *ap++;
			if (!*p)
				earg("input-path", "empty file path");
			in_fd = open(p, O_RDONLY);
			if (in_fd < 0)
				earg3("input-path", "can't open for reading", p,
							strerror(errno));
		} else if (strcmp("--output-path", a) == 0) {
			char *p;

			if (out_fd != 1)
				earg("output-path", "given twice");
			if (++i == argc)
				earg("output-path", "missing file path");
			p = *ap++;
			if (strlen(p) > sizeof output_path - 1)
				earg2("output-path", "file path too long", p);
			/*
			 *  Open output file for writing with read only
			 *  permissions.
			 */
			out_fd = open(p, O_WRONLY | O_EXCL | O_CREAT, S_IRUSR);
			if (out_fd < 0)
				earg3("output-path", "can't open for writing",
					p, strerror(errno));
			strcpy(output_path, p);
		} else if (strcmp("--service", a) == 0) {
			char *s, *cp;

			if (host[0])
				earg("service", "given twice");
			if (++i == argc)
				earg("service", "missing host:port");
			s = *ap++;

			cp = strchr(s, ':');
			if (!cp)
				earg2("service", "expected host:port", s);
			strncpy(host, s, cp - s);
			host[cp - s] = 0;
			if (!host[0])
				earg("service", "missing host");
			cp++;
			if (!*cp)
				earg("service", "missing port");
			if (sscanf(cp, "%u", &port) != 1)
				earg2("service", "can't scan port", cp);
			/*
			 *  Verify only up to 5 digits in port, after the colon.
			 */
			{
			 	static char non_digit[] =
					"--service: non-digits in port";
			 	static char extra_char[] =
					"--service: extra chars after port";
			 	char *p = cp, c;

				while (*p && p - cp < 5) {
					c = *p++;

					if (!isdigit(c))
						earg2("service", non_digit, cp);
				}
				if (*p)
					earg2("service", extra_char, cp);
			}
			if (port == 0)
			 	earg("service", "port is 0");
			if (port >= 65536)
				earg2("service", "port >= 65536", cp);
		} else if (strcmp("--timeout", a) == 0) {
			char *s;

			if (++i == argc)
				earg("timeout", "missing seconds");
			s = *ap++;

			if (sscanf(s, "%u", &timeout) != 1)
				earg2("timeout", "unexpected seconds", s);
		} else if (strcmp("--help", a) == 0) {
			fputs(usage, stdout);
			leave(0);
		} else if (strcmp("--trace", a) == 0) {
			tracing = 1;
			trace("enabled");
		} else if (strncmp("--", a, 2) == 0) {
			error2("unknown option", a);
			leave(126);
		} else {
			error2("unknown argument", a);
			leave(126);
		}
	}

	/*
	 *  Allocate request structure.
	 */
	rp = (struct request *)malloc(sizeof *rp);
	if (!rp) {
		if (server_fd > 0)
			_close_socket(server_fd);
		die("malloc(request) failed: out of memory");
	}
	memset(rp, 0, sizeof *rp);
	strcpy(rp->verb, verb);

	/*
	 *  Verify consistency of various command line arguments.
	 *
	 *  Note:
	 *	Shouldn't some of this code move to verb callback?
	 */
	if (strcmp("eat", verb) == 0) {
		/*
		 *  Verify arguments for "eat" request from remote service.
		 */
		if (host[0]) {
			if (!digest[0])
				earg("udig", "missing required option");
			if (in_fd != 0)
				earg("input-path", "not allowed");
			if (out_fd != 1)
				earg("output-path", "not allowed");
			open_service(rp, "eat", host, port);
		} else {
			if (!algorithm[0])
				no_arg2("algorithm", "udig");
		}
	} else if (strcmp("empty", verb) == 0) {
		if (!digest[0])
			no_arg1("udig");
	} else {
		if (!host[0])
			no_arg1("service");
		if (strcmp("wrap", verb) && (!algorithm[0] || !digest[0]))
			no_arg2("algorithm", "udig");
		open_service(rp, verb, host, port);
	}

	strcpy(rp->algorithm, algorithm);
	if (digest[0])
		strcpy(rp->digest, digest);
	rp->module = (void *)m;
	rp->in_fd = in_fd;
	rp->out_fd = out_fd;
	rp->open_data = (void *)0;

	if (m && m->open(rp)) {
		if (tracing)
			trace2(algorithm, "calling module open()");
		die3(rp->verb, rp->algorithm, "module open() failed");
	}
	status = (*verb_cb)(rp);
	if (tracing) {
		char tbuf[1024];

		snprintf(tbuf, sizeof tbuf, "verb exit status: %d", status);
		trace(tbuf);
	}
	if (m && m->close(rp)) {
		if (tracing)
			trace2(algorithm, "calling module close()");
		die3(rp->verb, rp->algorithm, "module close() failed");
		m = 0;
	}
	/*
	 *  Shutdown the remote connection.  This code needs to be in leave().
	 */
	if (rp->server_fd > 0)
		_disconnect(rp);
	leave(status);

	/*NOTREACHED*/
	exit(0);
}
