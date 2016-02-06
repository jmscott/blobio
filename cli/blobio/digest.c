/*
 *  Synopsis:
 *	Hash Digest Modules
 *  Blame:
 *	jmscott@setspace.com
 */
#include "blobio.h"

extern struct digest	sha_digest;

struct digest *digests[] =
{
	&sha_digest,
	(struct digest *)0
};
