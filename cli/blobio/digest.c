/*
 *  Synopsis:
 *	Hooks for all Hash Digest Modules
 *  Note:
 *	Why are the digest #defines prefixed with FS_ ?
 *	What about network access?
 */
#include <string.h>
#include <errno.h>
#include "blobio.h"

extern struct digest	*digest_module;
extern int		errno;

#ifdef FS_SHA_MODULE
extern struct digest	sha_digest;
#endif

#ifdef FS_BC160_MODULE
extern struct digest	bc160_digest;
#endif

#ifdef FS_BTC20_MODULE
extern struct digest	btc20_digest;
#endif

struct digest *digests[] =
{
#ifdef FS_BTC20_MODULE
	&btc20_digest,
#endif

#ifdef FS_SHA_MODULE
	&sha_digest,
#endif

#ifdef FS_BC160_MODULE
	&bc160_digest,
#endif
	(struct digest *)0
};
