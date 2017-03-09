/*
 *  Synopsis:
 *	Map a prefix of a digest string onto a temporary directory path.
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
 *	The enviroment variable BLOBIO_TMPDIR_MAP will map the prefix of the
 *	digest onto a temp directory path for use with the blob.  The syntax
 *	is
 *
 *		BLOBIO_TMPDIR_MAP=					\
 *			<algo>:<prefix>\t<path>\t<algo>:<prefix>\t<path>
 *		BLOBIO_TMPDIR_MAP=sha:a\t/var/local/blobio/data/a
 *	
 *	The <path> must start with / .  If no match occurs, then $BLOBIO_TMPDIR
 *	will be tried, followed by $BLOBIO_ROOT/tmp.
 *  Note:
 *	This whole code is kind of a hack for casual users.
 *
 *	Only ascii file paths are understood, which is wrong.
 *
 *	Need to investigate/document if LVM can extend inodes dynamically.
 *
 *	Need to verify that the <prefix> in BLOBIO_TMPDIR_MAP is correct
 *	for the algorithm.  In other words, need to add to module the callback
 *	is_prefix(prefix).
 */
#include <ctype.h>
#include <stdlib.h>

#include "biod.h"

#define _NULLC	(char *)0

extern char *BLOBIO_ROOT;

static char *TMPDIR = (char *)0;
static char *BLOBIO_TMPDIR = (char *)0;

struct tmp_map_entry
{
	char	algorithm[9];
	char	digest_prefix[129];
	char	path[256];
};

static struct tmp_map_entry *tmp_map;

static void
to_panic(char *msg)
{
	panic2("tmp_open", msg);
}

static void
to_panic2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	log_strcat2(buf, sizeof buf, msg1, msg2);
	to_panic(buf);
}

static void
to_info(char *msg)
{
	info2("tmp_open", msg);
}

static void
to_info2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	log_strcpy2(buf, sizeof buf, msg1, msg2);
	to_info(buf);
}

static void
td_panic(char *msg)
{
	to_panic2("BLOBIO_TMPDIR_MAP", msg);
}

static void
td_panic2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	log_strcpy2(buf, sizeof buf, msg1, msg2);

	td_panic(buf);
}

static void
tm_info(char *msg)
{
	to_info2("BLOBIO_TMPDIR_MAP", msg);
}

/*
 *  Parse env variables BLOBIO_TMPDIR_MAP and BLOBIO_TMPDIR
 */
void
tmp_open()
{
	char *BLOBIO_TMPDIR_MAP;
	char tm[4096];

	int tab_count, path_count, i;
	char *p;

	TMPDIR = getenv("TMPDIR");
	if (TMPDIR == NULL)
		to_info("environment variable not defined");
	else
		to_info2("env TMPDIR", TMPDIR);

	BLOBIO_TMPDIR = getenv("BLOBIO_TMPDIR");
	if (BLOBIO_TMPDIR == NULL)
		to_info("env BLOBIO_TMPDIR: not defined");
	else
		to_info2("BLOBIO_TMPDIR", BLOBIO_TMPDIR);

	BLOBIO_TMPDIR_MAP = getenv("BLOBIO_TMPDIR_MAP");
	if (BLOBIO_TMPDIR_MAP == NULL) {
		tm_info("not defined");
		return;
	}

	//  maximum length of BLOBIO_TMPDIR_MAP < 4096 (arbitrary).

	if (strlen(BLOBIO_TMPDIR_MAP) >= 4096)
		td_panic("env value >= 4096 bytes");
	info2("BLOBIO_TMPDIR_MAP", BLOBIO_TMPDIR_MAP);
	strcpy(tm, BLOBIO_TMPDIR_MAP);

	//  verify an odd number of tab chars and replace \t char with null

	p = tm;
	for (tab_count = 0;  p != NULL;  (p = strchr(p, '\t'))) {
		tab_count++;
		*p = 0;
	}
	if (tab_count == 0)
		td_panic2("no tabs in value", tm);
	if (tab_count % 2 == 0)
		td_panic2("even number of tab chars", tm);

	//  verify that each algorithm exists and the digest is well formed.
	//  replace ':' with null.

	p = tm;
	while (*p) {
		char *q;

		//  find colon that terminates <algorithm>
		q = strchr(p, ':');
		if (q == NULL)
			td_panic2("no colon after algorithm", tm);
		if (q - p > 8)
			td_panic2("algorithm > 8 bytes", tm);

		// replace colon with null

		*q++ = 0;

		if (module_get(p) == NULL)
			td_panic2("unknown digest algorithm", p);

		/*
		 *  insure prefix is <= 128 bytes.
		 */
		if (strlen(q) > 128)
			td_panic2("digest prefix > 128 bytes", q);

		/*
		 *  insure each character in digest prefix is graphable.
		 *
		 *  Note:
		 *	What about isascii()?
		 */
		while (*q) {
			char c, b[2];

			c = *q++;

			if (isgraph(c))
				continue;
			b[0] = c;
			b[1] = 0;
			td_panic2("digest prefix contains no-graphic char", b);
		}

		p = q;
	}

	/*
	 *  In the copy of BLOBIO_TMPDIR_MAP both \t and colon have been
	 *  replace with null strings.
	 *
	 *	<algo>\0<prefix1>\0<path1>\0<algo>\0<prefix2>\0<path2> ...
	 *
	 *  Build the map of <algo> <prefix> to <path>.
	 */
	path_count = (tab_count + 1) / 2;
	tmp_map = (struct tmp_map_entry *)malloc(
					sizeof *tmp_map * path_count + 1);
	if (tmp_map == NULL)
		td_panic("malloc(tmp_map) failed");

	//  null terminate the algorithm of final entry

	tmp_map[path_count].algorithm[0] = 0;

	//  build the full tmp_map by walking through the null terminated
	//  values of $BLOBIO_TMPDIR_MAP

	p = tm;
	i = 0;
	while (*p) {
		strcpy(tmp_map[i].algorithm, p);
		p += strlen(p);

		strcpy(tmp_map[i].digest_prefix, p);
		p += strlen(p);

		strcpy(tmp_map[i].path, p);
		p += strlen(p);

		i++;
	}
	if (i != path_count)
		td_panic("make tmp_map: count != path_count");
}
