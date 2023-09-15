/*
 *  Synopsis:
 *	Frisk and extract query from uri: {tmo,brr}={\d{1,3}|<path>}
 *  Note:
 *	Change value of brr=[01] to brr=[t|f].  we may wish to use
 *	brr=[0-9] for a file descriptor.
 *
 *	--help should document the query args, since they are the args
 *	for all services.
 *
 *	No escaping in query args, which is a limitation!
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
	TRACE_LL("max value length", (long long)max_vlen);

	while ((ce = *e++)) {
		cg = *g++;
		if (cg == 0)
			return "unexpected null char in value";
		if (!isascii(cg))
			return "char in value is not ascii";
		if (cg != ce) {
			return "unexpected char in arg";
		}
	}
	if (*g++ != '=')
		return "no \"=\" at end of arg";
	*p_equal = g;

	//  insure value of query arg is copacetic.

	while ((cg = *g++)) {
		if (cg == '&')
			break;
		if (g - *p_equal > max_vlen)
			return "value too large";
		if (!isascii(cg))
			return "char in value is not ascii";
		if (!isgraph(cg))
			return "char in value is not graphic";
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
 *		brr=[0-9a-f][0-9a-f]	#  bit mask for brr verbs to write
 *
 *	Tis an error if args other than the three above exist in query string.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_qargs(char *query)
{
	TRACE("entered");

	char *q = query, c;
	char *equal;
	char seen_algo = 0, seen_brr = 0;
	char *err;

	if (!query || !*query)
		return (char *)0;
	while ((c = *q++)) {
		switch (c) {

		//  match: algo=[a-z][a-z0-9]{0,7}[&\000]

		case 'a':
			TRACE("saw char 'a', expect qarg \"algo\"");
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

		//  match: brr=[0-9a-f][0-9a-f]		bit mask for bee verbs
		case 'b':
			TRACE("saw char 'b', expect qarg \"brr\"");
			err = frisk_qarg("brr", q - 1, &equal, 2);
			if (err)
				return qae("brr", err);
			if (seen_brr)
				return "brr: specified more than once";

			q = equal;

			c = *q++;
			if (c == 0)
				return "brr: empty: want [0-9a-f][0-9a-f]";
			if (!isxdigit(c))
				return "brr: first char is not hex digit";
			if (isalpha(c) && !islower(c))
				return "first hex digit is not lower case";

			c = *q++;
			if (c == 0)
				return "brr: short: want [0-9a-f][0-9a-f]";
			if (!isxdigit(c))
				return "brr: second char is not hex digit";
			if (isalpha(c) && !islower(c))
				return "second hex digit is not lower case";

			seen_brr = 1;
			c = *q++;
			break;
		default: {
			char got[2];

			got[0]= c;  got[1] = 0;
			TRACE2("unexpected char in query arg: got", got);
			return "unexpected char in query arg";
		}}
		if (c == 0)
			break;
		if (c == '&')
			if (!*q)
				return "no query argument after \"&\"";
	}
	TRACE("bye (ok)");
	return (char *)0;
}

/*
 *  Synopsis:
 *  	Extract the "algo" value from a frisked query string.
 */
void
BLOBIO_SERVICE_get_algo(char *query, char *algo)
{
	TRACE("entered");

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
 *  	Extract the bit mask for blob request records to write.
 */
void
BLOBIO_SERVICE_get_brr_mask(char *query, unsigned char *p_mask)
{
	TRACE("entered");

	char *q = query, c;
	
	while ((c = *q++)) {
		if (c == '&' || c != 'b')	//  not "brr="
			continue;

		q += 3;			//  skip over "brr="

		unsigned cu, cl;	//  upper & lower bytes

		unsigned c = *q++;
		if (islower(c))
			cu = (c - 'a') + 10;
		else
			cu = c - '0';

		c = *q++;
		if (islower(c))
			cl = (c - 'a') + 10;
		else
			cl = c - '0';
		*p_mask = (cu << 4) | cl;

		TRACE2("qarg: brr mask", brr_mask2ascii(*p_mask));
		return;
	}
}
