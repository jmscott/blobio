/*
 *  Synopsis:
 *	A sematics aware driver for caching bio4 requests to posix fs service
 *  Usage:
 *	cache4:<host>:port:/path/to/fs/root
 *  Note:
 *	We assume certain behaviour of the bio4_service.
 *	Would be nice to generalize as cache:slow service/fast service.
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

#ifdef COMPILE_TRACE

#define _TRACE(msg)		if (tracing) _trace(msg)
#define _TRACE2(msg1,msg2)	if (tracing) _trace2(msg1,msg2)

#else

#define _TRACE(msg)
#define _TRACE2(msg1,msg2)
#define _TRACE3(msg1,msg2,msg3)
#define _TRACE3(msg1,msg2,msg3)

#endif

extern char *verb;
extern char ascii_digest[];
extern int output_fd;
extern struct service bio4_service;
extern struct service fs_service;

struct service cache4_service;

static void
_trace(char *msg)
{
	if (verb)
		trace3("cache4", verb, msg);
	else
		trace2("cache4", msg);
}

static void
_trace2(char *msg1, char *msg2)
{
	if (verb)
		trace4("cache4", verb, msg1, msg2);
	else
		trace3("cache4", msg1, msg2);
}

/*
 *  The end point is the root directory of the source blobio file system.
 *  No non-ascii or '?' characters are allowed in root file path.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
cache4_end_point_syntax(char *host_port_path)
{
	char bio4[255 + 1];
	int len = strlen(host_port_path);

	memcpy(bio4, host_port_path, len);
	bio4[len] = 0;

	//  find the fs service and replace first colon with null.
	char *fs = strchr(bio4, ':');
	if (!fs)
		return "no colon in service"; 
	fs = strchr(fs + 1, ':');
	if (!fs)
		return "no colon following bio4 service";
	*fs++ = 0;

	//  verify bio4 service.
	char *err = bio4_service.end_point_syntax(bio4);
	if (err)
		return err;
	return fs_service.end_point_syntax(fs);
}

static char *
cache4_open()
{
	if (verb[0] != 'g' || verb[1] != 'e')
		return "only get verb is supported";

	fs_service.digest = cache4_service.digest;

	strcpy(
		fs_service.end_point,
		strrchr(cache4_service.end_point, ':') + 1
	);
	char *err = fs_service.open();
	if (err)
		return err;
	return (char *)0;
}

static char *
cache4_open_output()
{
	return (char *)0;
}

static char *
cache4_close()
{
	char *err = fs_service.close();
	if (err)
		return err;
	err = bio4_service.close();
	if (err)
		return err;
	return (char *)0;
}

static char *
zap_temp(char *path, int fd)
{
	//  unlink first, so the temp file still is freed at process exit.

	if (uni_unlink(path)) {
		if (errno == ENOENT)
			return (char *)0;
		return strerror(errno);
	}
	if (fd > -1) {
		output_fd = -1;
		if (uni_close(fd))
			return strerror(errno);
	}
	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  Note:
 *	brr_path not supported!
 *
 *	This is code is a mess!
 */
static char *
cache4_get(int *ok_no)
{
	int len;

	_TRACE("calling fs_service.get");
	char *err = fs_service.get(ok_no);
	if (err)
		return err;

	if (*ok_no == 0)
		return (char *)0;	//  found blob in fs cache
	_TRACE("fs_service.get == no");

	//  blob not in file cache so do the delayed open of bio4 service.

	bio4_service.digest = cache4_service.digest;
	char *right_colon = strrchr(cache4_service.end_point, ':');
	len = right_colon - cache4_service.end_point;
	memcpy(bio4_service.end_point, cache4_service.end_point, len);
	bio4_service.end_point[len] = 0;
	if ((err = bio4_service.open()))
		return err;
	/*
	 *  Build a path to the temp blob in fs.end_point/tmp.
	 *
	 *  Note:
	 *	How to handle if another fetch is flight?
	 */
	_TRACE("bio4.open() ok");

	char tmp_path[PATH_MAX+1];
	snprintf(tmp_path, sizeof tmp_path, "%s/tmp/cache4-%s-%ul",
		right_colon + 1,
		ascii_digest,
		getpid()
	);
	_TRACE2("tmp path", tmp_path);
	int tmp_fd = uni_open_mode(tmp_path, O_WRONLY|O_CREAT,S_IRUSR|S_IRGRP);
	if (tmp_fd < 0)
		return strerror(errno);
	int cli_output_fd = output_fd;
	output_fd = tmp_fd;
	char *status = bio4_service.get(ok_no);
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	if (*ok_no == 1)					//  no blob
		return zap_temp(tmp_path, tmp_fd);

	/*
	 *  blob exists, so move blob file from temp to cache tree:
	 *
	 *	data/<algo>_fs/<path to blob>
	 */
	output_fd = -1;
	if (uni_close(tmp_fd))
		return strerror(errno);
	tmp_fd = -1;

	char cache_path[PATH_MAX+1];
	cache_path[0] = 0;
	snprintf(cache_path, sizeof cache_path,
		"%s/data/%s_fs",
		right_colon + 1,
		bio4_service.digest->algorithm
	);
	_TRACE2("pre cache_path", cache_path);

	/*
	 *  Note:
	 *	Unfortunatly, digest->fs_mkdir() only makes the hash portion
	 *	of the path to the blob.  Need to also mkdir the dirs in
	 *	data/<algo>_fs.
	 */
	status = fs_service.digest->fs_mkdir(
			cache_path,
			sizeof cache_path - strlen(cache_path)
	);
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	_TRACE2("target dir cache path", cache_path);
	len = strlen(cache_path);
	status = fs_service.digest->fs_name(
			cache_path + len,
			sizeof cache_path - strlen(cache_path)
	);
	if (status)
		return status;
	_TRACE2("target file cache path", cache_path);
	if (uni_rename(tmp_path, cache_path)) {
		int e = errno;
		zap_temp(tmp_path, tmp_fd);
		return strerror(e);
	}
	if((err = fs_service.open()))
		return err;
	_TRACE("calling fs_service.get again");
	output_fd = cli_output_fd;
	err = fs_service.get(ok_no);
	if (err)
		return err;
	_TRACE("rename ok");
	return (char *)0;
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
cache4_eat(int *ok_no)
{
	(void)ok_no;
	return (char *)0;
}

static char *
cache4_put(int *ok_no)
{
	(void)ok_no;
	return (char *)0;
}

static char *
cache4_take(int *ok_no)
{
	return fs_service.take(ok_no);
}

static char *
cache4_give(int *ok_no)
{
	(void)ok_no;
	return "\"give\" not supported";
}

static char *
cache4_roll(int *ok_no)
{
	(void)ok_no;
	return "\"roll\" not supported";
}

static char *
cache4_wrap(int *ok_no)
{
	(void)ok_no;
	return "\"wrap\" not supported";
}

struct service cache4_service =
{
	.name			=	"cache4",
	.end_point_syntax	=	cache4_end_point_syntax,
	.open			=	cache4_open,
	.close			=	cache4_close,
	.get			=	cache4_get,
	.open_output		=	cache4_open_output,
	.eat			=	cache4_eat,
	.put			=	cache4_put,
	.take			=	cache4_take,
	.give			=	cache4_give,
	.roll			=	cache4_roll,
	.wrap			=	cache4_wrap
};
