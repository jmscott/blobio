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
 *		BLOBIO_TMPDIRS=<algo>:<prefix>\t<path>\t<algo>:<prefix>\t<path>
 *		BLOBIO_TMPDIRS=sha:a\t
 *	
 *	The <path> must start with / .  If no match occurs, then $BLOBIO_TMPDIR
 *	will be tried, followed by $BLOBIO_ROOT/tmp.
 *  Note:
 *	This whole code is kind of a hack for casual users.
 *
 *	Only ascii file paths are understood, which is wrong.
 *
 *	Need to investigate/document if LVM can extend inodes dynamically.
 */
#include <stdlib.h>
#include "biod.h"

extern char *BLOBIO_ROOT;

static char *BLOBIO_TMPDIR = (char *)0;

struct tmp_path
{
	char	*algorithm;
	char	*digest_prefix;
	char	*path;
};

void
tmp_open()
{
	char *BLOBIO_TMPDIRS;
	char td[4096];

	int tab_count;
	char *p;

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
	strcpy(td, BLOBIO_TMPDIRS);

	//  verify an odd number of tab chars and replace \t char with null

	p = td;
	for (tab_count = 0;  p != NULL;  (p = strchr(p, '\t'))) {
		tab_count++;
		*p = 0;
	}
	if (tab_count == 0)
		panic2("no tabs in BLOBIO_TMPDIRS", BLOBIO_TMPDIRS);
	if (tab_count % 2 == 0)
		panic2("even number of tab chars in BLOBIO_TMPDIRS",
							BLOBIO_TMPDIRS);

	//  verify each algorithm exists

	p = td;
	while (*p) {
		char *q, algo[9], digest[129];

		q = strchr(p, ':');
		if (q == NULL)
			panic3("BLOBIO_TMPDIRS", "no colon after algorithm",td);
		if (q - p > 8)
			panic3("BLOBIO_TMPDIRS", "algorithm > 8 bytes", td);
		memcpy(algo, p, q - p);
		algo[q - p] = 0;
		if (module_get(algo) == NULL)
			panic3("BLOBIO_TMPDIRS", "unknown digest algorithm", 
							algo);
	}

	info2("BLOBIO_TMPDIRS", BLOBIO_TMPDIRS);
}
