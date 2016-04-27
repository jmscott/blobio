/*
 *  Synopsis:
 *	A driver for a trusting posix file system blobio service.
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

static char	fs_path[PATH_MAX + 1];

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
 *  Verify the root directory of the blob file system.
 *  Both root directory and the data directory must be searchable.
 *
 *  Note:
 *	Too many stats()!  Why stat() both root/ and data/ directories? 
 *	Can we do a single stat()?
 */
static char *
fs_open()
{
	struct stat st;

	_TRACE("request to open()");

	_TRACE2("blob root directory", end_point);

	//  verify permissons to blobio root directory

	if (access(end_point, X_OK)) {
		if (errno == ENOENT)
			return "blob root directory does not exist";
		if (errno == EPERM)
			return "no permission to search blob root directory";
		return strerror(errno);
	}
			
	//  verify the end point (root directory) is, in fact, a directory.

	if (stat(end_point, &st))
		return strerror(errno);
	if (!S_ISDIR(st.st_mode))
		return "root blob directory is not a directory";

	//  assemble the path to the data directory

	fs_path[0] = 0;
	bufcat(fs_path, sizeof fs_path, end_point);
	bufcat(fs_path, sizeof fs_path, "/data/");
	bufcat(fs_path, sizeof fs_path, algorithm);
	bufcat(fs_path, sizeof fs_path, "_fs");

	//  verify existence and permission of path to data directory.
	//  path looks like <root_dir>/data/<algorithm>_fs

	_TRACE2("blob data/ directory", fs_path);
	if (access(fs_path, X_OK)) {
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
		out = uni_open(out_path, O_WRONLY);
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

	bufcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = fs_service.digest->fs_path(
				fs_path + len,
				PATH_MAX - len
	);
	if (err)
		return err;

	_TRACE2("blob file path", fs_path);

	//  link output path to source blob file
	
	if (output_path) {

		_TRACE("attempt hard link to source blob file");

		if (link(fs_path, output_path) == 0) {
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

	if (access(fs_path, R_OK)) {
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

	*ok_no = 1;

	//  make the full directory path

	err = fs_service.digest->fs_mkdir(fs_path);
	if (err)
		return err;

	len = strlen(fs_path);
	err = fs_service.digest->fs_path(fs_path + len, PATH_MAX - len);
	if (err)
		return err;
	_TRACE2("path to blob", fs_path);

	//  try to link the target blob to the source blob

	if (input_path) {
		if (link(input_path, fs_path) == 0 || errno == EEXIST) {
			*ok_no = 0;
			return (char *)0;
		}
		if (errno != EPERM)
			return strerror(errno);
	}

	//  link failed so attempt to copy the blob

	err = fs_copy(input_path, fs_path);
	if (err && errno != EEXIST)
		return err;
	
	*ok_no = 0;
	return (char *)0;
}

static char *
fs_take(int *ok_no)
{
	(void)ok_no;
	return "\"take\" not implemented (yet) in \"fs\" service";
}

static char *
fs_give(int *ok_no)
{
	(void)ok_no;
	return "\"give\" not implemented (yet) in \"fs\" service";
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
