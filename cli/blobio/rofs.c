/*
 *  Synopsis:
 *	A read only, trusting blob service that optimizes by linking to files.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "blobio.h"

#ifdef COMPILE_TRACE

#define _TRACE(msg)		if (tracing) _trace(msg)

#else

#define _TRACE(msg)

#endif

extern char	*verb;
extern char	algorithm[9];
extern char	digest[129];
extern char	end_point[129];
extern char	*output_path;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;

struct service rofs_service;

static void
_trace(char *msg)
{
	trace2("rofs", msg);
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
 *  Directory must be readable.
 */
static char *
rofs_open()
{
	struct stat st;
	char data_path[PATH_MAX + 1];

	_TRACE("request to open()");

	//  verify root blob directory exists and is statable.

	if (stat(end_point, &st)) {
		if (errno == ENOENT)
			return "root blob directory does not exist";
		if (errno == EPERM)
			return "no permission to access root blob directory";
		return strerror(errno);
	}

	if ((st.st_mode & S_IFDIR) == 0)
		return "root blob directory is not a directory";

	//  verify existence and permission of path to data directory.
	//  path looks like <root_dir>/data/<algorithm>_fs

	data_path[0] = 0;
	bufcat(data_path, sizeof data_path, end_point);
	bufcat(data_path, sizeof data_path, "/data/");
	bufcat(data_path, sizeof data_path, algorithm);
	bufcat(data_path, sizeof data_path, "_fs");

	if (stat(data_path, &st)) {
		if (errno == ENOENT)
			return "blob data directory does not exist";
		if (errno == EPERM)
			return "no permission to blob data directory";
		return strerror(errno);
	}

	if ((st.st_mode & S_IFDIR) == 0)
		return "blob data directory is not a directory";

	_TRACE("open() done");

	return (char *)0;
}

static char *
rofs_open_input()
{
	_TRACE("request to open_input()");

	_TRACE("open_input() done");

	return (char *)0;
}

static char *
rofs_open_output()
{
	_TRACE("request to open_output()");

	_TRACE("open_output() done");

	return (char *)0;
}

static char *
rofs_close()
{
	return (char *)0;
}

/*
 *  Link a
 */
static char *
rofs_get(int *ok_no)
{
	(void)ok_no;
	_TRACE("request to get()");
	_TRACE("get() done");
	return (char *)0;
}

static char *
rofs_eat(int *ok_no)
{
	(void)ok_no;

	_TRACE("request to eat()");

	_TRACE("eat() done");

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
	.open_input		=	rofs_open_input,
	.eat			=	rofs_eat,
	.put			=	rofs_put,
	.take			=	rofs_take,
	.give			=	rofs_give,
	.roll			=	rofs_roll,
	.wrap			=	rofs_wrap
};
