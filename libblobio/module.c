/*
 *  Synopsis:
 *	All digest modules.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include "libblobio.h"

extern struct blobio_digest_module blobio_sha1_module;

static struct blobio_digest_module *module[] =
{
	&blobio_sha1_module,
	0
};

struct blobio_digest_module *
blobio_digest_module_get(char *algorithm)
{
	int i;

	for (i = 0;  module[i];  i++)
		if (strcmp(module[i]->algorithm, algorithm) == 0)
			return module[i];
	return (struct blobio_digest_module *)0;
}
