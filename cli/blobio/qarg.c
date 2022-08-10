/*
 *  Synopsis:
 *	Frisk and extract query from uri: {tmo,brr}={\d{1,3}|<path>}
 *  Note:
 *	No escaping in query args, which is a serious bug!
 */
#include <ctype.h>
#include <stdlib.h>

#include "blobio.h"

/*
 *  Synopsis:
 *	Frisk (but not extract) query args in a uri of $BLOBIO_SERVICE.
 *  Description:
 *	Brute force frisk (but not extract) of query args in a uri
 *	$BLOBIO_SERVICE.
 *
 *		tmo=20			#  timeout in seconds <= 255
 *		brr=path/to/brr		#  path to brr log
 *
 *	Tis an error if args other than "tmo" or "brr" exist.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_query(char *query)
{
	char *q = query, c;
	char seen_tmo = 0, seen_brrp = 0, seen_algo = 0;
	int count;

	if (!query || !*query)
		return (char *)0;
	while ((c = *q++)) {
		switch (c) {
		case 'a':		//  match "algo"
			c = *q++;
			if (!c)
				return "unexpected null after \"a\" query arg";
			if (c != 'l')
				return "unexpected char after \"a\" query arg";

			c = *q++;
			if (!c)
				return "unexpected null after \"al\" query arg";
			if (c != 'g')
				return "unexpected char after \"al\" query arg";

			c = *q++;
			if (!c)
				return "unexpected null after "
				       "\"alg\" query arg";
			if (c != 'o')
				return "unexpected char after "
				       "\"alg\" query arg";

			c = *q++;
			if (c != '=')
				return "no char '=' after arg \"brr\"";
			
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
		case 'b':		//  match "brrp"
			c = *q++;
			if (!c)
				return "unexpected null after \"b\"";
			if (c != 'r')
				return "unexpected char after \"b\"";
			c = *q++;
			if (!c)
				return "unexpected null after \"br\"";
			if (c != 'r')
				return "unexpected char after \"br\"";
			c = *q++;
			if (!c)
				return "unexpected null after \"brr\"";
			if (c != 'p')
				return "unexpected char after \"brr\"";
			c = *q++;
			if (c != '=')
				return "no char '=' after arg \"brrp\"";
			
			//  at least one char
			c = *q++;
			if (!c || c == '&')
				return "missing file path after arg brr=";
			if (seen_brrp)
				return "\"brrp\" specified more than once";
			seen_brrp = 1;

			//  skip over path till we hit end of string or '&'
			count = 0;
			while ((c = *q++)) {
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isascii(c))
					return "non ascii char in \"brrp\" arg";
				if (!isgraph(c))
					return "non graph char in \"brrp\" arg";
				count++;
				if (count >= 64)
					return "=>64 chars in \"brrp\" arg";
			}
			break;
		case 't':		//  match "tmo"
			c = *q++;
			if (!c)
				return "unexpected null after 't' query arg";
			if (c != 'm')
				return "unexpected char after 't' query arg";
			c = *q++;
			if (!c)
				return "unexpected null after 'tm' query arg";
			if (c != 'o')
				return "unexpected char after 'tm' query arg";
			c = *q++;
			if (c != '=')
				return "no char '=' after arg 'tmo'";
			if (seen_tmo)
				return "arg 'tmo' given more than once";
			seen_tmo = 1;
			
			//  skip over timeout till we hit end of string or '&'
			count = 0;
			while ((c = *q++)) {
				if (c == '&')
					break;
				count++;
				if (count >= 4)
					return "too many chars in arg \"tmo=\"";
				if (!isdigit(c))
					return "non-digit in arg \"tmo=\"";
			}
			if (count == 0)
				return "tmo: no digits";
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
 *  	Get the brr path in the query portion of $BLOBIO_SERCICE
 *  Usage:
 *	char brrp[64];
 *	brrp[0] = 0;
 *	err = BLOBIO_SERVICE_get_brrp(query, char *brrp);
 *	if (err)
 *		die2("error parsing \"brrp\" for path", err);
 */
char *
BLOBIO_SERVICE_get_brrp(char *query, char *path)
{
	char *q = query, c, *p = path;
	
	*p = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "brrp", so skip to next '&' or end of string 
		if (c != 'b') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		/*
		 *  extract file name matching [[:ascii:]]+ and
		 *  [[:graph:]]+ and {1,64} and [^&].
		 */
		c = *q++;
		if (c != 'r')
			return "impossible: char 'r' not after char 'b'";
		c = *q++;
		if (c != 'r')
			return "impossible: char 'r' not after \"br\"";
		c = *q++;
		if (c != 'p')
			return "impossible: char 'p' not after \"brr\"";
		c = *q++;
		if (c != '=')
			return "impossible: char '=' not after \"brrp\"";
		while ((c = *q++) && c != '&')
			*p++ = c;
		*p = 0;
	}
	/*
	 *  brr path can not end with ".brr".
	 */
	if (p - 4>path && p[-4]=='.' && p[-3]=='b' && p[-2]=='r' && p[-1]=='r')
		return "brr path ends with \".brr\"";
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
