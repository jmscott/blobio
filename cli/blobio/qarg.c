/*
 *  Synopsis:
 *	Frisk and extract query from uri: {tmo,brr}={\d{1,3}|<path>}
 *  Note:
 *	No escaping in query args, which is a serious bug!
 */
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

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
char *
frisk_qarg(char *expect, char *given, char **p_equal, int max_vlen)
{
	char *e = expect, ce;
	char *g = given, cg;

	while ((ce = *e++)) {
		cg = *g++;
		if (cg == 0)
			return "unexpected null char in value";
		if (!isascii(cg))
			return "char in value is not ascii";
		if (cg != ce)
			return "unexpected char in value";
	}
	if (*g++ != '=')
		return "equal char \"=\" not terminating query arg variable";
	*p_equal = g;

	//  insure value of query arg is the correct length
	while ((cg = *g++)) {
		if (g - given >= max_vlen)
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
/*
 *  Synopsis:
 *	Frisk (but not extract) query args in a uri of $BLOBIO_SERVICE.
 *  Description:
 *	Brute force frisk (but not extract) of query args in a uri
 *	$BLOBIO_SERVICE.
 *
 *		algo=[sha|btc20]	#  algorithm for service wrap
 *		brr=0|1			#  write brr record
 *		BR=path/to/blobio	#  explict path to blobio root
 *
 *	Tis an error if args other than the three above exist in query string.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_query(char *query)
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
				return err;
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
				if (!isdigit(c) ||
				    (islower(c) && isalpha(c)))
					return "algo: non alnum in value";
			}
			seen_algo = 1;
			break;

		//  match: brr=[tf]
		case 'b':
			err = frisk_qarg("brr", q - 1, &equal, 1);
			if (err)
				return err;
			if (seen_brr)
				return "brr: specified more than once";
			q = equal;
			c = *q++;
			if (c != 't' && c != 'f')
				return "brr: value not \"t\" or \"f\"";
			seen_brr = 1;
			break;
		case 'B':
			err = frisk_qarg("BR", q - 1, &equal, 64);
			if (err)
				return err;
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
 *  	Extract timeout (tmo < 256) ) from a frisked $BLOBIO_SERVICE uri.
 *  Usage:
 *	char *status;
 *	status =  BLOBIO_SERVICE_frisk_query(query);
 *	if (status)
 *		return status;		//  error in query arg
 *
 *	...
 *
 *	status = BLOBIO_SERVICE_get_tmo(query, int &tmo);
 *	if (status)
 *		die2("error parsing \"tmo\" query arg", status);
 *	if (tmo == -1)
 *		tmo = 20;	//  no query arg, so set to default 20 seconds
 */
char *
BLOBIO_SERVICE_get_tmo(char *query, int *tmo)
{
	char *q = query, c;

	*tmo = -1;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "tmo", so skip to next '&' or end of string 
		if (c != 't') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		/*
		 *  extract unsigned timeout as int matching \d{1,3}.
		 *  god bless frisking, but we still do a few cheap sanity
		 *  tests.
		 */
		c = *q++;
		if (c != 'm')
			return "impossible: char 'm' not after char 't'";
		c = *q++;
		if (c != 'o')
			return "impossible: char 'o' not after \"tm\"";
		c = *q++;
		if (c != '=')
			return "impossible: char '=' not after \"tm\"";

		 //  extract 1-3 digits from frisked "tmo=" query arg.

		char digits[4];
		digits[0] = *q++;
		c = *q++;
		if (isdigit(c)) {
			digits[1] = c;
			c = *q++;
			if (isdigit(c)) {
				digits[2] = c;
				digits[3] = 0;
			} else
				digits[2] = 0;
		} else
			digits[1] = 0;

		unsigned long long t;
		char *status = jmscott_a2ui63(digits, &t);
		if (status)
			return status;		//  impossible
		if (t > 255)
			return "query arg \"tmo\" > 255";	//  possible
		*tmo = (int)t;
		return (char *)0;
	}

	return (char *)0;
}

/*
 *  Synopsis:
 *  	Extract the "algo" value for use by service drivers.
 *  Usage:
 *	char *err;
 *	err =  BLOBIO_SERVICE_frisk_algo(query, algo);
 *	if (err)
 *		return err;		//  error in a query arg
 *
 *	...
 *
 *	char algo[64];
 *	algo[0] = 0;
 *	err = BLOBIO_SERVICE_get_algo(query, char *algo);
 *	if (err)
 *		die2("error parsing \"algo\"", err);
 */
char *
BLOBIO_SERVICE_get_algo(char *query, char *algo)
{
	char *q = query, c, *a = algo;
	
	*a = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "algo", so skip to next '&' or end of string 
		if (c != 'a') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		c = *q++;
		if (c != 'l')
			return "impossible: char 'l' not after char \"a\"";

		c = *q++;
		if (c != 'g')
			return "impossible: char 'g' not after \"al\"";

		c = *q++;
		if (c != 'o')
			return "impossible: char 'o' not after \"alg\"";

		c = *q++;
		if (c != '=')
			return "impossible: char '=' not after \"algo\"";

		// already know algo matches [a-z][a-z]{0,7}
		
		while ((c = *q++) && c != '&')
			*a++ = c;
		*a = 0;
	}
	return (char *)0;
}

/*
 *  Synopsis:
 *  	Extract the "algo" value for use by service drivers.
 *  Usage:
 *	char *err;
 *	err =  BLOBIO_SERVICE_frisk_algo(query, algo);
 *	if (err)
 *		return err;		//  error in a query arg
 *
 *	...
 *
 *	char algo[64];
 *	algo[0] = 0;
 *	err = BLOBIO_SERVICE_get_algo(query, char *algo);
 *	if (err)
 *		die2("error parsing \"algo\"", err);
 */
char *
BLOBIO_SERVICE_get_BR(char *query, char *path)
{
	char *q = query, c, *p = path;
	
	*p = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "algo", so skip to next '&' or end of string 
		if (c != 'a') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		c = *q++;
		if (c != 'l')
			return "impossible: char 'l' not after char \"a\"";

		c = *q++;
		if (c != 'g')
			return "impossible: char 'g' not after \"al\"";

		c = *q++;
		if (c != 'o')
			return "impossible: char 'o' not after \"alg\"";

		c = *q++;
		if (c != '=')
			return "impossible: char '=' not after \"algo\"";

		// already know path matches [[:isgraph:]] with proper length.
		
		while ((c = *q++) && c != '&')
			*p++ = c;
		*p = 0;
	}
	return (char *)0;
}
