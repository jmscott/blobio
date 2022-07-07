/*
 *  Synopsis:
 *	Interuptible posix/unixish routines, with errno untouched.
 *  Note:
 *	Rename uni.c to posio.c
 *
 *	Eventually the uni.c goes away, replaced with jmscott_*() equivalents.
 *
 *	Investigate why syscall rename() requires stdio.h!
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "jmscott/libjmscott.h"

extern int	timeout;

/*
 *  Note:
 *	I (jms) refuse to pull in <stdio.h> for single declaration!
 *	WTF are "feature test macros", by the way?
 *
 *		http://man7.org/linux/man-pages/man7/feature_test_macros.7.html
 */
extern int rename(const char *oldpath, const char *newpath);

/*
 *  Interruptable close() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
int
uni_close(int fd)
{
	return jmscott_close(fd);
}

/*
 *  Interruptable write() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
uni_write(int fd, const void *buf, size_t count)
{
	return jmscott_write(fd, (void *)buf, count);
}

/*
 *  Write a full buffer or return error.
 *  Caller can depend on correct value of errno.
 *
 */
int
uni_write_buf(int fd, const void *buf, size_t count)
{
	return jmscott_write(fd, (void *)buf, count);
}

/*
 *  Make a directory path.
 */
int
uni_mkdir(const char *path, mode_t mode)
{
	return jmscott_mkdir(path, mode);
}

/*
 *  Interruptable read() of a file descriptor.
 *  Caller can depend on correct value of errno.
 */
ssize_t
uni_read(int fd, void *buf, size_t count)
{
	if (timeout > 0)
		return jmscott_read_timeout(fd, buf, count, timeout);
	return jmscott_read(fd, buf, count);
}

int
uni_open(const char *path, int flags)
{
	return jmscott_open((char *)path, flags, 0);
}

int
uni_open_mode(const char *path, int flags, int mode)
{
	return jmscott_open((char *)path, flags, mode);
}

int
uni_link(const char *old_path, const char *new_path)
{
	return jmscott_link(old_path, new_path);
}

int
uni_unlink(const char *path)
{
	return jmscott_unlink(path);
}

int
uni_access(const char *path, int mode)
{
	return jmscott_access(path, mode);
}
