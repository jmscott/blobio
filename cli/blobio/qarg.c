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
static char *
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
 *  	Extract the "algo" value from a frisked query string.
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
		q += 4;		// skip over "lgo="

		// already know algo matches [a-z][a-z]{0,7}
		
		while ((c = *q++) && c != '&')
			*a++ = c;
		*a = 0;
	}
	return (char *)0;
}

/*
 *  Synopsis:
 *  	Extract the "BRR" file path from the frisked query string.
 *  Usage:
 *	char *err;
 *	err =  BLOBIO_SERVICE_frisk_algo(query, path);
 *	if (err)
 *		return err;		//  error in a query arg
 */
char *
BLOBIO_SERVICE_get_BR(char *query, char *path)
{
	char *q = query, c, *p = path;
	
	*p = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "BRR", so skip to next '&' or end of string 
		if (c != 'B') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		q += 3;			//  skip "RR="

		//  extgract the frisked path
		while ((c = *q++) && c != '&')
			*p++ = c;
		*p = 0;
	}
	return (char *)0;
}
