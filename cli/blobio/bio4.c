/*
 *  Synopsis:
 *	A client driver for a paranoid blobio service 'bio4' over TCP/IP4
 *  Note:
 *	Getting an empty blob should always be true and never depend upon the
 *	underlying service driver!
 *
 *	Need to replace most tracing with i/o tracing in {_read,_write}().
 *
 *	Timeout not active when waiting for for the reply from a blob with
 *	incorrect digest.  The remote times out, but the client "blobio"
 *	does not!
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
#include <arpa/inet.h>
#include <stdio.h>

#include "blobio.h"

extern int	io_timeout;

//  Note: where is HOST_NAME_MAX defined on OS X?

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	64
#endif

static int server_fd = -1;

extern struct service bio4_service;		//  initialized below

/*
 *  Parse the host name/ip4, port and optional timeout and trust file
 *  system options from the end point.
 */
static char *
bio4_end_point_syntax(char *endp)
{
	char *ep, c;
	char buf[6], *bp;

	TRACE2("end point", endp);

	/*
	 *  Extract the ascii DNS host or ip4 address.
	 *
	 *  Note:
	 *	Incorrectly we assume DNS host names are ascii!!!!
	 */
	ep = endp;
	while ((c = *ep++) && c != ':') {
		if (!isascii(c))
			return "non ascii character in host";
		if (!isalnum(c) && c != '.' && c != '-')
			return "non alpha numeric or . char in host";
	}
	if (c != ':')
		return "no colon at after hostname or ip4";
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
	if (!c)
		return (char *)0;
	/*
	 *  parse query fragments for tmo or trusted file system
	 *  in the uri:
	 *
	 *	?tmo=[0-9]{1,3}
	 *	?tfs=true
	 */
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
bio4_connect(char *host, int port, int *p_server_fd)
{
	struct hostent *h;
	struct sockaddr_in s;
	int trys;
	int fd;

	/*
	 *  Look up the host name, trying up to 5 times.
	 */
	trys = 0;
AGAIN1:
	h = gethostbyname(host);
	trys++;
	if (!h) {
		TRACE2("gethostbyname() failed", host);
		/*
		 *  Map the h_errno value set by gethostbyname() onto
		 *  something reasonable.
		 */
		switch (h_errno) {
		case TRY_AGAIN:
			if (trys < 5)
				goto AGAIN1;
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
			 *  error code set for a few cases.
			 */
			return ENOENT;
		case NO_RECOVERY:
			return EIO;
		}
		return EINVAL;			/* wtf?? */
	}

	/*
	 *  Open and connect to the remote blobio server.
	 */
AGAIN2:
	TRACE("creating socket to remote ...");
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		int e = errno;

		if (e == EAGAIN || e == EINTR)
			goto AGAIN2;
		TRACE2("socket(remote) failed", strerror(e));
		return e;
	}
	TRACE("socket() created");
	memset(&s, 0, sizeof s);
	s.sin_family = AF_INET;
	memmove((void *)&s.sin_addr.s_addr, (void *)h->h_addr, h->h_length);
	s.sin_port = htons((short)port);

	/*
	 *  Need to timeout connect() !!
	 */
AGAIN3:
	TRACE("connecting to socket ...");

	if (connect(fd, (const struct sockaddr *)&s, sizeof s) < 0) {
		int e = errno;

		TRACE2("connect(remote) failed", strerror(e));

		if (e == EINTR || e == EAGAIN)
			goto AGAIN3;

		jmscott_close(fd);
		return e;
	}
	*p_server_fd = fd;

	//  grab the remote socket for the transport brr field
	struct sockaddr_in peer;
	socklen_t peer_len;
	if (getsockname(fd, (struct sockaddr *)&peer, &peer_len) != 0) {
		int e = errno;
		TRACE2("getpeername(remote) failed", strerror(e));
		return e;
	}
	snprintf(transport, sizeof transport - 1,
		"tcp4~%s:%u;%s:%u",
		jmscott_net_32addr2text(ntohl(s.sin_addr.s_addr)),
		(unsigned int)ntohs(s.sin_port),
		jmscott_net_32addr2text(ntohl(peer.sin_addr.s_addr)),
		(unsigned int)ntohs(peer.sin_port)
	);
	TRACE2("transport", transport);
	return 0;
}

static char *
bio4_open_output()
{
	int fd;
	int flag = O_WRONLY | O_CREAT;

	TRACE("entered");

	if (output_path != null_device)
		flag |= O_EXCL;		//  fail if file exists (and not null)

	fd = jmscott_open(output_path, flag, S_IRUSR | S_IRGRP);
	if (fd < -1)
		return strerror(errno);
	output_fd = fd;
	TRACE("opened");
	return (char *)0;
}

static char *
bio4_open()
{
	int status;
	char host[HOST_NAME_MAX + 1], *p;
	char pbuf[6];
	int port = 0;
	char *ep = bio4_service.end_point;

	if (output_path) {
		char *err = bio4_open_output();
		if (err)
			return err;
	}

	TRACE2("end point", bio4_service.end_point);

	if (algo[0])
		return "service query arg \"algo\" can not exist for bio4";

	p = strchr(ep, ':');
	memcpy(host, ep, p - ep);
	host[p - ep] = 0;
	p++;
	TRACE2("port fragment", p);
	strcpy(pbuf, p);

	port = atoi(pbuf);

	/*
	 *  Connect to service.
	 */
	switch (status = bio4_connect(host, port, &server_fd)) {
	case 0:
		break;
	case ENOENT:
		return "can't resolve host name";
	default:
		return strerror(status);
	}
	TRACE("open() done");

	return (char *)0;
}

static char *
bio4_close()
{
	if (server_fd >= 0 && jmscott_close(server_fd))
		return strerror(errno);

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
		if (ascii_digest[0]) {
			*c++ = ':';

			p = ascii_digest;
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
	char buf[MAX_ATOMIC_MSG];

	TRACE2(func, "caught io timeout alarm");

	buf[0] = 0;
	buf2cat(buf, sizeof buf, func, "() timed out");

	die_timeout(buf);
}

static void
catch_write_ALRM(int sig)
{
	TRACE("catch_write_ALRM");

	(void)sig;
	die_ALRM("write");
}

/*
 *  Synopsis:
 *	Write bytes with tracing and io timeout.
 */
static char *
_write(int fd, unsigned char *buf, int buf_size)
{
	char *err = 0;

	if (io_timeout > 0) {
		if (signal(SIGALRM, catch_write_ALRM) == SIG_ERR)
			return strerror(errno);
		alarm((unsigned int)io_timeout);
	}
#ifdef COMPILE_TRACE
	if (tracing) {
		TRACE("hex dump bytes written");
		hexdump((unsigned char *)buf, buf_size, '>');
	}
#endif
	if (jmscott_write_all(fd, (unsigned char *)buf, buf_size))
		err = strerror(errno);
	if (io_timeout > 0) {
		alarm(0);
		if (signal(SIGALRM, SIG_IGN) == SIG_ERR && !err)
			err = strerror(errno);
	}
	return err;
}

static char *
_read(int fd, unsigned char *buf, int buf_size, int *nread)
{
	char *err = 0;
	int nr;

	TRACE("request to read()");

	int ms = io_timeout * 1000;
	nr = jmscott_read_timeout(fd, (unsigned char *)buf, buf_size, ms);
	if (nr < 0) {
		if (nr == -2) {
			TRACE_ULL("io timeout secs", io_timeout);
			err = "i/o read() timeout";
		} else
			err = strerror(errno);
	}

#ifdef COMPILE_TRACE
	if (err) {
		TRACE2("ERROR", err);
	} else if (tracing) {
		if (nr > 0) {
			TRACE("hex dump bytes read");
			hexdump((unsigned char *)buf, nr, '>');
		} else if (nr == 0) {
			TRACE("read() returned 0 bytes");
		} else {
			TRACE("imposible: read error and err==null");
		}
	}
#endif
	if (err == (char *)0)
		*nread = nr;
	TRACE("read() done");
	return err;
}

/*
 *  Read a response from a server.  Write 0 if 'ok' returned,
 *  wrte 1 if 'no', or  return char string describing error.
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
	if (nread != 3)
		return "reply not three characters";

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
 *  Set global variables related to blog request records.
 *
 *  Note:
 *	Really, really need to push this code about the service level.
 *	Too much replication between services "fs" and "bio4".
 */
static char *
bio4_set_brr(char *hist)
{
	TRACE2("chat history", hist);
	strcpy(chat_history, hist);

	return (char *)0;
}

/*
 *  Get a blob from the server.
 */
static char *
_bio4_get(int *ok_no, int in_take)
{
	unsigned char buf[MAX_ATOMIC_MSG];
	char *err;
	int more;
	int nread;

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
			TRACE("write ok");
			if (more)
				TRACE("more data to digest");
		}
#endif
	}
	if (err)
		return err;
	if (more)
		return "blob does not match digest";

	TRACE("ok, got blob that matched digest");
	TRACE("get() done");
	return bio4_set_brr("ok");
}

static char *
bio4_get(int *ok_no)
{
	return _bio4_get(ok_no, 0);
}

static char *
bio4_eat(int *ok_no)
{
	char *err;

	TRACE("request to eat()");

	err = request(ok_no);
	if (err)
		return err;

	TRACE("eat() done");
	return bio4_set_brr(*ok_no == 0 ? "ok" : "no");
}

static char *
_put_untrusted()
{
	TRACE("_put_untrusted");

	char *err;
	int nread, more;
	unsigned char buf[MAX_ATOMIC_MSG];

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
			TRACE("write ok");
			if (more)
				TRACE("more data to digest");
		}
#endif
	}
	if (more)
		return "blob does not match digest";
	return (char *)0;
}

static char *
_put_trusted()
{
	TRACE("_put_trusted");

	char *err;
	int nread;
	unsigned char buf[MAX_ATOMIC_MSG];

	nread = -1;
	while (nread != 0) {
		err = _read(input_fd, buf, sizeof buf, &nread);
		if (err)
			return err;
		err = _write(server_fd, buf, nread);
		if (err)
			return err;
	}
	return (char *)0;
}

static char *
bio4_put(int *ok_no)
{
	char *err;
	char req[5 + 8 + 1 + 128 + 1 + 1];

	TRACE("request to put()");

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

	if (trust_fs == 0)
		err = _put_untrusted();
	else
		err = _put_trusted();
	if (err)
		return err;

	//  read "ok" or "no" reply from the server

	if ((err = read_ok_no(ok_no)))
		return err;

	TRACE("put() done");
	return bio4_set_brr(*ok_no == 0 ? "ok" : "no");
}

static char *
bio4_take(int *ok_no)
{
	char *err;

	TRACE("request to take()");

	//  write "take <udig>\n and read back reply and possibly the blob

	if ((err = _bio4_get(ok_no, 1)))
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

	return bio4_set_brr("ok,ok,ok");
}

static char *
bio4_give(int *ok_no)
{
	char *err;

	TRACE("request to give()");

	//  write "give <udig>\n and read back reply and possibly the blob

	if ((err = bio4_put(ok_no)))
		return err;

	//  server replied no

	if (*ok_no == 1)
		return (char *)0;

	TRACE("blob sent to server");

	//  server replied "ok", which implies the blob was accepted, so remove
	//  the local input.

	if (input_path) {
		int status = jmscott_unlink(input_path);

		if (status == -1 && errno != ENOENT) {

			int e = errno;
			_write(server_fd, (unsigned char *)"no\n", 3);
			return strerror(e);
		}
		if (_write(server_fd, (unsigned char *)"ok\n", 3))
			return strerror(errno);
	} else if (_write(server_fd, (unsigned char *)"ok\n", 3))
		return strerror(errno);

	return bio4_set_brr("ok,ok,ok");
}

static char *
bio4_roll(int *ok_no)
{
	char *err;

	TRACE("request to roll()");
	if ((err = request(ok_no)))
		return err;
	return bio4_set_brr("ok");
}

static char *
bio4_wrap(int *ok_no)
{
	char *err;
	char udig[8+1+128+1+1];		//  <algo>:<digest>\n\0
	unsigned int nread = 0;

	TRACE("entered");

	if ((err = request(ok_no)))	//  read chat history: ok|no
		return err;
	if (*ok_no)
		return (char *)0;

	//  read back 
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

	//  reply udig must be <= 35 ascii chars
	if (nread < (1 + 1 + 32 + 1))
		return "udig from server < 35 chars";
	if (nread >= sizeof udig - 1)
		return "failed to read udig from server";

	//  zap the newline and frisk the wrap udig
	udig[nread - 1] = 0;
	err = jmscott_frisk_udig(udig);
	if (err)
		return err;

	if (brr_mask_is_set(verb, brr_mask)) {
		//  update vars algorithm[] and ascii_digest[] for a the
		//  client side blob request record.
		if (!memccpy(algorithm, udig, ':', 8))
			return "memccpy(algo) return unexpected null";
		char *colon = index(udig, ':');
		if (!colon)
			return "index(colon) is null";
		colon++;
		strcpy(ascii_digest, colon);
	}

	if (_write(output_fd, (unsigned char *)udig, nread))
		return strerror(errno);

	return bio4_set_brr("ok");
}

struct service bio4_service =
{
	.name			=	"bio4",
	.end_point_syntax	=	bio4_end_point_syntax,
	.open			=	bio4_open,
	.close			=	bio4_close,
	.get			=	bio4_get,
	.eat			=	bio4_eat,
	.put			=	bio4_put,
	.take			=	bio4_take,
	.give			=	bio4_give,
	.roll			=	bio4_roll,
	.wrap			=	bio4_wrap
};
