/*
 *  Synopsis:
 *	Frisk and extract query from uri: {tmo,brr}={\d{1,3}|<path>}
 *  Note:
 *	No escaping in query args, which is a serious bug!
 */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include "jmscott/libjmscott.h"
#include "blobio.h"

#define _QARG_MAX_VALUE		64

/*
 *  A prosaic frisker of query arguments in BLOBIO_SERVICE.
 *
 *  Return:
 *	(char *)0:	no error and updates *p_equal to first char after '='
 *	error:		english error string
 *  Note:
 *	Add an arg for length of the value!
 *
 *	Correctness of escaped chars, etc, not frisked!
 */
static char *
frisk_qarg(char *expect, char *given, char **p_equal, int max_vlen)
{
	char *e = expect, ce;
	char *g = given, cg;

	TRACE2("expect", expect);
	TRACE2("given", given);

	while ((ce = *e++)) {
		cg = *g++;
		if (cg == 0)
			return "unexpected null char in value";
		if (!isascii(cg))
			return "char in value is not ascii";
		if (cg != ce)
			return "unexpected char in arg";
	}
	if (*g++ != '=')
		return "no \"=\" at end of arg";
	*p_equal = g;

	//  insure value of query arg is copacetic.

	while ((cg = *g++)) {
		if (g - *p_equal > max_vlen)
			return "value too large";
		if (!isascii(cg))
			return "char in value is not ascii";
		if (!isgraph(cg))
			return "char in value is not graphic";
		if (cg == '&')
			break;
	}
	if (g - given == 1)
		return "empty value";
	return (char *)0;
}

//  a memory unsafe concatentor of error messages regarding query args.

static char *
qae(char *qarg, char *err)
{
	static char msg[MAX_ATOMIC_MSG];

	msg[0] = 0;
	jmscott_strcat3(msg, sizeof msg,
			qarg,
			": ",
			err
	);
	return msg;
}

/*
 *  Synopsis:
 *	Frisk (but not extract) query args in a uri from $BLOBIO_SERVICE.
 *  Description:
 *	Brute force frisk (but not extract) of query args in a uri
 *	$BLOBIO_SERVICE.
 *
 *		algo=[a-z][a-z0-9]{0,7}	#  algorithm for service wrap
 *		brr=[01]		#  write brr record
 *		BR=path/to/blobio	#  explict path to blobio root
 *
 *	Tis an error if args other than the three above exist in query string.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_qargs(char *query)
{
	char *q = query, c;
	char *equal;
	char seen_BR = 0, seen_algo = 0, seen_brr = 0;
	int count;
	char *err;

	if (!query || !*query)
		return (char *)0;
	while ((c = *q++)) {
		switch (c) {

		//  match: algo=[a-z][a-z0-9]{0,7}[&\000]

		case 'a':
			err = frisk_qarg("algo", q - 1, &equal, 8);
			if (err)
				return qae("algo", err);
			if (seen_algo)
				return "algo: specified more than once";

			//  insure the algorithm matches [a-z][a-z0-9]{0,7}
			q = equal;

			c = *q++;
			if (!islower(c) || !isalpha(c))
				return "algo: "
				       "first char in value "
				       "not lower alpha"
				;
				
			//  scan [a-z0-9]{0,7}[&\000]
			while ((c = *q++)) {
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isdigit(c) &&
				    !(islower(c) && isalpha(c)))
					return "algo: non alnum in value";
			}
			seen_algo = 1;
			break;

		//  match: brr=[01]
		case 'b':
			err = frisk_qarg("brr", q - 1, &equal, 1);
			if (err)
				return qae("brr", err);
			if (seen_brr)
				return "brr: specified more than once";
			q = equal;
			c = *q++;
			if (c != '1' && c != '0')
				return "brr: value not \"0\" or \"1\"";
			seen_brr = 1;
			break;
		case 'B':
			err = frisk_qarg("BR", q - 1, &equal, 64);
			if (err)
				return qae("BR", err);
			if (seen_BR)
				return "BR: specified more than once";

			//  skip over blobio root path till we hit end of string
			//  or '&'
			count = 0;
			while ((c = *q++))
				if (!c)
					return (char *)0;
			break;
		default:
			return "unexpected char in query arg";
		}
		if (c == 0)
			break;
		if (c == '&')
			if (!*q)
				return "no query argument after \"&\"";
	}
	return (char *)0;
}

/*
 *  Synopsis:
 *  	Extract the "algo" value from a frisked query string.
 */
void
BLOBIO_SERVICE_get_algo(char *query, char *algo)
{
	char *q = query, c, *a = algo;
	
	*a = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;
		if (c == 'a') {
			q += 4;		// skip over "lgo="

			// already know algo matches [a-z][a-z]{0,7}
			while ((c = *q++) && c != '&')
				*a++ = c;
			*a = 0;
			return;
		}

		while ((c = *q++) && c != '&')
			;
		if (!c)
			return;
	}
}

/*
 *  Synopsis:
 *  	Extract the "BLOBIO_ROOT" file path from a frisked query string.
 */
void
BLOBIO_SERVICE_get_BR(char *query, char *path)
{
	char *q = query, c, *p = path;
	
	*p = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		if (c == 'B') {
			q += 2;			//  skip "R="

			//  extract the frisked path
			while ((c = *q++) && c != '&')
				*p++ = c;
			*p = 0;
			return;
		}
		while ((c = *q++) && c != '&')
			;
		if (!c)
			return;
	}
}

/*
 *  Synopsis:
 *  	Extract the "brr" boolean [01] from a frisked query string.
 */
void
BLOBIO_SERVICE_get_brr(char *query, char *put_brr)
{
	char *q = query, c;
	
	while ((c = *q++)) {
		if (c == '&')
			continue;

		if (c == 'b') {
			*put_brr = q[3];
			return;
		}
		while ((c = *q++) && c != '&')
			;
		if (!c)
			return;
	}
}
