/*
 *  Synopsis:
 *	Map a digest string onto a temporary directory path.
 *  Description:
 *	A "put" or "give" request first copies the blob into $BLOBIO_ROOT/tmp
 *	and then atomically rename()'s the tmp blob to it's final resting
 *	place in the file system.  Unfortunately this algorithm implies
 *	that the temp directory be on the same device as the final home for
 *	home for the blob, since a typical unix file system forbids renaming()
 *	across file systems. Atomically rename()'ing the blob is critical to the
 *	correctness of the blob tree. A partially copied blob could be
 *	interupted with a sig 9, for example.
 *
 *	The enviroment variable BLOBIO_TMPDIRS will map the prefix of the
 *	digest onto a temp directory path for use with the blob.  The syntax
 *	is
 *
 *		BLOBIO_TMPDIRS=<prefix>\t<path>\t<prefix>\t<path>
 *	
 *	The <path> must start with / .  If no match occurs, then $BLOBIO_TMPDIR
 *	will be tried, followed by $BLOBIO_ROOT/tmp.
 *  Note:
 *	This whole code is kind of a hack for casual users.
 *
 *	Need to investigate/document if LLVM can extend inodes dynamically.
 */
#include <stdlib.h>
#include "biod.h"

extern char *BLOBIO_ROOT;

static char *BLOBIO_TMPDIR = (char *)0;
static char *BLOBIO_TMPDIRS = (char *)0;

void
tmp_open()
{
	BLOBIO_TMPDIR = getenv("BLOBIO_TMPDIR");
	if (BLOBIO_TMPDIR == NULL)
		info("env BLOBIO_TMPDIR: not defined");
	else
		info2("BLOBIO_TMPDIR", BLOBIO_TMPDIR);

	BLOBIO_TMPDIRS = getenv("BLOBIO_TMPDIRS");
	if (BLOBIO_TMPDIRS == NULL) {
		info("env BLOBIO_TMPDIRS: not defined");
		return;
	}

	//  maximum length of BLOBIO_TMPDIRS < 4096 (arbitrary).

	if (strlen(BLOBIO_TMPDIRS) >= 4096)
		panic("env variable BLOBIO_TMPDIRS >= 4096 bytes");
	info2("BLOBIO_TMPDIRS", BLOBIO_TMPDIRS);
}
