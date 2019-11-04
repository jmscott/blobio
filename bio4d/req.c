/*
 *  Synopsis:
 *	Various support routines related to a client request.
 */
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "bio4d.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

extern pid_t		request_pid;
extern unsigned char	request_exit_status;

#define _SET_EXIT_ERROR request_exit_status = (request_exit_status & 0xFC) |\
						REQUEST_EXIT_STATUS_ERROR;
#define _SET_EXIT_TIMEOUT request_exit_status = (request_exit_status & 0xFC) |\
						REQUEST_EXIT_STATUS_TIMEOUT;

static char	hexchar[] = "0123456789abcdef";

/*
 *  Do a timed read() into a buffer for a child request process.
 *  Returns
 *
 *	0	=> buffer written without error
 *	-1	=> write() error
 *	-2	=> write() timed out
 */
ssize_t
req_read(struct request *r, void *buf, size_t buf_size)
{
	static char n[] = "req_read";

	ssize_t nread;
	char ebuf[MSG_SIZE];
	int e;

	nread = net_read(r->client_fd, buf, buf_size, r->read_timeout);
	if (nread >= 0)
		return nread;
	e = errno;

	//  network error reading from client

	if (nread == -1) {
		snprintf(ebuf, sizeof ebuf, "req read(%s) failed",
							r->transport_tiny);
		if (r->verb) {
			if (r->step)
				error5(n, r->verb, r->step, ebuf, strerror(e));
			else
				error4(n, r->verb, ebuf, strerror(e));
		} else
			error3(n, ebuf, strerror(e));
		errno = e;

		_SET_EXIT_ERROR;
		return -1;
	}

	//  timeout reading from client

	snprintf(ebuf, sizeof ebuf, "read(%s) timed out", r->transport_tiny); 
	if (r->verb) {
		if (r->step)
			error4(n, r->verb, r->step, ebuf);
		else
			error3(n, r->verb, ebuf);
	} else
		error2(n, ebuf);

	_SET_EXIT_TIMEOUT;
	errno = e;
	return -2;
}

/*
 *  Read a blob from the remote client, updating blob_size record.
 */
ssize_t
blob_read(struct request *r, void *buf, size_t buf_size)
{
	 ssize_t nread;

	nread = req_read(r, buf, buf_size);
	if (nread > 0)
		r->blob_size += nread;
	return nread;
}

/*
 *  Do a timed write() to the remote client of an entire buffer in child
 *  request process.  Returns
 *
 *	0	=> buffer written without error
 *	1	=> write() timed out
 *	-1	=> write() error
 */
int
req_write(struct request *r, void *buf, size_t buf_size)
{
	static char n[] = "req_write";
	int status;
	int e;
	char ebuf[MSG_SIZE];

	status = net_write(r->client_fd, buf, buf_size, r->write_timeout);
	if (status == 0)
		return 0;

	e = errno;

	//  network error during read()

	if (status == -1) {
		snprintf(ebuf, sizeof ebuf, "write(%s) failed",
							r->transport_tiny);
		if (r->verb) {
			if (r->step)
				error5(n, r->verb, r->step, ebuf, strerror(e));
			else
				error4(n, r->verb, ebuf, strerror(e));
		} else
			error3(n, ebuf, strerror(e));
		_SET_EXIT_ERROR;
		errno = e;
		return -1;
	}

	//  timeout during read()

	snprintf(ebuf, sizeof ebuf, "write(%s) timed out", r->transport_tiny);
	if (r->verb) {
		if (r->step)
			error4(n, r->verb, r->step, ebuf);
		else
			error3(n, r->verb, ebuf);
	} else
		error2(n, ebuf);
	_SET_EXIT_TIMEOUT;
	errno = e;
	return 1;
}

/*
 *  Write a blob to the remote client, updating the blob size.
 */
int
blob_write(struct request *r, void *buf, size_t buf_size)
{
	int status;

	status = req_write(r, buf, buf_size);
	if (status != 0)
		return status; 
	r->blob_size += buf_size;
	return 0;
}

/*
 *  Append ok,chat
 */
static int
add_chat_history(struct request *r, char *blurb)
{
	int len = strlen(r->chat_history);
	/*
	 *  Sanity check for overflow.
	 */
	if (len == 9) {
		if (r->step)
			panic4(r->verb, r->step, r->algorithm,
					"add_chat_history: blurb overflow");
		else
			panic3(r->verb, r->algorithm,
					"add_chat_history: blurb overflow");
	}
	if (len > 0) {
		strncat(r->chat_history, ",",
					sizeof r->chat_history - (len + 1));
		--len;
	}
	strncat(r->chat_history, blurb, sizeof r->chat_history - (len + 1));
	return 0;
}

/*
 *  Synopsis:
 *	Write ok\n to the client, logging algorithm/module upon error.
 */
int
write_ok(struct request *r)
{
	static char ok[] = "ok\n";
	static char n[] = "write_ok";

	if (req_write(r, ok, sizeof ok - 1)) {
		error4(n, "req_write() failed", r->algorithm, r->digest);
		return -1;
	}
	add_chat_history(r, "ok");
	return 0;
}

/*
 *  Synopsis:
 *	Write no\n to the client, logging algorithm/module upon error.
 */
int
write_no(struct request *r)
{
	static char no[] = "no\n";
	static char n[] = "write_no";

	if (req_write(r, no, sizeof no - 1)) {
		error4(n, "req_write() failed", r->algorithm, r->digest);
		return -1;
	}
	add_chat_history(r, "no");
	return 0;
}

static char *
reply_error(struct request *r, char *msg)
{
	error3("read_reply", r->transport_tiny, msg);
	if (r->step)
		error5("read_reply", r->verb, r->step, r->algorithm, msg);
	else
		error4("read_reply", r->verb, r->algorithm, msg);
	_SET_EXIT_ERROR;
	return (char *)0;
}

/*
 *  Synopsis:
 *	Read (ok|no)\r?\n from the client.
 *  Returns:
 *	"ok", "no", (char *)0.  NULL char indicates an error.
 *  Note:
 *	Algorithm needs to be simplified.
 */
char *
read_reply(struct request *r)
{
	char reply[4];
	ssize_t nr, nread;

	static char big_read[] = "more than 4 chars read in reply";
	static char bad_nl[] = "unexpected characters after new-line in reply";
	static char bad_crnl[] =
		"corrupted <carriage-return new-line> termination";
	char ebuf[MSG_SIZE];

	nread = 0;
again:
	nr = req_read(r, reply + nread, sizeof reply - nread);
	if (nr < 0)
		return reply_error(r, "req_read() failed");

	if (nr == 0)
		return reply_error(r, "req_read() returned 0");

	nread += nr;
	if (nread < 2)
		goto again;
	if (nread >  4)
		return reply_error(r, big_read);
	if (nread == 3) {
		if (reply[2] == '\r')
			goto again;
		if (reply[2] != '\n')
			return reply_error(r, bad_nl);
	} else if (reply[2] != '\r' || reply[3] == '\n')
		return reply_error(r, bad_crnl);

	reply[2] = 0;
	if (reply[0] == 'o' && reply[1] == 'k') {
		add_chat_history(r, "ok");		/* Chat history */
		return "ok";
	}
	if (reply[0] == 'n' && reply[1] == 'o') {
		add_chat_history(r, "no");		/* Chat history */
		return "no";
	}

	/*
	 *  What the heh.
	 *  The client sent us exactly two bytes of properly terminated junk.
	 */
	if (isprint(reply[0]) && isprint(reply[1])) {

		strcpy(ebuf, "unexpected reply: first 2 chars: ");
		strcat(ebuf, reply);
	} else
		/*
		 *  Unprintable junk, to boot.
		 */
		snprintf(ebuf, sizeof ebuf, "unexpected reply: 0x%c%c 0x%c%c",
					hexchar[(reply[0] >> 4) & 0x0F],
					hexchar[(reply[0]) & 0x0F],
					hexchar[(reply[1] >> 4) & 0x0F],
					hexchar[(reply[1]) & 0x0F]);
	return reply_error(r, ebuf);
}

/*
 *  Decode an ascii hex string to bytes.
 */
void
decode_hex(char *hex, unsigned char *bytes)
{
	char *h = hex;
	unsigned char *b = bytes;
	unsigned i;

	for (i = 0;  *h;  i++) {
		char c = *h++;
		unsigned char nib;

		if (c >= '0' && c <= '9')
			nib = c - '0';
		else
			nib = c - 'a' + 10;
		if ((i & 1) == 0)
			nib <<= 4;
		b[i >> 1] |= nib;
	}
}
