/*
 *  Synopsis:
 *	Restartable interface to i/o unix system calls with errno honored.
 *  Note:
 *	Can the timeout code in functions read_buf() and write_buf()
 *	be moved to io_read() and io_write().  Currently timeouts apply
 *	only to network level read/write, which seems incomplete.
 *
 *	EAGAIN is not caught.  Historically EAGAIN meant that the process
 *	would have been suspended and not to "try again".  Unfortunatly
 *	certain NFS clients have used EAGAIN when, for example, mounting over
 *	the network.
 */
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>

#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "biod.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

int
io_open(char *path, int flags, int mode)
{
	int fd;
again:
	fd = open(path, flags, mode);
	if (fd >= 0)
		return fd;
	if (errno == EINTR)
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
	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return (size_t)-1;
}

/*
 *  Write exactly count bytes to stream.
 *  Return 0 on success, -1 on failure, with errno set.
 */
int
io_write_buf(int fd, void *buf, size_t count)
{
	ssize_t nw = 0;
	size_t nwritten = 0;
again:
	nw = io_write(fd, buf, count - nwritten);
	if (nw < 0)
		return -1;
	nwritten += nw;
	if (nwritten < count)
		goto again;
	return 0;
}

int
io_pipe(int fds[2])
{
again:
	if (pipe(fds) == 0)
		return 0;

	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return -1;
}

int
io_stat(const char *path, struct stat *st)
{
again:
	if (stat(path, st)) {
		if (errno == EINTR)
			goto again;
		return -1;
	}
	return 0;
}

int
io_lstat(const char *path, struct stat *st)
{
again:
	if (lstat(path, st)) {
		if (errno == EINTR)
			goto again;
		return -1;
	}
	return 0;
}

int
io_path_exists(char *path)
{
	struct stat st;

	if (io_stat(path, &st))
		return errno == ENOENT ? 0 : -1;
	return 1;
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
	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return -1;
}

int
io_rename(char *old_path, char *new_path)
{
again:
	if (rename(old_path, new_path) == 0)
		return 0;
	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return (struct dirent *)0;
}

DIR *io_opendir(char *path)
{
	DIR *dp;

again:
	dp = opendir(path);
	if (dp)
		return dp;
	if (errno == EINTR)
		goto again;
	return (DIR *)0;
}

int
io_unlink(const char *path)
{
again:
	if (unlink(path) == 0 || errno == ENOENT)
		return 0;
	if (errno == EINTR)
		goto again;
	return -1;
}

int
io_rmdir(const char *path)
{
again:
	if (rmdir(path) == 0)
		return 0;
	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return -1;
}

int
io_chmod(char *path, mode_t mode)
{
again:
	if (chmod(path, mode) == 0)
		return 0;
	if (errno == EINTR)
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
	if (errno == EINTR)
		goto again;
	return (off_t)-1;
}

int
io_fstat(int fd, struct stat *buf)
{
again:
	if (fstat(fd, buf) == 0)
		return 0;
	if (errno == EINTR)
		goto again;
	return -1;
}

int
io_closedir(DIR *dp)
{
again:
	if (closedir(dp) == 0)
		return 0;
	if (errno == EINTR)
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
 *  Read an atomically written message from a stable (no timeout) byte stream.
 *  Messages in the stream are introduced with a leading length byte followed
 *  by upto 255 payload bytes.
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
	if (errno == EINTR)
		goto again;
	return -1;
}

/*
 *  Read the entire contents of a text file into memory and
 *  null terminate the buf.  Return 0 on success, otherwise error.
 */
int
slurp_text_file(char *path, char *buf, size_t buf_size)
{
	char err[MSG_SIZE];
	static char n[] = "read_text_file";
	int fd;
	ssize_t nr, nread;

	switch (io_path_exists(path)) {
	case -1:
		error3(n, "io_is_file() failed", path);
		return -1;
	case 0:
		error3(n, "file does not exist", path);
		return -1;
	}
	if ((fd = io_open(path, O_RDONLY, 0)) < 0) {
		snprintf(err, sizeof err, "open(%s) failed: %s",
							path, strerror(errno));
		error2(n, err);
		return -1;
	}
	/*
	 *  Slurp to end of file.
	 */
	nread = 0;
	while ((nr = io_read(fd, buf + nread, buf_size - nread)) > 0) {
		nread += nr;
		if (buf_size - nread <= 0) {
			error3(n, "file too big for buffer", path);
			return -1;
		}
	}
	if (nr < 0)
		error3(n, "read_buf() failed", path);
	if (close(fd))
		error4(n, path, "close() failed", strerror(errno));
	if (nr == 0) {
		buf[nread] = 0;
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
	static char n[] = "burp_text_file";
	int status = 0;

	if ((fd = io_open(path, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR))<0) {
		snprintf(err, sizeof err,"open(%s, CREAT|TRUNC, RW) failed: %s",
						path, strerror(errno));
		error2(n, err);
		return -1;
	}

	if (io_write_buf(fd, (unsigned char *)buf, strlen(buf))) {
		error3(n, "write() failed", path);
		status = -1;
	}
	if (io_close(fd)) {
		error4(n, path, "close() failed", strerror(errno));
		status = -1;
	}
	return status;
}
