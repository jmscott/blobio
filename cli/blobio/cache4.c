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

extern char *verb;
extern int output_fd;
extern struct service bio4_service;
extern struct service fs_service;

struct service cache4_service;

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
 *	brr_path not supported!  code desparatly needs to be refactored.
 */
static char *
cache4_get(int *ok_no)
{
	int len;

	TRACE("cache4_get: calling fs_service.get");
	char *err = fs_service.get(ok_no);
	if (err)
		return err;

	TRACE("cache4_get: fs_service.get ok");
	if (*ok_no == 0)
		return (char *)0;	//  found blob in fs cache

	//  blob not in file cache so do the delayed open of bio4 service.

	bio4_service.digest = cache4_service.digest;
	char *right_colon = strrchr(cache4_service.end_point, ':');
	len = right_colon - cache4_service.end_point;
	memcpy(bio4_service.end_point, cache4_service.end_point,  len);
	bio4_service.end_point[len] = 0;
	err = bio4_service.open();
	if (err)
		return err;

	//  build the path to the temp blob
	//
	//  Note:
	//	How to handle if another fetch is flight?

	char tmp_path[PATH_MAX+1], tmp_name[PATH_MAX+1];
	snprintf(tmp_name, sizeof tmp_name, "/cache4-%ul", getpid());
	tmp_path[0] = 0;
	buf3cat(tmp_path, sizeof tmp_path,
		right_colon + 1,
		"/tmp/",
		tmp_name
	);
	int tmp_fd = uni_open_mode(tmp_path, O_WRONLY|O_CREAT,S_IRUSR|S_IRGRP);
	if (tmp_fd < 0)
		return strerror(errno);
	output_fd = tmp_fd;
	char *status = bio4_service.get(ok_no);
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	if (*ok_no == 1)					//  no blob
		return zap_temp(tmp_path, tmp_fd);

	/*
	 *  blob exists, so move blob file from temp to cache area:
	 *
	 *	cache/data/<algo>_fs/<path to blob>
	 */
	output_fd = -1;
	if (uni_close(tmp_fd))
		return strerror(errno);
	tmp_fd = -1;

	char cache_path[PATH_MAX+1];
	cache_path[0] = 0;
	snprintf(cache_path, sizeof cache_path,
		"cache/data/%s_fs",
		bio4_service.digest->algorithm
	);
	TRACE2("cache4_get: pre cache_path", cache_path);
	len = strlen(cache_path);
	status = fs_service.digest->fs_mkdir(
			cache_path + len,
			sizeof cache_path - len
	);
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	TRACE2("cache4_get: target cache_path", cache_path);
	if (uni_rename(tmp_path, cache_path)) {
		zap_temp(tmp_path, tmp_fd);
		return strerror(errno);
	}
	TRACE("cache4_get: rename ok");
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
	(void)ok_no;
	return (char *)0;
}

static char *
cache4_give(int *ok_no)
{
	(void)ok_no;
	return (char *)0;
}

static char *
cache4_roll(int *ok_no)
{
	(void)ok_no;
	return "\"roll\" not implemented (yet) in \"cache4\" service";
}

static char *
cache4_wrap(int *ok_no)
{
	(void)ok_no;
	return "\"wrap\" not implemented (yet) in \"cache4\" service";
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
