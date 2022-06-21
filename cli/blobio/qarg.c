/*
 *  Synopsis:
 *	Frisk and extract query from uri: {tmo,brr}={\d{1,3}|<path>}
 */
#include <ctype.h>
#include <stdlib.h>

#include "blobio.h"

/*
 *  Synopsis:
 *	Syntaxtically Frisk (but not extract) query args in a uri of $BLOBIO_SERVICE.
 *  Description:
 *	Brute force frisk (but not extract) of query args in a uri
 *	$BLOBIO_SERVICE.
 *
 *		tmo=20			#  timeout in seconds <= 255
 *		brr=<file name>		#  path to brr log at spool/<name>.brr
 *
 *	Tis an error if args other than "tmo" or "brr" exist.
 *  Returns:
 *	An error string or (char *)0 if no unexpected args exist.
 */
char *
BLOBIO_SERVICE_frisk_query(char *query)
{
	char *q = query, c;

	if (!query || *query)
		return (char *)0;
	while ((c = *q++)) {
		if (c == '&')
			continue;
		switch (c) {
		case 'b':		//  match "brr"
			c = *q++;
			if (!c)
				return "unexpected null after 'b' query arg";
			if (c != 'r')
				return "unexpected char after 'b' query arg";
			c = *q++;
			if (!c)
				return "unexpected null after 'br' query arg";
			if (c != 'r')
				return "unexpected char after 'br' query arg";
			c = *q++;
			if (c != '=')
				return "no char '=' after arg 'brr'";
			
			//  at least one char
			c = *q++;
			if (!c || c == '&')
				return "missing file path after arg brr=";

			//  skip over file name till we hit end of string or '&'
			int cnt = 0;
			while ((c = *q++)) {
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isascii(c))
					return "non ascii char in \"brr\" arg";
				if (!isgraph(c))
					return "non graph char in \"brr\" arg";
				cnt++;
				if (cnt > 31)
					return ">31 chars in \"brr\" arg";
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
			
			//  at least one digit
			c = *q++;
			if (!c)
				return "unexpected null in \"tmo=\"";
			if (!isdigit(c))
				return "first char not digit in arg \"tmo=\""; 

			//  skip over path till we hit end of string or '&'
			int count = 0;
			while ((c = *q++)) {
				if (count++ > 3)
					return "too many chars in arg \"tmo=\"";
				if (!c)
					return (char *)0;
				if (c == '&')
					break;
				if (!isdigit(c))
					return "non-digit in arg \"tmo=\"";
			}
			break;
		default:
			return "unexpected char in query arg";
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
 *  	Get glob request file name a frisked $BLOBIO_SERVICE string
 *  Usage:
 *	char *status;
 *	status =  BLOBIO_SERVICE_frisk_query(query);
 *	if (status)
 *		return status;		//  error in a query arg
 *
 *	...
 *
 *	char brr_file_name[32];
 *	file_name[0] = 0;
 *	status = BLOBIO_SERVICE_get_brr_file_name(query, char file_name);
 *	if (status)
 *		die2("error parsing \"brr\" query arg", status);
 */
char *
BLOBIO_SERVICE_get_brr_file_name(char *query, char *file_name)
{
	char *q = query, c;

	*file_name = 0;
	while ((c = *q++)) {
		if (c == '&')
			continue;

		//  not arg "brr", so skip to next '&' or end of string 
		if (c != 'b') {
			while ((c = *q++) && c != '&')
				;
			if (!c)
				return (char *)0;
			continue;
		}
		/*
		 *  extract file name matching [[:ascii:]]+ and
		 *  [[:graph:]]+ and {1,31} and [^&].
		 */
		c = *q++;
		if (c != 'r')
			return "impossible: char 'r' not after char 'b'";
		c = *q++;
		if (c != 'r')
			return "impossible: char 'r' not after \"br\"";
		c = *q++;
		if (c != '=')
			return "impossible: char '=' not after \"brr\"";
	}

	return (char *)0;
}
