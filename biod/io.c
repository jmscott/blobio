/*
 *  Synopsis:
 *	Restartable i/o operations.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 *  Note:
 *  	Need to prefix the global functions with 'os' instead of 'io'
 *  	and rename this os.c
 *
 *  	Verify that EGAIN is tested properly.  I (jmscott) still not sure what
 *  	EGAIN REALLY means.
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
#include "biod.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

extern pid_t		request_pid;
extern unsigned char	request_exit_status;

static char	hexchar[] = "0123456789abcdef";
static int	alarm_caught = 0;

#define DUMP_BUF_SIZE		(10 * 1024)
#define SLURP_TEXT_TIMEOUT	10
#define BURP_TEXT_TIMEOUT	10

/*
 *  Synopsis:
 *	Ye' old hex dumper
 *  Example:
 *
     0  >  67 65 74 20 70 69 6e 67 3a 61 62 63 64 65 66 0a  get ping:abcdef.   
    16  >  3a 35 34 20 43 44 54 09 31 32 37 2e 30 2e 30 2e  :54 CDT.127.0.0.
    32  >  01 02 03                                         ... 

0.........1.........2.........3.........4.........5.........6.........7.........

 *	Each formattedd line is 76 chars plus new line.
 *	The characters '>' and '<' represent either read or write direction,
 *	respectively. 
 *
 *  Note:
 *	If the source buffer requires more space than the target buffer
 *	can provide, then none of the source buffer is formated,
 *	which is hosed.  This is a mistake.  Instead, the source
 *	should be truncated and a truncation message should be written
 *	to the target.
 */
#ifdef NO_COMPILE
static void
dump_format(unsigned char *src, int src_size,
	    char direction, char *tgt, int tgt_size)
{
	char *t;
	unsigned char *s, *s_end;
	int need;

	/*
	 *  Insure we have plenty of room in the buffer.
	 *  We need the (number of lines) * (76 + 1) characters per line
	 *  + single trailing null termination char.
	 *
	 *	((nbytes - 1) / 16 + 1) * 76 + 1
	 *
	 *  Probably ought to truncate buffer instead of punting.
	 */
	need = ((src_size - 1) / 16 + 1) * (76 + 1) + 1;
	if (need > tgt_size) {
		snprintf(tgt, tgt_size,
			"      dump_format: source bigger than target: %d > %d",
				need, tgt_size);
		return;
	}

	s = src;
	s_end = s + src_size;
	t = tgt;

	while (s < s_end) {
		int i;

		/*
		 *  Start of line in the target.
		 */
		char *l = t;

		/*
		 *  Write the offset into the source buffer.
		 */
		sprintf(l, "%6lu", (long unsigned)(s - src));

		/*
		 *  Write direction of flow
		 */
		l[6] = l[7] = ' ';
		l[8] = direction;
		l[9] = ' ';

		/*
		 *  Format up to next 16 bytes from the source.
		 */
		for (i = 0;  s < s_end && i < 16;  i++) {
			l[10 + i * 3] = ' ';
			l[10 + i * 3 + 1] = hexchar[(*s >> 4) & 0x0F];
			l[10 + i * 3 + 2] = hexchar[*s & 0x0F];
			if (isprint(*s))
				l[60 + i] = *s;
			else
				l[60 + i] = '.';
			s++;
		}
		/*
		 *  Pad out last line with white space.
		 */
		while (i < 16) {
			l[10 + i * 3] =
			l[10 + i * 3 + 1] =
			l[10 + i * 3 + 2] = ' ';
			l[60 + i] = ' ';
			i++;
		}
		l[58] = l[59] = ' ';
		l[76] = '\n';
		t = l + 77;
	}
	tgt[need - 1] = 0;
}

#endif

static void
catch_SIGALRM(int sig)
{
	UNUSED_ARG(sig);

	if (request_pid)
		request_exit_status = (request_exit_status & 0xFC) |
						REQUEST_EXIT_STATUS_TIMEOUT;
	alarm_caught = 1;
}

/*
 *  Write an entire buffer to a file descriptor.
 */
int
write_buf(int fd, unsigned char *buf, int buf_size, unsigned timeout)
{
	int nwrite;
	unsigned char *b, *b_end;
	struct sigaction a;
	struct itimerval t;
	int e;

	b = buf;
	b_end = buf + buf_size;

	/*
	 *  Set the timeout alarm handler.
	 */
	memset(&a, 0, sizeof a);

	alarm_caught = 0;
	a.sa_handler = catch_SIGALRM;
	a.sa_flags = 0;
	sigemptyset(&a.sa_mask);
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	t.it_value.tv_sec = timeout;
	t.it_value.tv_usec = 0;
again:
	/*
	 *  Set the ALRM handler.
	 */
	if (sigaction(SIGALRM, &a, (struct sigaction *)0))
		panic2("write_buf: sigaction(ALRM) failed", strerror(errno));
	/*
	 *  Set the timer
	 */
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("write_buf: setitimer(REAL) failed", strerror(errno));
	nwrite = write(fd, (void *)b, b_end - b);
	e = errno;

	/*
	 *  Disable the timer.
	 */
	t.it_value.tv_sec = 0;
	t.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("write_buf: setitimer(REAL, 0) failed",
							strerror(errno));
	if (nwrite < 0) {
		if (e == EINTR || e == EAGAIN) {
			if (alarm_caught) {
				char tbuf[20];

				snprintf(tbuf, sizeof tbuf,
						"timeout is %d secs", timeout);
				error2("write_buf: write() interupted", tbuf);
				return -1;
			}
			goto again;
		}
		error2("write_buf: write() failed", strerror(errno));
		return -1;
	}
	b += nwrite;
	if (b < b_end)
		goto again;
	return 0;
}

/*
 *  Synopsis:
 *	Accept incoming socket, possibly timing out the request.
 *  Returns:
 *	0	new socket accepted
 *	1	timed out the request
 *	-1	error, see errno.
 *  Notes:
 *	client_fd only changes on success.
 */
int
io_accept(int listen_fd, struct sockaddr *addr, socklen_t *len,
          int *client_fd, unsigned timeout)
{
	int fd, e;
	struct sigaction a;
	struct itimerval t;

	/*
	 *  Set the timeout alarm handler.
	 */
	memset(&a, 0, sizeof a);
	alarm_caught = 0;
	a.sa_handler = catch_SIGALRM;
	a.sa_flags = 0;
	sigemptyset(&a.sa_mask);
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	t.it_value.tv_sec = timeout;
	t.it_value.tv_usec = 0;
again:
	/*
	 *  Set the ALRM handler.
	 */
	if (sigaction(SIGALRM, &a, (struct sigaction *)0))
		panic2("io_accept: sigaction(ALRM) failed", strerror(errno));
	/*
	 *  Set the timer
	 */
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("io_accept: setitimer(REAL) failed", strerror(errno));

	/*
	 *  Accept an incoming client connection.
	 */
	fd = accept(listen_fd, addr, len);
	e  = errno;

	/*
	 *  Disable the timer.
	 */
	t.it_value.tv_sec = 0;
	t.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("io_accept: setitimer(REAL, 0) failed", strerror(errno));
	if (fd > -1) {
		*client_fd = fd;
		return 0;
	}

	/*
	 *  Error or timeout.
	 */
	if (e == EINTR || e == EAGAIN) {
		/*
		 *  Timeout.
		 */
		if (alarm_caught)
			return 1;
		goto again;
	}
	error2("io_accept: accept() failed", strerror(e));
	return -1;
}

/*
 *  Synopsis:
 *	Read from a file descriptor, timeing out a read.
 */
int
read_buf(int fd, unsigned char *buf, int buf_size, unsigned timeout)
{
	int nread;
	int e;
	struct sigaction a;
	struct itimerval t;

	/*
	 *  Set the timeout alarm handler.
	 */
	memset(&a, 0, sizeof a);
	alarm_caught = 0;
	a.sa_handler = catch_SIGALRM;
	a.sa_flags = 0;
	sigemptyset(&a.sa_mask);
	t.it_interval.tv_sec = 0;
	t.it_interval.tv_usec = 0;
	t.it_value.tv_sec = timeout;
	t.it_value.tv_usec = 0;
again:
	/*
	 *  Set the alarm handler.
	 */
	if (sigaction(SIGALRM, &a, (struct sigaction *)0))
		panic2("read_buf: sigaction(ALRM) failed", strerror(errno));
	/*
	 *  Set the interval time.
	 */
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("read_buf: setitimer(REAL) failed", strerror(errno));
	nread = read(fd, (void *)buf, buf_size);
	e = errno;

	/*
	 *  Disable timer.
	 */
	t.it_value.tv_sec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("read_buf: setitimer(REAL) failed", strerror(errno));
	if (nread < 0) {
		if (e == EINTR) {
			if (alarm_caught) {
				char tbuf[27];

				alarm_caught = 0;
				snprintf(tbuf, sizeof tbuf,
						"timeout is %u secs", timeout);
				error2(
				    "read_buf: read() interupted: alarm caught",
				    tbuf
				);
				return -1;
			}
			goto again;
		}
		if (e == EINTR || e == EAGAIN)
			goto again;
		error2("read_buf: read() failed", strerror(e));
		return -1;
	}
	return nread;
}

/*
 *  Record history of no/ok chats with client.
 */
static int
add_chat_history(struct request *rp, char *blurb)
{
	int len = strlen(rp->chat_history);
	/*
	 *  Sanity check for overflow.
	 */
	if (len == 9)
		panic3(rp->verb, rp->algorithm,
					"add_chat_history: blurb overflow");
	if (len > 0) {
		strncat(rp->chat_history, ",",
					sizeof rp->chat_history - (len + 1));
		--len;
	}
	strncat(rp->chat_history, blurb, sizeof rp->chat_history - (len + 1));
	return 0;
}

/*
 *  Synopsis:
 *	Write ok\n to the client, logging algorithm/module upon error.
 */
int
write_ok(struct request *rp)
{
	static char ok[] = "ok\n";
	static char n[] = "write_ok";
	/*
	 *  Sanity checks.
	 */
	if (rp == NULL)
		panic4(n, rp->verb, rp->algorithm, "struct request * is null");

	/*
	 *  Sanity checks on client file descriptor.
	 */
	if (rp->client_fd < 0)
		panic4(n, rp->verb, rp->algorithm, "client file is < 0");

	/*
	 *  Record time to first byte:
	 */
	if (rp->ttfb_time.tv_sec == 0)
		if (clock_gettime(CLOCK_REALTIME, &rp->ttfb_time) < 0)
			panic3(n, "clock_gettime(start REALTIME) failed",
							strerror(errno));
	if (write_buf(rp->client_fd, (unsigned char *)ok, sizeof ok - 1,
		rp->write_timeout)) {
		error5(n, rp->verb, rp->algorithm, rp->netflow,
						       "write_buf() failed");
		return -1;
	}
	add_chat_history(rp, "ok");
	return 0;
}

/*
 *  Synopsis:
 *	Write no\n to the client, logging algorithm/module upon error.
 */
int
write_no(struct request *rp)
{
	static char no[] = "no\n";
	static char n[] = "write_no";

	/*
	 *  Sanity checks.
	 */
	if (rp == NULL)
		panic3(rp->verb, rp->algorithm,
					"write_no: struct request * is null");
	/*
	 *  Sanity checks.
	 */
	if (rp->client_fd < 0)
		panic4(n, rp->verb,rp->algorithm,"client file descriptior < 0");

	/*
	 *  Record time to first byte:
	 */
	if (rp->ttfb_time.tv_sec == 0)
		if (clock_gettime(CLOCK_REALTIME, &rp->ttfb_time) < 0)
			panic3(n, "clock_gettime(start REALTIME) failed",
							strerror(errno));
	if (write_buf(rp->client_fd, (unsigned char *)no, sizeof no - 1,
		      rp->write_timeout)) {
		error5(n, rp->verb, rp->algorithm, rp->netflow,
					"write_buf() failed");
		return -1;
	}
	add_chat_history(rp, "no");
	return 0;
}

int
read_blob(struct request *rp, unsigned char *buf, int size)
{
	int nread;

	nread = read_buf(rp->client_fd, buf, size, rp->read_timeout);
	if (nread < 0) {
		error4(rp->verb, rp->algorithm, rp->netflow,
					"read_blob: read_buf() failed");
		return -1;
	}
	rp->blob_size += nread;
	return nread;
}

/*
 *  Write a chunk of blob to the client.
 */
int
write_blob(struct request *rp, unsigned char *buf, int nbytes)
{
	if (rp == NULL)
		panic3(rp->verb, rp->algorithm,
					"write_blob: struct request * is null");
	if (rp->client_fd < 0)
		panic3(rp->verb, rp->algorithm,
				"write_blob: client file descriptor < 0");
	if (buf == NULL)
		panic3(rp->verb, rp->algorithm, "write_blob: null blob buf");
	if (write_buf(rp->client_fd, buf, nbytes, rp->write_timeout)) {
		error4(rp->verb, rp->algorithm, rp->netflow,
					"write_blob: write_buf() failed");
		return -1;
	}
	rp->blob_size += nbytes;
	return 0;
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
read_reply(struct request *rp)
{
	char reply[4];
	int status, nread;
	static char bad_nl[] =
		"unexpected characters after new-line in reply";
	static char bad_crnl[] =
		"expected new-line after carriage return in reply: got 0x%x";

	nread = 0;
	while ((status = read_buf(
			rp->client_fd, (unsigned char *)reply + nread,
			sizeof reply - nread, rp->read_timeout))
		  > 0) {
		nread += status;
		if (nread < 2)
			continue;
		/*
		 *  Third character must be either newline or carriage return.
		 */
		switch (reply[2]) {
		/*
		 *  Looking for
		 *	ok\n
		 *	no\n
		 */
		case '\n':
			if (nread > 3) {
				error4(rp->verb, rp->algorithm,
						rp->netflow, bad_nl);
				return (char *)0;
			}
			break;
		/*
		 *  Looking for
		 *	ok\r\n
		 *	no\r\n
		 */
		case '\r':
			if (nread < 4)
				continue;
			if (reply[3] != '\n') {
				char buf[MSG_SIZE];

				snprintf(buf, sizeof buf, bad_crnl, reply[3]);
				error4(rp->verb, rp->algorithm,
							rp->netflow, buf);
				return (char *)0;
			}
		}
		goto got_reply;
	}
	if (nread < 3) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "read_reply() short read: %d bytes",
				nread);
		error4(rp->verb, rp->algorithm, rp->netflow, buf);
		return (char *)0;
	}
got_reply:
	reply[2] = 0;
	if (strcmp(reply, "ok") == 0) {
		add_chat_history(rp, "ok");		/* Chat history */
		return "ok";
	}
	if (strcmp(reply, "no") == 0) {
		add_chat_history(rp, "no");		/* Chat history */
		return "no";
	}
	/*
	 *  What the heh.  Client sent us junk.
	 */
	if (isprint(reply[0]) && isprint(reply[1])) {
		char buf[MSG_SIZE];

		reply[2] = 0;
		snprintf(buf, sizeof buf, "%s: %s: %s",
				rp->verb, rp->algorithm, rp->netflow);
		error3(buf, "unexpected reply: first 2 chars", reply);
	}
	else {
		char buf[MSG_SIZE];

		/*
		 *  Unprintable junk, to boot.
		 */
		snprintf(buf, sizeof buf, "unexpected reply: 0x%c%c 0x%c%c",
					hexchar[(reply[0] >> 4) & 0x0F],
					hexchar[(reply[0]) & 0x0F],
					hexchar[(reply[1] >> 4) & 0x0F],
					hexchar[(reply[1]) & 0x0F]);
		error4(rp->verb, rp->algorithm, rp->netflow, buf);
	}
	return (char *)0;		/* Error */
}

int
io_open(char *path, int flags, int mode)
{
	int fd;
again:
	fd = open(path, flags, mode);
	if (fd >= 0)
		return fd;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_open_append(char *path, int truncate)
{
	int flags = O_WRONLY | O_CREAT | O_APPEND;

	if (truncate)
		flags |= O_TRUNC;
	return io_open(path, flags, S_IRUSR|S_IWUSR|S_IRGRP);
}

/*
 *  Interuptable read() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
io_read(int fd, void *buf, size_t count)
{
	ssize_t nread;
again:
	nread = read(fd, buf, count);
	if (nread >= 0)
		return nread;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return (size_t)-1;
}

/*
 *  Interuptable write() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
io_write(int fd, void *buf, size_t count)
{
	ssize_t nwritten;
again:
	nwritten = write(fd, buf, count);
	if (nwritten >= 0)
		return nwritten;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return (size_t)-1;
}

int
io_pipe(int fds[2])
{
again:
	if (pipe(fds) == 0)
		return 0;

	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

/*
 *  Interuptable close() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
int
io_close(int fd)
{
again:
	if (close(fd) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_stat(const char *path, struct stat *st)
{
again:
	if (stat(path, st)) {
		if (errno == EINTR || errno == EAGAIN)
			goto again;
		return -1;
	}
	return 0;
}

int
is_file(char *path)
{
	struct stat st;

again:
	if (stat(path, &st)) {
		char buf[MSG_SIZE];

		if (errno == ENOENT)
			return 0;
		if (errno == EINTR || errno == EAGAIN)
			goto again;
		snprintf(buf, sizeof buf, "is_file: stat(%s) failed: %s",
						path, strerror(errno));
		error(buf);
		return -1;
	}
	return 1;
}

/*
 *  Read the entire contents of a text file into memory and
 *  null terminate the buf.  Return 0 on success, otherwise error.
 */
int
slurp_text_file(char *path, char *buf, int buf_size)
{
	char err[MSG_SIZE];
	static char nm[] = "read_text_file";
	int fd;
	int nread, ntotal;

	switch (is_file(path)) {
	case -1:
		error3(nm, "is_file() failed", path);
		return -1;
	case 0:
		error3(nm, "not a file", path);
		return -1;
	}
	if ((fd = open(path, O_RDONLY, 0)) < 0) {
		snprintf(err, sizeof err, "open(%s) failed: %s",
							path, strerror(errno));
		error2(nm, err);
		return -1;
	}
	/*
	 *  Slurp up the file
	 */
	nread = ntotal = 0;
	while ((nread = read_buf(fd, (unsigned char *)buf + ntotal,
			buf_size - ntotal, SLURP_TEXT_TIMEOUT) > 0))
		ntotal += nread;
	if (nread < 0)
		error3(nm, "read_buf() failed", path);
	if (nread > 0)
		error3(nm, "file too big for buffer", path);
	if (close(fd))
		error4(nm, path, "close() failed", strerror(errno));
	if (nread == 0) {
		buf[ntotal] = 0;
		return 0;
	}
	return -1;
}

/*
 *  Replace the contents of a file with a text string.
 */
int
burp_text_file(char *buf, char *path)
{
	int fd;
	char err[MSG_SIZE];
	static char nm[] = "burp_text_file";
	int status = 0;

	if ((fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR)) < 0) {
		snprintf(err, sizeof err,"open(%s, CREAT|TRUNC, RW) failed: %s",
						path, strerror(errno));
		error2(nm, err);
		return -1;
	}
	if (write_buf(fd, (unsigned char *)buf,strlen(buf),BURP_TEXT_TIMEOUT)) {
		error3(nm, "write_buf() failed", path);
		status = -1;
	}
	if (close(fd)) {
		error4(nm, path, "close() failed", strerror(errno));
		status = -1;
	}
	return status;
}

/*
 *  Convert 32 bit internet address to dotted text.
 *
 *  Derived from traceport.c written by Andy Fullford (akfull@august.com).
 */
char *
addr2text(u_long addr)
{
#define SHOW_IP_BUFS	4
#define SHOW_IP_BUFSIZE	20

	char *cp, *buf;
	u_int byte;
	int n;
	static char bufs[SHOW_IP_BUFS][SHOW_IP_BUFSIZE];
	static int curbuf = 0;

	buf = bufs[curbuf++];
	if (curbuf >= SHOW_IP_BUFS)
		curbuf = 0;

	cp = buf + SHOW_IP_BUFSIZE;
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	cp++;
	return cp;
}

int
io_select(int nfds, fd_set *readfds, fd_set *writefds,
         fd_set *errorfds, struct timeval *timeout)
{
	int status;

again:
	status = select(nfds, readfds, writefds, errorfds, timeout);
	if (status >= 0)
		return status;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

pid_t
io_waitpid(pid_t pid, int *stat_loc, int options)
{
	pid_t id;

again:
	id = waitpid(pid, stat_loc, options);
	if (id >= 0)
		return id;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_rename(char *old_path, char *new_path)
{
again:
	if (rename(old_path, new_path) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

struct dirent *
io_readdir(DIR *dirp)
{
	struct dirent *dp;

again:
	if ((dp = readdir(dirp)))
		return dp;
	if (errno == EINTR || errno == EAGAIN) {
		
		errno = 0;
		goto again;
	}
	return (struct dirent *)0;
}

DIR *io_opendir(char *path)
{
	DIR *dp;

again:
	dp = opendir(path);
	if (dp)
		return dp;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return (DIR *)0;
}

int
io_unlink(const char *path)
{
again:
	if (unlink(path) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_rmdir(const char *path)
{
again:
	if (rmdir(path) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_mkfifo(char *path, mode_t mode)
{
	int fd;

again:
	fd = mkfifo(path, mode);
	if (fd >= 0)
		return fd;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_chmod(char *path, mode_t mode)
{
again:
	if (chmod(path, mode) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

off_t
io_lseek(int fd, off_t offset, int whence)
{
	off_t o;

again:
	o = lseek(fd, offset, whence);
	if (o > -1)
		return o;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return (off_t)-1;
}

int
io_fstat(int fd, struct stat *buf)
{
again:
	if (fstat(fd, buf) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
io_closedir(DIR *dp)
{
again:
	if (closedir(dp) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

void
io_msg_new(struct io_message *ip, int fd)
{
	ip->fd = fd;
	ip->len = 0;
}

/*
 *  Read a message from a byte stream.  Messages in the stream are introduced
 *  with a leading length byte followed by the payload.
 *
 *  Returns:
 *	-1	errno indicates
 *	0	end of file (not end of message)
 *	1	message read
 *
 *  Note:
 *	This is the inefficent but correct two read() version.
 *	This algorithm will be replaced with a buffered version
 *	more appropriate to the fast logger processes.
 */
int
io_msg_read(struct io_message *ip)
{
	int nread, fd;
	unsigned char len, *p;

	if (ip == (struct io_message *)0 || ip->fd < 0) {
		errno = EINVAL;
		return -1;
	}

	fd = ip->fd;
	ip->len = 0;

	/*
	 *  Read the single byte payload length.
	 */
	nread = io_read(fd, &len, 1);
	if (nread <= 0)
		return nread;
	if (len == 0) {			/* no zero length messages */
#ifdef ENODATA
		errno = ENODATA;
#else
		errno = EBADMSG;
#endif
		return -1;
	}

	/*
	 *  Multiple blocking reads to slurp up the payload.
	 */
	p = ip->payload;
	while (ip->len < len) {
		nread = io_read(fd, p, len - ip->len);
		if (nread < -1)
			return -1;
		/*
		 *  End of file before full payload slurped.
		 */
		if (nread == 0) {
			errno = EBADMSG;
			return -1;
		}
		p += nread;
		ip->len += nread;
	}
	return 1;
}

/*
 *  Write an entire payload as a message.  Payload must be <= MSG_SIZE.
 *
 *  Returns:
 *  	0	message written ok
 *  	-1	error occured, errno set to error
 *  Note:
 *	Replace buffered version with writev()!
 */
ssize_t
io_msg_write(int fd, void *payload, unsigned char count)
{
	ssize_t nwritten, nwrite;
	unsigned char buf[MSG_SIZE + 1];	// payload plus len byte

	if (fd < 0 || payload == (void *)0 || count == 0) {
		errno = EINVAL;
		return -1;
	}
	buf[0] = count;
	nwrite = count + 1;
	memcpy(buf + 1, payload, count);
	nwritten = io_write(fd, buf, nwrite);
	if (nwritten == nwrite)
		return 0;
	if (nwritten == -1)
		return (size_t)-1;
	errno = EBADMSG;			// partially written message
	return (size_t)-1;
}

int
io_mkdir(const char *pathname, mode_t mode)
{
again:
	if (mkdir(pathname, mode) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}
