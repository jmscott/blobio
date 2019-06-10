/*
 *  Synopsis:
 *	A driver for caching bio4 requests to posix fs service
 *  Usage:
 *	cache4:<host>:port:/path/to/fs/root
 *  Note:
 *	Would be nice to generalize as cache:slow service/fast service
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

extern struct service bio4_service;
extern struct service fs_service;

//  does the verb imply a possible write to the file system?

#define IS_WRITE_VERB()	(*verb == 'p' || (*verb == 'g' && *verb == 'i'))

struct service cache4_service;

/*
 *  The end point is the root directory of the source blobio file system.
 *  No non-ascii or '?' characters are allowed in root file path.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 *
 *	Unfortunatly, the space for host_port_path is written to by this
 *	routine.  Instead a copy should be made.
 */
static char *
cache4_end_point_syntax(char *host_port_path)
{
	char *colon_bio4 = strchr(host_port_path, ':');
	if (!colon_bio4)
		return "no colon in service"; 

	char *colon_fs = strchr(colon_bio4 + 1, ':');
	if (!colon_fs)
		return "no colon following bio4 service";

	//  verify bio4 service.
	*colon_fs = 0;
	char *err = bio4_service.end_point_syntax(host_port_path);
	if (err)
		return err;

	return (char *)0;
}

/*
 *  Verify that the root and data directories of the blob file system
 *  are at least searchable.
 */
static char *
cache4_open()
{
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
	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  If output path exists then link the output path to the blob path.
 *  Copy the input to the output if the blobs are on different devices.
 */
static char *
cache4_get(int *ok_no)
{
	(void)ok_no;
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
