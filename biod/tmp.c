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
 *	The tmp.c code is kind of a hack for casual users who don't use
 *	volume mangers.
 *
 *	Only ascii file paths are understood, which is wrong.
 *
 *	Need to investigate/document if LVM can extend inodes dynamically.
 *
 *	Need to verify that the <prefix> in BLOBIO_TMPDIR_MAP is correct
 *	for the algorithm.  In other words, need to add to module the callback
 *	is_prefix(prefix).
 *
 *	The TMPDIR variable is ignored and defaults to $BLOBIO_ROOT.  Why? 
 *
 *	Also, probably need cross volume tests on startup.
 */
#include <ctype.h>
#include <stdlib.h>

#include "biod.h"

#define _NULLC	(char *)0

extern char *BLOBIO_ROOT;

static char *BLOBIO_TMPDIR = (char *)0;

struct tmp_map_entry
{
	char	digest_algorithm[9];
	char	digest_prefix[129];
	size_t	prefix_len;
	char	path[97];		//  limited by arborist message size
};

static struct tmp_map_entry *tmp_map = 0;
static char *default_path = 0;

static void
_info(char *msg)
{
	info2("tmp_open", msg);
}

static void
_info2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	log_strcpy2(buf, sizeof buf, msg1, msg2);
	_info(buf);
}

static void
_panic(char *msg)
{
	panic2("tmp_open", msg);
}

static void
_panic2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	log_strcpy2(buf, sizeof buf, msg1, msg2);

	_panic(buf);
}

//  make the table to map the digest prefix onto a temp directory

static void
make_map()
{
	char *BLOBIO_TMPDIR_MAP;
	char tm[4096], buf[MSG_SIZE];

	int tab_count = 0, path_count = 0, i;
	char *p, *p_end;

	BLOBIO_TMPDIR_MAP = getenv("BLOBIO_TMPDIR_MAP");
	if (BLOBIO_TMPDIR_MAP == NULL) {
		_info("not defined");
		return;
	}

	//  maximum length of BLOBIO_TMPDIR_MAP < 4096 (arbitrary).

	if (strlen(BLOBIO_TMPDIR_MAP) >= 4096)
		_panic("env value >= 4096 bytes");
	_info2("BLOBIO_TMPDIR_MAP", BLOBIO_TMPDIR_MAP);
	strcpy(tm, BLOBIO_TMPDIR_MAP);

	//  verify an odd number of tab chars and replace \t char with null

	p = tm;
	while (p != NULL)
		if ((p = strchr(p, '\t'))) {
			tab_count++;
			*p++ = 0;
		}
	if (tab_count == 0)
		_panic2("no tabs in $BLOBIO_TMPDIR_MAP", tm);
	if (tab_count % 2 == 0)
		_panic2("even number of tab chars in $BLOBIO_TMPDIR_MAP", tm);

	//  verify that each algorithm exists and the digest is well formed.
	//  replace ':' with null.

	p = tm;
	p_end = tm + strlen(BLOBIO_TMPDIR_MAP);
	while (p < p_end) {
		char *q;

		//  find colon that terminates <algorithm>
		q = strchr(p, ':');
		if (q == NULL)
			_panic2("no colon after digest algorithm", tm);
		if (q - p > 8)
			_panic2("digest algorithm > 8 bytes", tm);

		// replace colon with null

		*q++ = 0;

		if (module_get(p) == NULL)
			_panic2("unknown digest algorithm", p);

		if (strlen(q) == 0)
			_panic("digest prefix == 0");
		/*
		 *  insure prefix is <= 128 bytes.
		 */
		if (strlen(q) > 128)
			_panic2("digest prefix > 128 bytes", q);

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
			_panic2("digest prefix contains no-graphic char", b);
		}

		p = q;
	}

	/*
	 *  In the copy of BLOBIO_TMPDIR_MAP (var tm) both \t and colon have been
	 *  replace with null strings.
	 *
	 *	<algo>\0<prefix1>\0<path1>\0<algo>\0<prefix2>\0<path2> ...
	 *
	 *  Build the map of <algo> <prefix> to <path>.
	 */
	path_count = (tab_count + 1) / 2;
	tmp_map = (struct tmp_map_entry *)malloc(
					sizeof *tmp_map * (path_count + 1));
	if (tmp_map == NULL)
		_panic("malloc(tmp_map) failed");

	//  null terminate the algorithm of final entry

	tmp_map[path_count].digest_algorithm[0] = 0;

	//  build the full tmp_map by walking through the null terminated
	//  values of $BLOBIO_TMPDIR_MAP

	p = tm;
	i = 0;
	while (p < p_end) {
		strcpy(tmp_map[i].digest_algorithm, p);
		p += strlen(p) + 1;

		strcpy(tmp_map[i].digest_prefix, p);
		p += strlen(p) + 1;

		if (strlen(tmp_map[i].path) > 96)
			_panic2("path > 96 characters", tmp_map[i].path);
		strcpy(tmp_map[i].path, p);
		p += strlen(p) + 1;

		tmp_map[i].prefix_len = strlen(tmp_map[i].digest_prefix);

		i++;
	}
	if (i != path_count)
		_panic("count != path_count");
	snprintf(buf, sizeof buf, "putting %d temp maps", path_count);
	_info(buf);
	for (i = 0;  i < path_count;  i++) {
		_info2("	digest algorithm", tmp_map[i].digest_algorithm);
		_info2("	digest prefix", tmp_map[i].digest_prefix);
		_info2("	tmp path", tmp_map[i].path);
	}
}

/*
 *  Parse env variables BLOBIO_TMPDIR_MAP and BLOBIO_TMPDIR
 */
void
tmp_open()
{
	make_map();

	BLOBIO_TMPDIR = getenv("BLOBIO_TMPDIR");
	if (BLOBIO_TMPDIR == NULL) {
		_info("env BLOBIO_TMPDIR: not defined");
		default_path = "tmp";
	} else {
		default_path = BLOBIO_TMPDIR;
		_info2("BLOBIO_TMPDIR", BLOBIO_TMPDIR);
	}
	_info2("default temp path", default_path);
	if (!io_path_exists(default_path))
		_panic2("tmp path does not exist", default_path);
}

static int
tmp_cmp(struct tmp_map_entry *tm, char *digest_algorithm, char *digest_prefix)
{
	return strcmp(digest_algorithm, tm->digest_algorithm) == 0 &&
	       strncmp(digest_prefix, tm->digest_prefix, tm->prefix_len) == 0
	;
}

char *
tmp_get(char *digest_algorithm, char *digest_prefix)
{
	struct tmp_map_entry *tm;

	if ((tm = tmp_map))
		while (tm->digest_algorithm[0]) {
			if (tmp_cmp(tm, digest_algorithm, digest_prefix))
				return tm->path;
			tm++;
		}
	return default_path;
}
