/*
 *  Synopsis:
 *	Wrapper around restartable posix/unixish routines, with errno untouched.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/*
 *  Interruptable close() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
int
uni_close(int fd)
{
again:
	errno = 0;
	if (close(fd) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

/*
 *  Interruptable write() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
uni_write(int fd, const void *buf, size_t count)
{
	ssize_t nwrite;

again:
	errno = 0;
	nwrite = write(fd, buf, count);
	if (nwrite > 0)
		return nwrite;

	//  write of zero only occurs on interupted system call?

	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

/*
 *  Write a full buffer or return error.
 *  Caller can depend on correct value of errno.
 *
 *  Note:
 *	Ought to be renamed uni_write_all().
 */
int
uni_write_buf(int fd, const void *buf, size_t count)
{
	size_t nwrite = 0;

	while (nwrite < count) {

		ssize_t nw;

		nw = uni_write(fd, buf + nwrite, count - nwrite);
		if (nw == -1)
			return -1;
		nwrite += nw;
	}
	return 0;
}

/*
 *  Interruptable read() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
uni_read(int fd, void *buf, size_t count)
{
	ssize_t nread;

again:
	errno = 0;
	nread = read(fd, buf, count);
	if (nread >= 0)
		return nread;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
uni_open(const char *path, int flags)
{
	int fd;

again:
	errno = 0;
	fd = open(path, flags);
	if (fd >= 0)
		return fd;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
uni_open_mode(const char *path, int flags, int mode)
{
	int fd;

again:
	errno = 0;
	fd = open(path, flags, mode);
	if (fd >= 0)
		return fd;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
uni_link(const char *oldpath, const char *newpath)
{
again:
	errno = 0;
	if (link(oldpath, newpath) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
uni_unlink(const char *path)
{
again:
	errno = 0;
	if (unlink(path) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}

int
uni_access(const char *path, int mode)
{
again:
	errno = 0;
	if (access(path, mode) == 0)
		return 0;
	if (errno == EINTR || errno == EAGAIN)
		goto again;
	return -1;
}
