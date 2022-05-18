/*
 *  Synopsis:
 *	Frisk and extract name=val query args from a query of service url
 */
#include "blobio.h"

/*
 *  Frisk query path of BLOBIO_SERVICE.
 */
char *
uri_frisk_BLOBIO_SERVICE_query(char *query)
{
	char *q = query, c;

	while ((c = *q++)) {
		if (c == 't') {
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
			if (*q && *q != '&')
				return "no & char after tmp query arg";
		}
	}
	return "unknown query arg";
}
