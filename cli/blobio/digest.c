/*
 *  Synopsis:
 *	Hash Digest Modules
 */
#include "blobio.h"

extern struct digest	sha_digest;
extern struct digest	bch_digest;

struct digest *digests[] =
{
	&sha_digest,
	&bch_digest,
	(struct digest *)0
};
