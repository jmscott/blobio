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
	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  Note:
 *	brr_path not supported!
 */
static char *
cache4_get(int *ok_no)
{
	char *err = fs_service.get(ok_no);

	if (err)
		return err;
	if (*ok_no == 0)
		return (char *)0;	//  found blob in fs cache

	//  fetch the file from bio4 service and write to the cache.
	err = bio4_service.get(ok_no);
	if (err)
		return err;
	if (*ok_no)
		return (char *)0;	//  blob not found in bio4 service

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
