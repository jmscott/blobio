/*
 *  Synopsis:
 *	Blob Service Module
 *  Blame:
 *	jmscott@setspace.com
 */
#include "blobio.h"

extern struct service	bio4_service;

struct service *services[] =
{
	&bio4_service,
	(struct service *)0
};
