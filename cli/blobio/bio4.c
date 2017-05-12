/*
 *  Synopsis:
 *	A client driver for the very paranoid blobio service 'bio4' over TCP/IP4
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#include "blobio.h"

//  Note: where is HOST_NAME_MAX defined on OX X?

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	64
#endif

#ifdef COMPILE_TRACE

#define _TRACE(msg)		if (tracing) _trace(msg)
#define _TRACE2(msg1,msg2)	if (tracing) _trace2(msg1,msg2)
#define _TRACE3(msg1,msg2,msg3)	if (tracing) _trace3(msg1,msg2,msg3)
#define _TRACE3(msg1,msg2,msg3)	if (tracing) _trace3(msg1,msg2,msg3)
#define CTRACE(msg)		if (tracing) ctrace(msg)
#define CTRACE2(msg1,msg2)	if (tracing) ctrace2(msg1,msg2)

#else

#define _TRACE(msg)
#define _TRACE2(msg1,msg2)
#define _TRACE3(msg1,msg2,msg3)
#define _TRACE3(msg1,msg2,msg3)

#endif

extern char	*verb;
extern char	algorithm[9];
extern char	digest[129];
extern char	end_point[129];
extern char	*output_path;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;

static int server_fd = -1;
static unsigned int timeout =	25;

struct service bio4_service;

static void
_trace(char *msg)
{
	trace2("bio4", msg);
}

static void
_trace2(char *msg1, char *msg2)
{
	trace3("bio4", msg1, msg2);
}

static void
_trace3(char *msg1, char *msg2, char *msg3)
{
	trace4("bio4", msg1, msg2, msg3);
}

static void
ctrace(char *msg)
{
	_trace2("connect", msg);
}

static void
ctrace2(char *msg1, char *msg2)
{
	_trace3("connect", msg1, msg2);
}

/*
 *  Check the syntax of host:port.  Host name must be > 0 and < 128 bytes.
 *  Port must be >= 0 and <= 65535 as a parsed decimal unsigned int.
 *
 *  Note:
 *  	The host name is not checked for well formed UTF8.
 */
static char *
bio4_end_point_syntax(char *end_point)
{
	char *cp, *colon;

	_TRACE("request to end_point_syntax()");

	//  find colon in host:port

	colon = strchr(end_point, ':');
	if (!colon)
		return "missing colon in endpoint";

	//  verify the host is > 0 && < HOST_NAME_MAX bytes.

	if (colon == end_point)
		return "empty host";
	if (colon - end_point > HOST_NAME_MAX)
		return "host name is too many bytes";
	cp = colon + 1;

	//  verify the port is > 0 and < 6 ascii decimal digits

	if (*cp) {
		char c;

		while ((c = *cp++)) {
			if (!isdigit(c))
				return "non decimal digit in port";
			if (cp - colon >= 6)
				return "port has > 5 decimal digits";
		}
	} else
		return "missing port";

	//  verify port <= 65535

	if (atoi(colon + 1) > 65535)
		return "port > 65535";

	_TRACE("end_point_syntax() done");

	return (char *)0;
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

	CTRACE2("request connect() to remote host", host);

	/*
	 *  Look up the host name, trying up to 5 times.
	 */
	trys = 0;
again1:
	h = gethostbyname(host);
	trys++;
	if (!h) {
		CTRACE2("gethostbyname() failed", host);
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

	CTRACE("creating socket to remote ...");
	/*
	 *  Open and connect to the remote blobio server.
	 */
again2:
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		int e = errno;

		CTRACE2("socket() failed", strerror(e));

		if (e == EAGAIN || e == EINTR)
			goto again2;
		return e;
	}
	CTRACE("socket() created");
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	memmove((void *)&s.sin_addr.s_addr, (void *)h->h_addr, h->h_length);
	s.sin_port = htons((short)port);

	/*
	 *  Need to timeout connect() !!
	 */
again3:
	CTRACE("connecting to socket ...");

	if (connect(fd, (const struct sockaddr *)&s, sizeof s) < 0) {
		int e = errno;

		CTRACE2("connect() failed", strerror(e));

		if (e == EINTR || e == EAGAIN)
			goto again3;

		uni_close(fd);
		return e;
	}
	*p_server_fd = fd;
	CTRACE("_connect() done");
	return 0;
}

static char *
bio4_open()
{
	int status;
	char host[HOST_NAME_MAX + 1], *p;
	int port = 0;

	_TRACE("request to open()");

	p = strchr(end_point, ':');
	memcpy(host, end_point, p - end_point);
	host[p - end_point] = 0;
	port = atoi(p + 1);

	/*
	 *  Connect to service.
	 */
	switch (status = _connect(host, port, &server_fd)) {
	case 0:
		break;
	case ENOENT:
		return "can't resolve host name";
	default:
		return strerror(status);
	}
	_TRACE("open() done");

	return (char *)0;
}

static char *
bio4_close()
{
	_TRACE("request to close()");

	if (server_fd >= 0 && uni_close(server_fd))
		return strerror(errno);

	_TRACE("close() done");

	return (char *)0;
}

/*
 *  Assemble the command sent to the server.
 *
 *	get/put/give/take/eat/wrap/roll algorithm:digest
 *	wrap algorithm
 */
static int
copy_request(char *command)
{
	char *c, *p;

	c = command;
	*c++ = verb[0];
	*c++ = verb[1];
	*c++ = verb[2];
	if (verb[3])
		*c++ = verb[3];
	if (algorithm[0]) {
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
	}
	*c++ = '\n';
	*c = 0;
	return c - command;
}

static void
die_ALRM(char *func)
{
	char buf[PIPE_MAX];

	_TRACE2(func, "caught timeout alarm");

	buf[0] = 0;
	buf2cat(buf, sizeof buf, func, "() timed out");

	die(17, buf);
}

static void
catch_write_ALRM(int sig)
{
	(void)sig;
	die_ALRM("write");
}

static void
catch_read_ALRM(int sig)
{
	(void)sig;
	die_ALRM("read");
}

/*
 *  Synopsis:
 *	Write bytes with tracing and timeout
 */
static char *
_write(int fd, unsigned char *buf, int buf_size)
{
	_TRACE("request to write()");

	if (signal(SIGALRM, catch_write_ALRM) == SIG_ERR)
		return strerror(errno);

	alarm(timeout);
	if (uni_write_buf(fd, buf, buf_size))
		return strerror(errno);
	alarm(0);

	if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
		return strerror(errno);

#ifdef COMPILE_TRACE
	_TRACE("write() ok, hex dump follows ...");
	if (tracing)
		hexdump(buf, buf_size, '<');
#endif
	_TRACE("write() done");
	return (char *)0;
}

/*
 *  Synopsis:
 *	Do a partial read() of all bytes.
 */
static char *
_read(int fd, unsigned char *buf, int buf_size, int *nread)
{
	int nr;

	_TRACE("request to read()");

	if (signal(SIGALRM, catch_read_ALRM) == SIG_ERR)
		return strerror(errno);

	alarm(timeout);
	nr = uni_read(fd, buf, buf_size);
	if (nr < 0)
		return strerror(errno);
	alarm(0);

	//  Note: should the signal be reset to previous handler?

	if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
		return strerror(errno);

#ifdef COMPILE_TRACE
	if (tracing) {
		if (nr > 0) {
			_trace("read() ok, hex dump follows ...");
			hexdump(buf, nr, '>');
		} else
			_trace("read() returned 0 bytes");
	}
#endif
	*nread = nr;
	_TRACE("read() done");
	return (char *)0;
}

/*
 *  Read a response from a server.  Return 0 if 'ok' returned,
 *  return 1 if 'no', or  die() upon error.
 */
static char *
read_ok_no(int *reply)
{
	char buf[3], *err;
	int nread = 0;

	/*
	 *  Read a 3 character response from the server.
	 */
	while (nread < 3) {
		int nr;

		err =  _read(server_fd, (unsigned char *)buf, 3 - nread, &nr);
		if (err)
			return err;
		if (nr == 0)
			return "unexpected end of stream reading reply";
		nread += nr;
	}

	/*
	 *  Look for "ok\n" or "no\n" from the server.
	 */
	if (buf[2] == '\n') {
		if (buf[0] == 'o' && buf[1] == 'k')
			*reply = 0;
		else if (buf[0] == 'n' && buf[1] == 'o')
			*reply = 1;
		else
			return "server reply not \"ok\" or \"no\"";

	} else
		return "server reply missing new-line";
	return (char *)0;
}

/*
 *  Send a get/wrap/roll command to the remote server and
 *  read the reply.
 *
 *	give sha:f6e723cc3642009cb74c740bd14317b207d05923\n	
 *
 *  Return 0 if 'ok' is reply, 1 if 'no' is reply,
 *  -1 otherwise.
 */
static char *
request(int *ok_no)
{
	char *err;
	char req[5 + 8 + 1 + 128 + 1 + 1];

	if ((err = _write(server_fd, (unsigned char *)req, copy_request(req))))
		return err;

	/*
	 *  Read response from server.  We expect either.
	 *
	 *	ok\n
	 *	no\n
	 */
	if ((err = read_ok_no(ok_no)))
		return err;
	return (char *)0;
}

/*
 *  Get a blob from the server.
 */
static char *
_get(int *ok_no, int in_take)
{
	unsigned char buf[PIPE_MAX];
	char *err;
	int more;
	int nread;

	_TRACE("request to get()");

	err = request(ok_no);
	if (err)
		return err;

	//  blob server replied with no.

	if (*ok_no == 1 || (in_take && bio4_service.digest->empty()))
		return (char *)0;

	/*
	 *  Read the blob back from the server, incrementally
	 *  verifying the signature.
	 */
	more = 1;
	while (more) {

		err = _read(server_fd, buf, sizeof buf, &nread);
		if (err)
			return err;

		more = bio4_service.digest->get_update(buf, nread);

		if (nread == 0)
			break;

		/*
		 *  Partial blob verified, so write() to output.
		 */
		err = _write(output_fd, buf, nread);
		if (err)
			return err;
#ifdef COMPILE_TRACE
		if (tracing) {
			_trace("write ok");
			if (more)
				_TRACE("more data to digest");
		}
#endif
	}
	if (err)
		return err;
	if (more)
		return "blob does not match digest";

	_TRACE("ok, got blob that matched digest");
	_TRACE("get() done");
	return (char *)0;
}

static char *
bio4_get(int *ok_no)
{
	return _get(ok_no, 0);
}

static char *
bio4_open_output()
{
	int fd;
	fd = uni_open_mode(
			output_path,
			O_WRONLY | O_EXCL | O_CREAT,
			S_IRUSR | S_IRGRP
		);
	if (fd == -1)
		return strerror(errno);
	output_fd = fd;
	return (char *)0;
}

static char *
bio4_eat(int *ok_no)
{
	char *err;

	_TRACE("request to eat()");

	err = request(ok_no);
	if (err)
		return err;

	_TRACE("eat() done");
	return (char *)0;
}

static char *
bio4_put(int *ok_no)
{
	char *err;
	char req[5 + 8 + 1 + 128 + 1 + 1];
	int nread, more;
	unsigned char buf[PIPE_MAX];

	_TRACE("request to put()");

	//  write the put request to the remote server

	if ((err = _write(server_fd, (unsigned char *)req, copy_request(req))))
		return err;

	more = 1;
	while (more) {

		err = _read(input_fd, buf, sizeof buf, &nread);
		if (err)
			return err;

		more = bio4_service.digest->get_update(buf, nread);

		if (nread == 0)
			break;

		/*
		 *  Partial blob verified locally, so write() to server.
		 */
		err = _write(server_fd, buf, nread);
		if (err)
			return err;
#ifdef COMPILE_TRACE
		if (tracing) {
			_trace("write ok");
			if (more)
				_TRACE("more data to digest");
			hexdump(buf, nread, '<');
		}
#endif
	}
	if (more)
		return "blob does not match digest";

	//  read "ok" or "no" reply from the server

	if ((err = read_ok_no(ok_no)))
		return err;

	_TRACE("put() done");
	return (char *)0;
}

static char *
bio4_take(int *ok_no)
{
	char *err;

	_TRACE("request to take()");

	//  write "take <udig>\n and read back reply and possibly the blob

	if ((err = _get(ok_no, 1))) {
		uni_write_buf(server_fd, "no\n", 3);
		return err;
	}

	//  server replied no

	if (*ok_no == 1)
		return (char *)0;

	//  server replied "ok" and we read the blob successfully,
	//  so reply to server with "ok\n"

	if (uni_write_buf(server_fd, "ok\n", 3))
		return strerror(errno);
	if((err = read_ok_no(ok_no)))
		return err;

	_TRACE("take() done");
	return (char *)0;
}

static char *
bio4_give(int *ok_no)
{
	char *err;

	_TRACE("request to give()");

	//  write "give <udig>\n and read back reply and possibly the blob

	if ((err = bio4_put(ok_no)))
		return err;

	//  server replied no

	if (*ok_no == 1)
		return (char *)0;

	_TRACE("blob sent to server");

	//  server replied "ok" and we put the blob successfully,
	//  so remove the local input.

	if (input_path) {
		int status = uni_unlink(input_path);

		if (status == -1 && errno != ENOENT) {

			int e = errno;
			uni_write_buf(server_fd, "no\n", 3);
			return strerror(e);
		}
		if (uni_write_buf(server_fd, "ok\n", 3))
			return strerror(errno);
	} else if (uni_write_buf(server_fd, "ok\n", 3))
		return strerror(errno);

	_TRACE("give() done");
	return (char *)0;
}

static char *
bio4_roll(int *ok_no)
{
	char *err;

	_TRACE("request to roll()");
	if ((err = request(ok_no)))
		return err;
	_TRACE("roll() done");

	return (char *)0;
}

static char *
bio4_wrap(int *ok_no)
{
	char *err;
	char udig[8+1+128+1+1];
	unsigned int nread = 0;

	_TRACE("request to wrap()");

	if ((err = request(ok_no)))
		return err;
	if (*ok_no)
		return (char *)0;

	while (nread < sizeof udig -1) {
		int nr;
		
		nr = uni_read(server_fd, udig + nread, sizeof udig - 1 - nread);

		if (nr < 0)
			return strerror(errno);
		if (nr == 0)
			return "read() returned unexpected 0 bytes";
		nread += nr;
		if (udig[nread - 1] == '\n')
			break;
	}
	if (nread >= sizeof udig - 1)
		return "failed to read udig from server";

	if (uni_write_buf(output_fd, udig, nread))
		return strerror(errno);

	_TRACE("wrap() done");

	return (char *)0;
}

struct service bio4_service =
{
	.name			=	"bio4",
	.end_point_syntax	=	bio4_end_point_syntax,
	.open			=	bio4_open,
	.close			=	bio4_close,
	.get			=	bio4_get,
	.open_output		=	bio4_open_output,
	.eat			=	bio4_eat,
	.put			=	bio4_put,
	.take			=	bio4_take,
	.give			=	bio4_give,
	.roll			=	bio4_roll,
	.wrap			=	bio4_wrap
};
