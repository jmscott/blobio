/*
 *  Synopsis:
 *	Low level, network stream i/o with explicit timeouts.
 *  Note:
 *	Consider a 'net' data structure.
 *
 *	Explict, global timeouts are used instead of setsockopt(SO_RCVTIMEO).
 *	Not clear to me (jmscott) if setsockopt(SO_RCVTIMEO) prevents
 *	multiple packets from accumulating in the read() buffer.
 */
#include <sys/time.h>

#include <signal.h>
#include <unistd.h>
#include "biod.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

/*
 *  Explicity catch an alarm.
 */
static int	alarm_caught = 0;

static void
catch_SIGALRM(int sig)
{
	(void)sig;

	if (alarm_caught)
		panic("catch_SIGALRM() called with out reseting flag");
	alarm_caught = 1;
}

/*
 *  Synopsis:
 *	Accept incoming socket.
 *  Returns:
 *	0	new socket accepted
 *	1	timed out the request
 *	-1	accept() error, see errno.
 *  Notes:
 *	Unfortunatley, only the accept() error code is returned to the caller.
 *	The sigaction()/settime() failures cause a panic.
 *
 *	client_fd only changes on success.
 */
int
net_accept(int listen_fd, struct sockaddr *addr, socklen_t *len,
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
	 *  Disable timer.
	 *
	 *  Note:
	 *	Does setitimer(t.it_interval.tv_sec == 0) above imply
	 *	timer never refires?
	 */
	t.it_value.tv_sec = 0;
	t.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic2("io_accept: settimer(REAL, 0) failed", strerror(errno));
	if (fd > -1) {
		*client_fd = fd;
		return 0;
	}

	/*
	 *  Retry or timeout.
	 */
	if (e == EINTR) {
		/*
		 *  Timeout.
		 *
		 *  We can only catch the alarm once,
		 *  then process must exit.
		 */
		if (alarm_caught) {
			alarm_caught = 0;
			return 1;
		}
		goto again;
	}
	errno = e;
	return -1;
}

/*
 *  Do a timed read of some bytes on the network.
 *
 *	>= 0	=> read some bytes
 *	-1	=> read() error
 *	-2	=> timeout
 */
ssize_t
net_read(int fd, void *buf, size_t buf_size, unsigned timeout)
{
	static char n[] = "net_read";

	ssize_t nread;
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
		panic3(n, "sigaction(ALRM) failed", strerror(errno));
	/*
	 *  Set the interval time.
	 */
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic3(n, "setitimer(REAL) failed", strerror(errno));

	nread = read(fd, (void *)buf, buf_size);
	e = errno;

	/*
	 *  Disable timer.
	 *
	 *  Note:
	 *	Does setitimer(t.it_interval.tv_sec == 0) above imply
	 *	timer never refires?
	 */
	t.it_value.tv_sec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic3(n, "setitimer(REAL) reset failed", strerror(errno));

	if (nread < 0) {
		char tbuf[27];

		if (e != EINTR) {
			error3(n, "read() failed", strerror(e));
			errno = e;
			return -1;
		}
		if (!alarm_caught)
			goto again;

		alarm_caught = 0;
		snprintf(tbuf, sizeof tbuf, "timed out after %d secs", timeout);
		error3(n, "alarm caught", tbuf);
		return -2;
	}
	return nread;
}

/*
 *  Do a timed write() of an entire buffer.
 *  Returns
 *
 *	0	=> entire buffer written without error
 *	1	=> write() timed out
 *	-1	=> write() error
 */
int
net_write(int fd, void *buf, size_t buf_size, unsigned timeout)
{
	static char n[] = "net_write";

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
		panic3(n, "sigaction(ALRM) failed", strerror(errno));
	/*
	 *  Set the timer
	 */
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic3(n, "write_buf: setitimer(REAL) failed", strerror(errno));
	nwrite = write(fd, (void *)b, b_end - b);
	e = errno;

	/*
	 *  Disable timer.
	 *
	 *  Note:
	 *	Does setitimer(t.it_interval.tv_sec == 0) above imply
	 *	timer never refires?
	 */
	t.it_value.tv_sec = 0;
	t.it_value.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &t, (struct itimerval *)0))
		panic3(n, "setitimer(REAL, 0) failed", strerror(errno));

	if (nwrite < 0) {
		char tbuf[20];

		if (e != EINTR) {
			error3(n, "write() failed", strerror(errno));
			errno = e;
			return -1;
		}
		if (!alarm_caught)
			goto again;

		alarm_caught = 0;
		snprintf(tbuf, sizeof tbuf, "timed out after %d secs", timeout);
		error3(n, "alarm caught", tbuf);
		return 1;
	}
	b += nwrite;
	if (b < b_end)
		goto again;
	return 0;
}

/*
 *  Convert 32 bit internet address to dotted text.
 *
 *  Derived from traceport.c written by Andy Fullford (akfull@august.com).
 */
char *
net_32addr2text(u_long addr)
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
