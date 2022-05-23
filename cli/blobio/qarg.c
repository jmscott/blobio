/*
 *  Synopsis:
 *	Frisk and extract name=val query args from a query of service url
 */
#include "blobio.h"

/*
 *  Frisk query args of uri BLOBIO_SERVICE.
 *
 *	tmo=20			#  timeout in seconds <= 255
 *	brr=<path>		#  file path to write blob request record
 *	tru=[fs|net=cidr]	#  who to trust (no digest check) during request
 *
 *  returning an error string or (char *) if passes.
 */
char *
uri_frisk_BLOBIO_SERVICE_query_args(char *query)
{
	char *q = query, c;

	while ((c = *q++)) {
		switch (c) {
		case 'b':		//  match "brr"
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
			break;
		case 't':		//  match "tmo"
			c = *q++;
			if (!c)
				return "unexpected null after 't' query arg";
			switch (c) {
			case 'm':
				c = *q++;
				if (!c)
					return "unexpected null after 'tm'"
					       "query arg";
				if (c != 'o')
					return "unexpected char after 'tm'"
					       "query arg";
				break;
			case 'r':
				c = *q++;
				if (!c)
					return "unexpected null after 'tr' "
					       "query arg";
				if (c != 'u')
					return "unexpected char after 'tr' "
					       "query arg";
				break;
			default:
				return "unexpected char after 't' query arg";
			}
			break;
		}
		if (*q && *q != '&')
			return "did not see expected & character";
	}
	return (char *)0;
}

/*
 *  Extract a timeout value from a frisked query arg string.
 */
char *
extract_query_arg_tmo(char **p_query, unsigned int tmo)
{
	(void)p_query;
	(void)tmo;

	return (char *)0;
}
