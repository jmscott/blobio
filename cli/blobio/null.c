/*
 *  Synopsis:
 *	Drive requests for null:/dev/null or null:/<algo>:digest.
 *  Note:
 *	The sematics of service "null:/dev/null" are intended to be the same
 *	as those for unix device /dev/null.
 */
#include <string.h>

#include "blobio.h"

extern char		verb[];
extern char		algorithm[9];
extern char		chat_history[9];
extern char		transport[129];
extern char		*output_path;
extern int		output_fd;
extern char		*input_path;
extern int		input_fd;
extern char		*null_device;
extern long long	blob_size;

extern struct service	null_service;

static char *
null_end_point_syntax(char *null_path)
{
	if (strcmp(null_path, "/dev/null") == 0)
		return (char *)0;
	return "unknown null path: expected /dev/null";
}

static char *
null_open()
{
	return (char *)0;
}

static char *
null_open_output()
{
	return (char *)0;
}

static char *
null_close()
{
	return (char *)0;
}

/*
 *  Note:
 *	we now know the service is null:/dev/null.  therefore all "get"
 *	requests are empty, since no udig exists.  
 */
static char *
null_get(int *ok_no)
{
	*ok_no = 1;
	return "null_get not supported";
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
null_eat(int *ok_no)
{
	*ok_no = 0;
	return "null_eat not supported";
}

static char *
null_put(int *ok_no)
{
	*ok_no = 0;
	return "null_put not supported";
}

static char *
null_take(int *ok_no)
{
	*ok_no = 0;
	return "null_take not supported";
}

static char *
null_give(int *ok_no)
{
	*ok_no = 0;
	return "null_give not supported";
}

static char *
null_roll(int *ok_no)
{
	*ok_no = 0;
	return "null_roll not supported";
}

static char *
null_wrap(int *ok_no)
{
	*ok_no = 0;
	return "null_wrap not supported";
}

struct service null_service =
{
	.name			=	"null",
	.end_point_syntax	=	null_end_point_syntax,
	.open			=	null_open,
	.close			=	null_close,
	.get			=	null_get,
	.open_output		=	null_open_output,
	.eat			=	null_eat,
	.put			=	null_put,
	.take			=	null_take,
	.give			=	null_give,
	.roll			=	null_roll,
	.wrap			=	null_wrap
};
