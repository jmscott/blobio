/*
 *  Synopsis:
 *	Interuptible posix/unixish routines, with errno untouched.
 *  Note:
 *	Investigate why syscall rename() requires stdio.h!
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "jmscott/posio.c"

/*
 *  Note:
 *	I (jms) refuse to pull in <stdio.h> for single declaration!
 *	WTF are "feature test macros"?
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
