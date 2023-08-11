/*
 *  Synopsis:
 *	Driver for a fast, trusted posix file system blobio service.
 *  Note:
 *	Not UTF-8 safe.  Various char counts are in ascii bytes, not UTF8 chars.
 */

#include <string.h>
#include <errno.h>

#include "blobio.h"

extern struct service fs_service;

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

	//  verify full path is endpoint.

	if (*end_point != '/')
		return "first char of end point is not '/'";

	//  verify that the end point exists as a directory

	struct stat st;
	if (jmscott_stat(end_point, &st)) {
		if (errno == ENOENT)
			return "directory end_point does not exist";
		if (S_ISDIR(st.st_mode))
			return "end point is not a directory";
		return strerror(errno);
	}

	char v = *verb;

	/*
	 *  If the verb is a read request then we do NOT care if the sub dirs/
	 *  {data, spool} of end point actually exist or not. we only care if
	 *  the root exists.
	 */
	if (v == 'g' || v == 't' || v == 'e' || v == 'c') {
		TRACE2("read only verb", verb);
		return (char *)0;
	}
	TRACE2("write verb", verb);

	char tgt_dir[BLOBIO_MAX_FS_PATH + 1];

	/*
	 *  Verify existence of directory
	 *
	 *	$end_point/data/fs_<algo>/ exists and is writable.
	 */

	char *err = digest_module->fs_mkdir(tgt_dir, sizeof tgt_dir);
	if (err != (char *)0)
		return err;
	TRACE2("target dir", tgt_dir); 
	return (char *)0;
}

static char *
fs_close()
{
	TRACE("entered");

	return (char *)0;
}

static char *
fs_get(int *ok_no)
{
	TRACE("entered");
	*ok_no = 1;
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
