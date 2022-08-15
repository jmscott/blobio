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

/*
 *  A prosaic frisker of query arguments in BLOBIO_SERVICE.
 */
char *
frisk_qarg(char *expect, char *given, char **p_equal)
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
	*equal = g;

	//  insure value of query arg is roughly kosher
	while ((cg = *g++)) {
		if (!isgraphic(cg))
			return "char in value is not graphic";
		if (cg == '&')
			break;
	}
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
 #		BR=path/to/blobio	#  explict path
 *
 *	Tis an error if args other than the three aboce exist in query string.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_query(char *query)
{
	char *q = query, c;
	char seen_brr = 0, seen_BR = 0, seen_algo = 0;
	int count;
	char *equal, *err;

	if (!query || !*query)
		return (char *)0;
	while ((c = *q++)) {
		switch (c) {
		case 'a':		//  match "algo"
			err = peek_qarg("algo", q - 1, &q);
			if (err)
				return err;
			
			//  at least one char
			c = *q++;
			if (!c || c == '&')
				return "missing file path after arg \"algo=\"";
			if (seen_algo)
				return "\"algo\" specified more than once";
			seen_algo = 1;

			//  skip over path till we hit end of string or '&'
			count = 0;
			while ((c = *q++)) {
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isascii(c))
					return "non ascii char in \"algo\" arg";
				if (count == 0) {
					if (!islower(c) || !isalpha(c))
						return "first char not "
						       "lower alpha in \"algo\""
						;
				} else {
					//  Note: wrong algo, fix!
					if (!isalnum(c))
						return "non alnum in \"algo\"";
				}
				count++;
				if (count > 8)
					return ">8 chars in \"algo\" arg";
			}
			break;
		case 'b':		//  match "brr=[01]"
			c = *q++;
			if (!c)
				return "unexpected null after \"b\" query arg";
			if (c != 'r')
				return "unexpected char after \"r\" query arg";

			c = *q++;
			if (!c)
				return "unexpected null after \"br\" query arg";
			if (c != 'r')
				return "unexpected char after \"br\" query arg";

			c = *q++;
			if (c != '=')
				return "no char \"=\" after arg \"brr\"";
			
			//  at least one char
			c = *q++;
			if (!c || c == '&')
				return "missing file path after arg algo=";
			if (seen_algo)
				return "\"algo\" specified more than once";
			seen_algo = 1;

			//  skip over path till we hit end of string or '&'
			count = 0;
			while ((c = *q++)) {
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isascii(c))
					return "non ascii char in \"algo\" arg";
				if (count == 0) {
					if (!islower(c) || !isalpha(c))
						return "first char not "
						       "lower alpha in \"algo\""
						;
				} else {
					//  Note: wrong algo, fix!
					if (!isalnum(c))
						return "non alnum in \"algo\"";
				}
				count++;
				if (count > 8)
					return ">8 chars in \"algo\" arg";
			}
			break;
		case 'B':
			c = *q++;
			if (!c)
				return "unexpected null after \"B\" query arg";
			if (c != 'R')
				return "unexpected char after \"B\" query arg";
			c = *q++;
			if (c != '=')
				return "no char \"=\" after arg \"BR\"";
			if (seen_BR)
				return "arg \"BR\" given more than once";
			seen_BR = 1;
			
			//  skip over blobio root path till we hit end of string
			//  or '&'
			count = 0;
			while ((c = *q++)) {
				if (c == '&')
					break;
				if (!isgraph(c))
					return "non-graph char in \"BR\""
					       " query arg";
				count++;
				if (count >= PATH_MAX)
					return "too many chars in arg \"BR=\"";
			}
			if (count == 0)
				return "empty path in \"BR\" query arg";
			break;
		default:
			return "unexpected char in query arg";
		}
		if (c == 0)
			break;
		if (c == '&') {
			if (!*q)
				return "no query argument after \"&\"";
		}
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
