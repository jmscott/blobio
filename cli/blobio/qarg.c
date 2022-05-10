/*
 *  Synopsis:
 *	Index the "name=val" args of a uri query frag into an array of offsets.
 *  Usage:
 *	char *err;
 *
 *	//  index[0] == number of indexed qargs
 *
 *	int qindex[1 + BLOBIO_MAX_URI_QARGS * 2] = {};
 *	if ((err = uri_index_query_arg("tmo=20&trust=fs", qindex)))
 *		return err;
 *	int i;
 *	for (i = 0;  i < BLOBIO_MAX_URI_QARGS;  i++) {
 *		if (qindex[i] == 0)
 *			break;
 *		//  process qarg
 *	}
 *  Note:
 *	Upon error, index[0] to point to offending qarg!
 */
#include "blobio.h"

char *parse_tmo(char *tmo)
{
	(void)tmo;
	return "tmo not implemented";
}

struct uri_query_arg_parse {
	char	*name;
	char    *(*frisk)(char *value);
} uri_query_args[] = {

	{"tmo",	parse_tmo},
	{0}
};

char *
uri_index_qargs(char *frag, int index[])
{
	int nargs = 0;
	char *f = frag;
	int *ip = index + 1;
	char *n = 0, *v = 0;
	
	index[0] = -1;
	if (nargs + 1 >= BLOBIO_MAX_URI_QARGS)
		return "too many query args";

	char c = *f++;

	//  update index with the qarg just parsed
	if (!c || c == '&') {
		if (*f == '&')
			return "empty query arg"; 
		if (!*f)
			return "final query arg is empty";
		*ip++ = n - frag;
		*ip++ = v - frag;
	}
	return "unknown query arg";
}
