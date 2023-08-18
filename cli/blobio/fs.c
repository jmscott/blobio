/*
 *  Synopsis:
 *	Driver for a fast, trusted posix file system blobio service.
 *  Note:
 *	Not UTF-8 safe.  Various char counts are in ascii bytes, not UTF8 chars.
 */

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "blobio.h"

#define DEFER {if (!err && errno > 0) err = strerror(errno); goto BYE;}

extern struct service fs_service;

static char fs_data[4 + 1 + 3 + 8 + 1];			// data/fs_<algo>
static int end_point_fd = -1;

/*
 *  Verify the syntax of the end point of the file system service.
 *  The endpoint is the root directory of the source blobio file system.
 *  The query args (following '?') have already been removed.
 *  The end point is assumed be comprised of ascii chars.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
fs_end_point_syntax(char *root_dir)
{
	if (strlen(root_dir) > BLOBIO_MAX_FS_PATH - 1)
		return "too many chars in root directory path";
	return (char *)0;
}

//  open a request to posix file system.
//  assume global var "algo" has been extracted by caller.

static char *
fs_open()
{
	char *end_point = fs_service.end_point;

	TRACE2("end point directory", end_point);

	//  verify endpoint is full path.

	//  Note: why care about full path end point?
	if (*end_point != '/')
		return "first char of end point is not '/'";

	end_point_fd = jmscott_open(end_point, O_DIRECTORY, 0777);
        if (end_point_fd < 0)
                return strerror(errno);

	// assemble "fs_data" using global algorithm

	char *err;
	fs_data[0] = 0;
	jmscott_strcat2(fs_data, sizeof fs_data, "data/fs_", algorithm);
	TRACE2("mkdir: fs data", fs_data);
	err = jmscott_mkdirat_path(end_point_fd, fs_data, 0777);
	if (err)
		return err;

	char v = *verb;

	if ((v == 'g' && verb[1] == 'e') || v == 't' || v == 'e') {
		TRACE2("read only verb", verb);
		return (char *)0;
	}

	char *p;
	if (v == 'w')
		p = "spool/wrap";
	else if (v == 'r')
		p = "spool/roll";
	else
		p = "spool";
	TRACE2("mkdir", p);

	return jmscott_mkdirat_path(end_point_fd, p, 0777);
}

static char *
fs_close()
{
	TRACE("entered");
	if (end_point_fd > -1) {
		int fd = end_point_fd;
		end_point_fd = -1;
		if (jmscott_close(fd))
			return strerror(errno);
	}
	return (char *)0;
}

static char *
fs_get(int *ok_no)
{
	TRACE("entered");

	char blob_path[BLOBIO_MAX_FS_PATH+1];
	blob_path[0] = 0;
	char *p = jmscott_strcat2(blob_path, sizeof blob_path, fs_data, "/");

	size_t len = sizeof blob_path - (p - blob_path);
	char *err = digest_module->fs_path(p, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	//  attempt to hard link blob to output path, for zero copy

	if (output_path && output_path != null_device) {
		int status = jmscott_linkat(
				end_point_fd,
				blob_path,
				AT_FDCWD,
				output_path,
				0
		);
		if (status == 0) {
			TRACE("hard linked blob to output path");
			*ok_no = 0;
			return (char *)0;
		}
		if (status < 0) {
			if (errno == ENOENT) {
				TRACE("blob does not exist");
				*ok_no = 1;
				return (char *)0;
			}
			if (errno != EXDEV)
				return strerror(errno);

			//  not on same device, so hard link failed, so copy
		}
		TRACE("blob not on same device as output path");
	}

	//  trusted copy of blob to output fd
	int blob_fd = jmscott_openat(end_point_fd, blob_path, O_RDONLY, 077);
	if (blob_fd < 0) {
		if (errno == ENOENT) {		//  hmm, blob disappeared (ok)
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}
	*ok_no = 0;

	if (output_path == null_device) {
		TRACE("output path is null device, so no read of blob");
		goto BYE;
	}

	if (output_path) {
		TRACE2("open output path exists", output_path)
		output_fd = jmscott_open(
				output_path,
				O_WRONLY | O_CREAT | O_EXCL,
				S_IRUSR | S_IRGRP
		);
		if (output_fd < 0)
			DEFER;
	}

	TRACE("sendfile ...");
	blob_size = 0;
	err = jmscott_send_file(blob_fd, output_fd, &blob_size);
BYE:
	if (blob_fd >= 0 && jmscott_close(blob_fd) && !err)
		err = strerror(errno);
	blob_fd = -1;

	if (output_path && output_fd >= 0 && jmscott_close(output_fd) && !err)
		err = strerror(errno);
	output_fd = -1;

	if (err)
		return err;
	return (char *)0;
}

static char *
fs_open_output()
{
	TRACE("entered");

	return (char *)0;
}

struct service fs_service =
{
	.name			=	"fs",
	.end_point_syntax	=	fs_end_point_syntax,
	.open			=	fs_open,
	.open_output		=	fs_open_output,
	.close			=	fs_close,
	.get			=	fs_get
};
