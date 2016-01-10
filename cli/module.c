/*
 *  Synopsis:
 *	UDIG Client Module Table.
 *  Blame:
 *	jmscott@setspace.com
 */
#include "blobio.h"

#ifdef SHA_FS_MODULE
extern struct module	sha_module;
#endif

struct module *modules[] =
{
#ifdef SHA_FS_MODULE
	&sha_module,
#endif
	(struct module *)0
};
