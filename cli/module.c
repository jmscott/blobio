/*
 *  Synopsis:
 *	UDIG Client Module Table.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#include "blobio.h"

#ifdef SHA_FS_MODULE
extern struct module	sha_module;
#endif

#ifdef SK_FS_MODULE
extern struct module	sk_module;
#endif

struct module *modules[] =
{
#ifdef SHA_FS_MODULE
	&sha_module,
#endif

#ifdef SK_FS_MODULE
	&sk_module,
#endif
	(struct module *)0
};
