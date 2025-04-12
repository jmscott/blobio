/*
 *  Synopsis:
 *	Frisk and extract query from uri: {algo,brr}={\d{1,3}|<path>}
 *  Note:
 *	Add qarg "tmo=<sec>" for timeout!
 *
 *	No proper escaping in query args!
 */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "jmscott/libjmscott.h"
#include "blobio.h"

#define _QARG_MAX_VALUE		64

static int	algo_offset = -1;
static int	algo_length = -1;

static int	brr_mask_offset = -1;

static int	fnp_offset = -1;
static int	fnp_length = -1;

/*
 *  A prosaic frisker of query arguments in BLOBIO_SERVICE.
 *
 *  Return:
 *	(char *)0:	no error and updates *p_equal to first char after '='
 *	error:		english error string
 *  Note:
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
 *		algo=[a-z][a-z0-9]{0,7}	#  algorithm for verb "wrap"
 *		brr=[0-9a-f][0-9a-f]	#  bit mask for brr verbs to write
 *		fnp=[a-z][0-9a-f]{0,15}	#  file name prefix of brr log in spool
 *
 *	tis an error if args other than the three above exist in query string.
 *  returns:
 *	an error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_qargs(char *query)
{
	TRACE("entered");

	char *q = query, c;
	char *equal;
	char *err;
	static char eonce[] = "arg specified more than once";
	static char efirst[] = "first char not lower alpha";
	static char elowdig[] = "char not lower alpha or digit";
	static char ehex1[] = "first digit is not lower hex";
	static char ehex2[] = "second digit is not lower hex";
	static char enograph[] = "non graph char in path";

	if (!query || !*query)
		return (char *)0;
	while ((c = *q++)) {
		switch (c) {

		//  match: algo=[a-z][a-z0-9]{0,7}[&\000]

		case 'a':
			TRACE("saw char 'a', so expect \"algo\"");
			err = frisk_qarg("algo", q - 1, &equal, 8);
			if (err)
				return qae("algo", err);
			if (algo_offset > -1)
				return qae("algo", eonce);

			//  insure the algorithm matches [a-z][a-z0-9]{0,7}
			q = equal;
			algo_offset = q - query;

			c = *q++;
			if (!islower(c))
				return qae("algo", efirst);

			//  scan [a-z0-9]{0,7}[&\000]

			while ((c = *q++) && c != '&')
				if (!islower(c) && !isdigit(c))
					return qae("algo", elowdig);
			algo_length = q - &query[algo_offset];
			if (!c)
				return (char *)0;
			algo_length--;
			break;

		//  match: brr=[0-9a-f][0-9a-f], the bit mask for which verbs 
		//         write brr records

		case 'b':
			TRACE("saw char 'b', so expect \"brr\"");
			err = frisk_qarg("brr", q - 1, &equal, 2);
			if (err)
				return qae("brr", err);
			if (brr_mask_offset > -1)
				return qae("brr", eonce);

			q = equal;
			brr_mask_offset = q - query;

			c = *q++;
			if (c == 0)
				qae("brr", "no hex value");
			if (!isxdigit(c) || (isalpha(c) && !islower(c)))
				return qae("brr", ehex1);

			c = *q++;
			if (c == 0)
				return qae("brr", "no second hex char");
			if (!isxdigit(c) || !islower(c))
				return qae("brr", ehex2);

			c = *q++;
			break;
		case 'f':
			TRACE("saw char 'f', so expect \"fnp\"");
			err = frisk_qarg("fnp", q - 1, &equal, 32);
			if (err)
				return qae("fnp", err);
			if (fnp_offset > -1)
				return qae("fnp", eonce);

			//  insure the algorithm matches [[:graph:]]{1,32}
			q = equal;
			fnp_offset = q - query;

			TRACE2("fnp: parsing query fragment", q);

			//  scan [[:graph:]]{1,32}
			while ((c = *q++) && c != '&')
				if (!isgraph(c))
					return qae("fnp", enograph);
			fnp_length = q - &query[fnp_offset];
			if (c)
				fnp_length--;
			if (fnp_length == 0)
				return qae("fnp", "no path after =");
			if (fnp_length > 32)
				return qae("fnp", "path length > 32 chars");
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
 *  Note:
 *	Already frisked value of "algo" for matching [a-z][a-z0-9]{0,7}
 */
void
BLOBIO_SERVICE_get_algo(char *query)
{
	if (algo_offset == -1)
		return;
	memcpy(algo, query + algo_offset, algo_length);
	algo[algo_length] = 0;

	TRACE2("algo", algo);
}

/*
 *  Synopsis:
 *  	Extract the "fnp" value from a frisked query string.
 *  Note:
 *	Already frisked value of "fnp" for matching [a-z][a-z0-9_-]{0,31}
 */
void
BLOBIO_SERVICE_get_fnp(char *query)
{
	if (fnp_offset == -1)
		return;
	memcpy(fnp, query + fnp_offset, fnp_length);
	fnp[fnp_length] = 0;
	TRACE2("fnp", fnp);
}

/*
 *  Synopsis:
 *  	Extract the bit mask for blob request records to write.
 *  Note:
 *	Already frisked value of "brr" for matching [a-f0-9][a-f0-9]
 */
void
BLOBIO_SERVICE_get_brr_mask(char *query)
{
	if (brr_mask_offset == -1)
		return;

	unsigned char cu, cl;	//  upper & lower bytes
	char *q = &query[brr_mask_offset];

	unsigned c = *q++;
	if (islower(c))
		cu = (c - 'a') + 10;
	else
		cu = c - '0';

	c = *q;
	if (islower(c))
		cl = (c - 'a') + 10;
	else
		cl = c - '0';
	brr_mask = (cu << 4) | cl;

	TRACE2("brr mask", brr_mask2ascii(brr_mask));
}
