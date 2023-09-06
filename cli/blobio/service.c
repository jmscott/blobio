/*
 *  Synopsis:
 *	Blob Service Module
 */
#include "blobio.h"

extern struct service	bio4_service;
extern struct service	fs_service;

struct service *services[] =
{
	&bio4_service,
	&fs_service,
	(struct service *)0
};
