/*
 *  Synopsis:
 *	A driver for a trusting posix file system blobio service.
 *  Note:
 *  	Long (>= PATH_MAX) file paths are still problematic.
 *
 *	The input stream is not drained when the blob exists during a put/give.
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

#define _TRACE(msg)		0
#define _TRACE2(msg1,msg2)	0

#endif

extern char	*verb;
extern char	algorithm[9];
extern char	digest[129];
extern char	end_point[129];
extern char	*output_path;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;

static char	fs_path[PATH_MAX + 1];
static char	tmp_path[PATH_MAX + 1];

struct service fs_service;

static void
_trace(char *msg)
{
	trace2("fs", msg);
}

static void
_trace2(char *msg1, char *msg2)
{
	trace3("fs", msg1, msg2);
}

/*
 *  The end point is the root directory of the source blobio file system.
 *  No non-ascii or '?' characters are allowed in root file path.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
fs_end_point_syntax(char *root_dir)
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
	 *  trailing $BLOBIO_ROOT/data/<algorithm>_fs/<path/to/blob>.
	 *
	 *  Currently only sha1 is understood.
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
 *  Verify that the root and data directories of the blob file system
 *  are at least searchable.
 */
static char *
fs_open()
{
	struct stat st;

	_TRACE("request to open()");

	_TRACE2("blob root directory", end_point);

	//  build the path to the $BLOBIO_ROOT/data/<algo>_fs/ directory

	fs_path[0] = 0;
	buf4cat(fs_path, sizeof fs_path, end_point, "/data/", algorithm, "_fs");

	//  verify existence and permission of path to data directory.
	//  path looks like <root_dir>/data/<algorithm>_fs

	_TRACE2("data directory", fs_path);
	if (uni_access(fs_path, X_OK)) {
		if (errno == ENOENT)
			return "missing entry in blob data directory";
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

//   Note:  assume that service-open() has been called.

//  verify that the temp directory exists in the target root directory.

static char *
fs_open_output()
{
	return (char *)0;
}

static char *
fs_close()
{
	_TRACE("request to close()");

	_TRACE("close() done");

	return (char *)0;
}

static char *
fs_copy(char *in_path, char *out_path)
{
	int in = -1, out = -1;
	int nr;
	int e;
	unsigned char buf[PIPE_MAX];

	//  open input path or point to standard input

	if (in_path) {
		in = uni_open(in_path, O_RDONLY);
		if (in < 0)
			return strerror(errno);
	} else
		in = input_fd;

	//  open output path or point to standard out

	if (out_path) {
		out = uni_open_mode(
				out_path,
				O_WRONLY | O_CREAT, 
				S_IRUSR | S_IRGRP
		);
		if (out < 0)
			return strerror(errno);
	} else
		out = output_fd;

	while ((nr = uni_read(in, buf, sizeof buf)) > 0)
		if (uni_write_buf(out, buf, nr) < 0)
			break;
	e = errno;
	if (in != -1)
		uni_close(in);
	if (out != -1)
		uni_close(out);
	errno = e;
	if (e != 0)
		return strerror(e);
	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  If output path exists then link the output path to the blob path.
 *  Copy the input to the output if the blobs are on different devices.
 */
static char *
fs_get(int *ok_no)
{
	char *err;
	int len;

	*ok_no = 1;

	//  build path to source blob file

	len = strlen(fs_path);
	err = fs_service.digest->fs_path(
				fs_path + len,
				PATH_MAX - len
	);
	if (err)
		return err;

	_TRACE2("source blob file path", fs_path);

	//  link output path to source blob file
	
	if (output_path) {

		_TRACE("attempt hard link target to source blob file");

		if (uni_link(fs_path, output_path) == 0) {
			_TRACE("hard link to target succeeded");
			*ok_no = 0;
			return (char *)0;
		}

		//  blob file does not exist

		if (errno == ENOENT)
			return (char *)0;

		//  error is NOT link() across file systems

		if (errno != EXDEV)
			return strerror(errno);

		_TRACE("source blob on different file system, so copy");

	}
	if ((err = fs_copy(fs_path, output_path))) {
		if (errno == ENOENT)
			return (char *)0;
		return err;
	}
	*ok_no = 0;
	return (char *)0;
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
fs_eat(int *ok_no)
{
	char *err;
	int len;

	_TRACE("request to eat()");

	bufcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = fs_service.digest->fs_path(
				fs_path + len,
				PATH_MAX - len
	);
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
}

static char *
fs_put(int *ok_no)
{
	char *err;
	int len;

	//  3 + 1 + 8 + 1 + 21 + 1 + 21 + 1
	//char tmp_id[57];	//  tmp/<verb>-<epoch>.<pid>

	*ok_no = 1;

	buf2cat(tmp_path, sizeof tmp_path, fs_path, "/tmp");

	//  make the full directory path

	err = fs_service.digest->fs_mkdir(fs_path);
	if (err)
		return err;

	//  append the path to the blob

	len = strlen(fs_path);
	err = fs_service.digest->fs_path(fs_path + len, PATH_MAX - len);
	if (err)
		return err;
	_TRACE2("path to target blob", fs_path);

	//  if the blob file exists then we are done

	if (uni_access(fs_path, F_OK) == 0) {
		*ok_no = 0;

		//  Note: what about draining the input not bound to a file?
		return (char *)0;
	}
	if (errno != ENOENT)
		return strerror(errno);

	//  try to link the target blob to the source blob

	if (input_path) {
		_TRACE2("hard link source blob to target", input_path);
		if (uni_link(input_path, fs_path) == 0 || errno == EEXIST) {
			*ok_no = 0;
			return (char *)0;
		}

		if (errno != EPERM)
			return strerror(errno);
		_TRACE("no permisson to hard link to target, so copy");
	} else
		_TRACE("no input path, so copy from standard input");

	buf2cat(tmp_path, sizeof tmp_path, fs_path, "/tmp");

	err = fs_copy(input_path, fs_path);
	if (err && errno != EEXIST)
		return err;
	
	*ok_no = 0;
	return (char *)0;
}

static char *
fs_take(int *ok_no)
{
	char *err = (char *)0;

	err = fs_get(ok_no);
	if (err || *ok_no == 1)
		return err;
	_TRACE2("unlinking source blob", fs_path);
	if (uni_unlink(fs_path) != 0 && errno != ENOENT)
		return strerror(errno);
	return (char *)0;
}

static char *
fs_give(int *ok_no)
{
	return fs_put(ok_no);
}

static char *
fs_roll(int *ok_no)
{
	(void)ok_no;
	return "\"roll\" not implemented (yet) in \"fs\" service";
}

static char *
fs_wrap(int *ok_no)
{
	(void)ok_no;
	return "\"wrap\" not implemented (yet) in \"fs\" service";
}

struct service fs_service =
{
	.name			=	"fs",
	.end_point_syntax	=	fs_end_point_syntax,
	.open			=	fs_open,
	.close			=	fs_close,
	.get			=	fs_get,
	.open_output		=	fs_open_output,
	.eat			=	fs_eat,
	.put			=	fs_put,
	.take			=	fs_take,
	.give			=	fs_give,
	.roll			=	fs_roll,
	.wrap			=	fs_wrap
};
