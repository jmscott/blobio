/*
 *  Synopsis:
 *	A driver for a trusting posix file system blobio service.
 *  Note:
 *  	Long (>= PATH_MAX) file paths are still problematic.
 *
 *	The input stream is not drained when the blob exists during a put/give.
 *
 *	A typo in the BLOBIO_SERVICE yields a "blob not found error", which
 *	is confusing.  Instead, the existence of the data directory ought to
 *	tested for a more clear error message.  An earlier version of fs did
 *	just this test.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#include "blobio.h"

//  does the verb imply a possible write to the file system?

#define IS_WRITE_VERB()	(*verb == 'p' || (*verb == 'g' && *verb == 'i'))

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
	return (char *)0;
}

/*
 *  Verify that the root and data directories of the blob file system
 *  are at least searchable.
 */
static char *
fs_open()
{
	//  build the path to the $BLOBIO_ROOT/data/<algo>_fs/ directory

	*fs_path = 0;
	buf4cat(fs_path, sizeof fs_path,
				end_point,
				"/data/",
				algorithm,
				"_fs"
	);

	//  if writing a blob then build path to temporary directory.

	if (IS_WRITE_VERB()) {
		*tmp_path = 0;
		buf2cat(tmp_path, sizeof tmp_path, fs_path, "/tmp");
	}

	return (char *)0;
}

//   Note:  assume that fs_open() has been called.

static char *
fs_open_output()
{
	return (char *)0;
}

static char *
fs_close()
{
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

	//  link output path to source blob file
	
	if (output_path) {

		if (uni_link(fs_path, output_path) == 0) {
			*ok_no = 0;
			return (char *)0;
		}

		//  blob file does not exist

		if (errno == ENOENT)
			return (char *)0;

		//  error is NOT link() across file systems

		if (errno != EXDEV)
			return strerror(errno);


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

	bufcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = fs_service.digest->fs_path(
				fs_path + len,
				PATH_MAX - len
	);
	if (err)
		return err;

	//  insure we can read the file.

	if (uni_access(fs_path, R_OK)) {
		if (errno == ENOENT) {
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}
	*ok_no = 0;

	return (char *)0;
}

static char *
fs_put(int *ok_no)
{
	char *err = (char *)0;
	char *np;
	int in_fd, tmp_fd;
	int nr;
	unsigned char buf[PIPE_MAX];

	//  Name of temporary file
	//  size: 1 + 8 + 1 + 21 + 1 + 21 + 1

	char tmp_name[57];	//  /<verb>-<epoch>.<pid>\0

	*ok_no = 1;

	//  make the full directory path to the blob

	err = fs_service.digest->fs_mkdir(fs_path, PATH_MAX - strlen(fs_path));
	if (err)
		return err;

	//  append /<blob-file-name> to the path to the blob

	np = bufcat(fs_path, sizeof fs_path, "/");
	err = fs_service.digest->fs_name(np, PATH_MAX - (np - fs_path));
	if (err)
		return err;

	//  if the blob file already exists then we are done

	if (uni_access(fs_path, F_OK) == 0) {
		*ok_no = 0;

		//  Note: what about draining the input not bound to a file?
		return (char *)0;
	}
	if (errno != ENOENT)
		return strerror(errno);

	//  try to hard link the target blob to the existing source blob

	if (input_path) {
		if (uni_link(input_path, fs_path) == 0 || errno == EEXIST) {
			*ok_no = 0;
			return (char *)0;
		}

		if (errno != EXDEV)
			return strerror(errno);
	}

	//  build path to temporary file which will accumulate the blob.

	snprintf(tmp_name, sizeof tmp_name, "/%s.%ul", verb, getpid());
	bufcat(tmp_path, sizeof tmp_path, tmp_name);

	//  open input file.  may be standard input

	if (input_path) {
		in_fd = uni_open(input_path, O_RDONLY);
		if (in_fd < 0)
			return strerror(errno);
	} else
		in_fd = input_fd;

	//  open tempory file to for incoming blob

	tmp_fd = uni_open_mode(
			tmp_path,
			O_WRONLY | O_CREAT, 
			S_IRUSR | S_IRGRP
	);
	if (tmp_fd < 0)
		return strerror(errno);

	//  copy the blob to temporary path.

	while ((nr = uni_read(in_fd, buf, sizeof buf)) > 0)
		if (uni_write_buf(tmp_fd, buf, nr) < 0) {
			err = strerror(errno);
			break;
		}
	if (nr < 0) {
		err = strerror(errno);
	}

	//  copy successfull so link temp and final blob.

	if (err == (char *)0) {

		if (uni_link(tmp_path, fs_path) && errno != EEXIST) {
			err = strerror(errno);
		} else
			*ok_no = 0;
	}

	uni_close(tmp_fd);
	uni_unlink(tmp_path);
	return err;
}

static char *
fs_take(int *ok_no)
{
	char *err = (char *)0;

	err = fs_get(ok_no);
	if (err || *ok_no == 1)
		return err;
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
