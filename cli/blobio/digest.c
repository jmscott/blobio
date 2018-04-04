/*
 *  Synopsis:
 *	Hash Digest Modules
 */
#include "blobio.h"

#ifdef SHA_FS_MODULE
extern struct digest	sha_digest;
#endif

#ifdef BC160_FS_MODULE
extern struct digest	bc160_digest;
#endif

struct digest *digests[] =
{
#ifdef SHA_FS_MODULE
	&sha_digest,
#endif

#ifdef BC160_FS_MODULE
	&bc160_digest,
#endif

	(struct digest *)0
};
