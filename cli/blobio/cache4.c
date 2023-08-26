/*
 *  Synopsis:
 *	A sematics aware driver for caching bio4 requests to posix fs service
 *  Usage:
 *	cache4:<host>:port:/path/to/fs/root
 *  Note:
 *	--brr-path needs a transport[] for cache4.  Currently the underlying
 *	service for "fs" or "bio4" generates the transport for the brr-path
 *	construction.
 *
 *	Need to query TMPDIR for temporary copy of the blob.
 *
 *	Way to many globals and assumptions about behavior of fs/bio4 services.
 *
 *	We assume certain behaviour of the bio4_service.
 *	Would be nice to generalize as cache:slow/fast service.
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

extern char		verb[];
extern char		ascii_digest[];
extern int		output_fd;
extern struct service	bio4_service;
extern struct service	fs_service;

struct service		cache4_service;

/*
 *  The end point is a concatenation of the bio4 service and file system
 *  service.
 *
 *	host:port:/path/to/blobio/root
 *
 *  Split out the bio4 and fs service and let the underlying class parse
 *  those services.
 */
static char *
cache4_end_point_syntax(char *host_port_path)
{
	char bio4[255 + 1];
	int len = strlen(host_port_path);

	memcpy(bio4, host_port_path, len);
	bio4[len] = 0;

	//  find the fs service and replace right hand colon with null.
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

	if (jmscott_unlink(path)) {
		if (errno == ENOENT)
			return (char *)0;
		return strerror(errno);
	}
	if (fd > -1) {
		output_fd = -1;
		if (jmscott_close(fd))
			return strerror(errno);
	}
	return (char *)0;
}

static char *
_bio4_open()
{
	bio4_service.digest = cache4_service.digest;
	char *right_colon = strrchr(cache4_service.end_point, ':');
	int len = right_colon - cache4_service.end_point;
	memcpy(bio4_service.end_point, cache4_service.end_point, len);
	bio4_service.end_point[len] = 0;
	return bio4_service.open();
}

/*
 *  Get a blob from cache, filling empty cache from bio4 service.
 *
 *  Note:
 *	This is code is a mess!  Time to refactor all of blobio command
 *	to be reentrant.
 */
static char *
cache4_get(int *ok_no)
{
	int len;

	char *err = fs_service.get(ok_no);
	if (err || *ok_no == 0)
		return err;

	TRACE("blob not in fs, so query bio4");
	//  blob not in file cache so do the delayed open of bio4 service.
	if ((err = _bio4_open()))
		return err;

	/*
	 *  Copy the bio4 blob into a temp blob in directory
	 *  fs_service.end_point/tmp.
	 *
	 *  Note:
	 *	Need to query environment for TMPDIR.
	 */

	char *right_colon = strrchr(cache4_service.end_point, ':');
	char tmp_path[PATH_MAX];
	snprintf(tmp_path, sizeof tmp_path, "%s/tmp/cache4-%s-%ul",
		right_colon + 1,
		ascii_digest,
		getpid()
	);
	TRACE2("copy bio4 blob to tmp", tmp_path);
	int tmp_fd = jmscott_open(tmp_path, O_WRONLY|O_CREAT,S_IRUSR|S_IRGRP);
	if (tmp_fd < 0)
		return strerror(errno);
	int cli_output_fd = output_fd;
	output_fd = tmp_fd;
	char *status = bio4_service.get(ok_no);
	output_fd = cli_output_fd;
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	if (*ok_no == 1)
		return zap_temp(tmp_path, tmp_fd);

	/*
	 *  blob written to temp directory, so move blob file from temp to
	 *  cache tree:
	 *
	 *	data/fs_<algo>/<path to blob>
	 */
	output_fd = -1;
	if (jmscott_close(tmp_fd))
		return strerror(errno);
	tmp_fd = -1;

	char cache_path[PATH_MAX];
	cache_path[0] = 0;
	snprintf(cache_path, sizeof cache_path,
		"%s/data/fs_%s",
		right_colon + 1,
		bio4_service.digest->algorithm
	);
	TRACE2("cache path", cache_path);

	/*
	 *  Note:
	 *	Unfortunatly, digest->fs_mkdir() only makes the hash portion
	 *	of the path to the blob.  Need to also mkdir the dirs in
	 *	data/fs_<algo>.
	 */
	status = fs_service.digest->fs_mkdir(
			cache_path,
			sizeof cache_path - strlen(cache_path)
	);
	if (status) {
		zap_temp(tmp_path, tmp_fd);
		return status;
	}
	len = strlen(cache_path);
	status = fs_service.digest->fs_name(
			cache_path + len,
			sizeof cache_path - strlen(cache_path)
	);
	if (status)
		return status;
	if (rename(tmp_path, cache_path)) {
		int e = errno;
		zap_temp(tmp_path, tmp_fd);
		if (e != EEXIST)
			return strerror(e);
	}
	if((err = fs_service.open()))
		return err;
	TRACE("calling fs_service.get again");
	output_fd = cli_output_fd;
	return fs_service.get(ok_no);
}

/*
 *  Need to think about sematics of cached eat.
 */
static char *
cache4_eat(int *ok_no)
{
	char *status = fs_service.eat(ok_no);
	if (status)
		return status;
	if (*ok_no == 0)
		return (char *)0;
	if ((status = _bio4_open()))
		return status;
	if ((status = bio4_service.eat(ok_no)))
		return status;
	return (char *)0;
}

static char *
cache4_put(int *ok_no)
{
	(void)ok_no;
	return "\"put\" not supported";
}

static char *
cache4_take(int *ok_no)
{
	//  take only local cache and not remote bio4 server.
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
	.eat			=	cache4_eat,
	.put			=	cache4_put,
	.take			=	cache4_take,
	.give			=	cache4_give,
	.roll			=	cache4_roll,
	.wrap			=	cache4_wrap
};
