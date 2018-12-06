/*
 *  Synopsis:
 *	A read only, trusting blob service that optimizes by linking to files.
 *  Note:
 *	Only ascii is acccepted in the file system path!
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "blobio.h"

#ifdef COMPILE_TRACE

#define _TRACE(msg)		if (tracing) _trace(msg)
#define _TRACE2(msg1,msg2)	if (tracing) _trace2(msg1,msg2)

#else

#define _TRACE(msg)
#define _TRACE2(msg1,msg2)

#endif

extern char	*verb;
extern char	algorithm[9];
extern char	digest[129];
extern char	end_point[129];
extern char	*output_path;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;
extern char	*null_device;
extern char	*brr_path;

//  Note: rename as fs_path
static char	fs_path[PATH_MAX + 1];

struct service rofs_service;

static void
_trace(char *msg)
{
	trace2("rofs", msg);
}

static void
_trace2(char *msg1, char *msg2)
{
	trace3("rofs", msg1, msg2);
}

/*
 *  The end point is the root directory of the source blobio file system.
 *  No non-ascii or '?' character are allowed in root file path.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
rofs_end_point_syntax(char *root_dir)
{
	char *p = root_dir, c;

	_TRACE("request to end_point_syntax()");

	while ((c = *p++)) {
		if (!isascii(c))
			return "character in root directory path is not ascii";
		if (c == '?')
			return "? character not allowed in root directory path";
	}
	/*
	 *  root directory can't be too long.  Need room for
	 *  trailing $BLOBIO_ROOT/data/<algorithm>_fs/path/to/blob
	 *
	 *  Note:
	 *	Seems like this code ought to be in the digest module.
	 */
	if (p - root_dir >= (PATH_MAX - 59))
		return "too many chars in root directory path";

	_TRACE("end_point_syntax() done");

	return (char *)0;
}

/*
 *  Verify the root directory of the blob file system.
 *  Directory must be readable.
 */
static char *
rofs_open()
{
	struct stat st;

	_TRACE("request to open()");

	if (brr_path)
		return "option --brr-path not supported";

	_TRACE2("blob root directory", end_point);

	if (*verb == 'w')
		return "wrap verb not supported";

	//  verify permissons

	if (uni_access(end_point, X_OK)) {
		if (errno == ENOENT)
			return "blob root directory does not exist";
		if (errno == EPERM)
			return "no permission to search blob root directory";
		return strerror(errno);
	}
			
	//  verify the end point is, in fact, a directory

	if (stat(end_point, &st))
		return strerror(errno);
	if (!S_ISDIR(st.st_mode))
		return "root blob directory is not a directory";

	//  assemble the path to the data directory

	fs_path[0] = 0;
	buf4cat(fs_path, sizeof fs_path, end_point, "/data/", algorithm, "_fs");

	//  verify existence and permission of path to data directory.
	//  path looks like <root_dir>/data/<algorithm>_fs

	_TRACE2("blob data directory", fs_path);

	//  reverify permissons on data/ directory

	if (uni_access(fs_path, X_OK)) {
		if (errno == ENOENT)
			return "blob data directory does not exist";
		if (errno == EPERM)
			return "no permission to search blob data directory";
		return strerror(errno);
	}
			
	//  verify the data directory is, in fact, a directory

	if (stat(fs_path, &st))
		return strerror(errno);
	if (!S_ISDIR(st.st_mode))
		return "blob data directory is not a directory";

	_TRACE("open() done");

	return (char *)0;
}

static char *
rofs_open_output()
{
	_TRACE("request to open_output()");
	_TRACE("open_output() done");

	//  rofs requires linking
	if (output_path == null_device)
		return "null device not supported for output";
	return (char *)0;
}

static char *
rofs_close()
{
	_TRACE("request to close()");

	_TRACE("close() done");

	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  If output path exists then the output path to the blob path;
 *  otherwise copy the file to the output
 */
static char *
rofs_get(int *ok_no)
{
	char *err;
	int len, blob_fd;
	unsigned char buf[PIPE_MAX];
	ssize_t nread;

	_TRACE("request to get()");

	//  build path to source blob file

	bufcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = rofs_service.digest->fs_path( fs_path + len, PATH_MAX - len);
	if (err)
		return err;

	_TRACE2("blob file path", fs_path);

	//  link output path to source blob file
	
	if (output_path) {
		_TRACE("hard link to source blob file");

		if (uni_link(fs_path, output_path)) {
			if (errno == ENOENT) {
				*ok_no = 1;
				return (char *)0;
			}
			//  cross-link or link not permitted
			if (errno != EXDEV && errno != EPERM)
				return strerror(errno);
		} else {
			*ok_no = 0;
			return (char *)0;
		}
	}

	//  no output path so copy blob to output file
	
	_TRACE("copying blob to output stream");

	//  Note:  think about linux splice()

	if ((blob_fd = uni_open(fs_path, O_RDONLY)) < 0) {

		//  blob does not exist

		if (errno == ENOENT) {
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}

	//  Note: stat size to compare empty file to well known empty digest.

	while ((nread = uni_read(blob_fd, buf, sizeof buf)) > 0)
		if (uni_write_buf(output_fd, buf, nread))
			return strerror(errno);
	if (nread < 0)
		return strerror(errno);
	if (uni_close(blob_fd))
		return strerror(blob_fd);

	_TRACE("get() done");
	return (char *)0;
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
rofs_eat(int *ok_no)
{
	char *err;
	int len;

	_TRACE("request to eat()");

	bufcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = rofs_service.digest->fs_path(fs_path + len, PATH_MAX - len);
	if (err)
		return err;

	_TRACE2("blob file path", fs_path);

	//  insure we can read the file.

	if (uni_access(fs_path, R_OK)) {
		if (errno == ENOENT) {
			_TRACE("blob does not exists");
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}

	*ok_no = 0;

#ifdef COMPILE_TRACE
	if (tracing) {
		TRACE("blob is readable");
		TRACE("eat() done");
	}
#endif

	return (char *)0;


	return (char *)0;
}

static char *
rofs_put(int *ok_no)
{
	(void)ok_no;
	return "\"put\" not in read only file system service";
}

static char *
rofs_take(int *ok_no)
{
	(void)ok_no;
	return "\"take\" not in read only file system service";
}

static char *
rofs_give(int *ok_no)
{
	(void)ok_no;
	return "\"give\" not in read only file system service";
}

static char *
rofs_roll(int *ok_no)
{
	(void)ok_no;
	return "\"roll\" not in read only file system service";
}

static char *
rofs_wrap(int *ok_no)
{
	(void)ok_no;
	return "\"wrap\" not in read only file system service";
}

struct service rofs_service =
{
	.name			=	"rofs",
	.end_point_syntax	=	rofs_end_point_syntax,
	.open			=	rofs_open,
	.close			=	rofs_close,
	.get			=	rofs_get,
	.open_output		=	rofs_open_output,
	.eat			=	rofs_eat,
	.put			=	rofs_put,
	.take			=	rofs_take,
	.give			=	rofs_give,
	.roll			=	rofs_roll,
	.wrap			=	rofs_wrap
};
