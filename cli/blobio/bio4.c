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
extern char	*null_device;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;

static int server_fd = -1;
static char host[256];
static uint16_t port;
static unsigned int timeout =	20;

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
 *  Parse the host name/ip4, port and optional timeout from the end point.
 */
static char *
bio4_end_point_syntax(char *end_point)
{
	char *ep, c;
	char buf[6], *bp;

	TRACE2("end point syntax", end_point);

	/*
	 *  Extract the ascii DNS host or ip4 address.
	 */
	bp = host;
	ep = end_point;
	while ((c = *ep++) && c != ':') {
		if (!isalnum(c) && c != '.')
			return "non alpha numeric or . char in host/ip4";
		*bp++ = c;
	}
	if (c != ':')
		return "no colon at after hostname or ip4";
	*bp = 0;
	if (*ep == '?')
		return "no port number before query argument";
	/*
	 *  Extract port number > 0 and < 65536
	 */
	bp = buf;
	while ((c = *ep++) && isdigit(c)) {
		if (bp - buf > 5)
			return "> 5 chars in port number";
		*bp++ = c;
	}
	if (bp == buf)
		return "no port number after colon";
	*bp = 0;
	int p = atoi(buf);
	if (p == 0)
		return "port number can not be 0";
	if (p > 65535)
		return "port number > 65535";
	port = (uint16_t)p;
	if (!c)
		return (char *)0;

	/*
	 *  Is a timeout specified as a query fragment on the URI:
	 *
	 *	?tmo=[0-9]{1,3}
	 */
	if (c != '?')
		return "expected ? char after port number";
	if (*ep++ != 't' || *ep++ != 'm' || *ep++ != 'o' || *ep++ != '=')
		return "expected tmo= in query fragment";
	if (!isdigit(*ep))
		return "missing seconds after tmo= in query fragment";
	bp = buf;
	while ((c = *ep++)) {
		if (!isdigit(c))
			return "expected digit in tmo query fragment";
		if (bp - buf > 2)
			return "too many digits in timeout seconds";
		*bp++ = c;
	}
	*bp = 0;
	timeout = atoi(buf);
	if (timeout > 255)
		return "timeout > 255 seconds";
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
make_request(char *command)
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
	char *err = 0;

	_TRACE("request to write()");

	if (timeout > 0) {
		if (signal(SIGALRM, catch_write_ALRM) == SIG_ERR)
			return strerror(errno);
		alarm(timeout);
	}
	if (uni_write_buf(fd, (unsigned char *)buf, buf_size))
		err = strerror(errno);
	if (timeout > 0) {
		if (signal(SIGALRM, SIG_IGN) == SIG_ERR && !err)
			err = strerror(errno);
		alarm(0);
	}

#ifdef COMPILE_TRACE
	_TRACE("write() ok, hex dump follows ...");
	if (err) {
		_TRACE2("ERROR", err);
	} else if (tracing)
		hexdump((unsigned char*)buf, buf_size, '<');
	_TRACE("write() done");
#endif
	return err;
}

/*
 *  Synopsis:
 *	Do a partial read() of all bytes.
 */
static char *
_read(int fd, unsigned char *buf, int buf_size, int *nread)
{
	char *err = 0;
	int nr;

	_TRACE("request to read()");

	if (timeout > 0) {
		if (signal(SIGALRM, catch_read_ALRM) == SIG_ERR)
			return strerror(errno);
		alarm(timeout);
	}
	if ((nr = uni_read(fd, (unsigned char *)buf, buf_size)) < 0)
		err = strerror(errno);
		
	if (timeout > 0) {
		if (signal(SIGALRM, SIG_IGN) == SIG_ERR && !err)
			err = strerror(errno);
		alarm(0);
	}

#ifdef COMPILE_TRACE
	if (err) {
		_TRACE2("ERROR", err);
	} else if (tracing) {
		if (nr > 0) {
			_TRACE("read() ok, hex dump follows ...");
			hexdump((unsigned char *)buf, nr, '>');
		} else
			_TRACE("read() returned 0 bytes");
	}
#endif
	if (err == (char *)0)
		*nread = nr;
	_TRACE("read() done");
	return err;
}

/*
 *  Read a response from a server.  Return 0 if 'ok' returned,
 *  return 1 if 'no', or  die() upon error.
 */
static char *
read_ok_no(int *reply)
{
	char unsigned buf[3];
	char *err;
	int nread = 0;

	/*
	 *  Read a 3 character response from the server.
	 */
	while (nread < 3) {
		int nr;

		err =  _read(server_fd, buf, 3 - nread, &nr);
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
 *  Send a get/put/take/eat/wrap/roll command to the remote server and
 *  read the reply of either "ok" or "no".  Any other reply is an error,
 *  including a timeout.
 *
 *	give sha:f6e723cc3642009cb74c740bd14317b207d05923\n	
 *
 *  Set *ok_no 0 if 'ok' is reply, 1 if 'no' is reply;
 *  otherwise return error string.
 */
static char *
request(int *ok_no)
{
	char *err;
	char req[5 + 8 + 1 + 128 + 1 + 1];

	if ((err = _write(server_fd, (unsigned char *)req, make_request(req))))
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

		char *status = bio4_service.digest->get_update(buf, nread);
		if (*status == '0')
			more = 0;
		else if (*status != '1')
			return status;

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
	int flag = O_WRONLY | O_CREAT;

	if (output_path != null_device)
		flag |= O_EXCL;		//  fail if file exists (and not null)

	fd = uni_open_mode(output_path, flag, S_IRUSR | S_IRGRP);
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

	err = _write(
		server_fd,
		(unsigned char *)req,
		make_request(req)
	);
	if (err)
		return err;
	if ((err = read_ok_no(ok_no)))
		return err;

	if (*ok_no == 1)
		return (char *)0;		//  server said no

	//  write the blob to the server
	more = 1;
	while (more) {

		err = _read(input_fd, buf, sizeof buf, &nread);
		if (err)
			return err;

		char *status = bio4_service.digest->get_update(buf, nread);
		if (*status == '0')
			more = 0;
		else if (*status != '1')
			return status;

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
			hexdump((unsigned char *)buf, nread, '<');
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

	if ((err = _get(ok_no, 1)))
		return err;

	//  server replied no

	if (*ok_no == 1)
		return (char *)0;

	//  server replied "ok" and we read the blob successfully,
	//  so reply to server with "ok\n"

	if (_write(server_fd, (unsigned char *)"ok\n", 3))
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
			_write(server_fd, (unsigned char *)"no\n", 3);
			return strerror(e);
		}
		if (_write(server_fd, (unsigned char *)"ok\n", 3))
			return strerror(errno);
	} else if (_write(server_fd, (unsigned char *)"ok\n", 3))
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
		char *err;
		
		err = _read(
			server_fd,
			(unsigned char*)udig + nread,
			sizeof udig - 1 - nread,
			&nr
		);
		if (err)
			return err;

		if (nr == 0)
			return "read() returned unexpected 0 bytes";
		nread += nr;
		if (udig[nread - 1] == '\n')
			break;
	}
	if (nread >= sizeof udig - 1)
		return "failed to read udig from server";

	if (_write(output_fd, (unsigned char *)udig, nread))
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
