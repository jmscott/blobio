/*
 *  Synopsis:
 *	blk_SHA1 client digest module interface routines.
 *  Note:
 *  	Why does sha_request exist at all?
 */
#ifdef SHA_FS_MODULE    

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../sha1/sha1.h"
#include "blobio.h"

#ifdef COMPILE_TRACE

#define _TRACE(msg)	if (tracing) _trace(msg)

#else

#define _TRACE(msg)

#endif

extern char	*verb;
extern char	digest[];
extern int	input_fd;

static unsigned char	bin_digest[20];
static blk_SHA_CTX	sha_ctx;

static char	empty[]		= "da39a3ee5e6b4b0d3255bfef95601890afd80709";

static char *
sha_init()
{
	char *d40, c;
	unsigned char *d20;
	unsigned int i;
	static char nm[] = "sha: init";

	if (strcmp("roll", verb)) {
		blk_SHA1_Init(&sha_ctx);

		/*
		 *  Convert the 40 character hex signature to 20 byte binary.
		 *  The ascii, hex version has already been scanned for 
		 *  syntactic correctness.
		 */
		if (digest[0]) {
			d40 = digest;
			d20 = bin_digest;

			memset(d20, 0, 20);	/*  since nibs are or'ed in */
			for (i = 0;  i < 40;  i++) {
				unsigned char nib;

				c = *d40++;
				if (c >= '0' && c <= '9')
					nib = c - '0';
				else
					nib = c - 'a' + 10;
				if ((i & 1) == 0)
					nib <<= 4;
				d20[i >> 1] |= nib;
			}
#ifdef COMPILE_TRACE
			if (tracing) {
				trace2(nm, "hex dump of 20 byte digest:");
				hexdump(d20, 20, '=');
			}
#endif
		}
	}
	return (char *)0;
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 *  Return 0 if digest matches requested digest;  otherwise return 1.
 */
static int
chew(unsigned char *chunk, int size)
{
	static char nm[] = "sha: chew";

	TRACE2(nm, "request to chew()");

	/*
	 *  Incremental digest.
	 */
	unsigned char tmp_digest[20];
	blk_SHA_CTX tmp_ctx;

	blk_SHA1_Update(&sha_ctx, chunk, size);
	/*
	 *  Copy current digest state to a temporary state,
	 *  finalize and then compare to expected state.
	 */
	tmp_ctx = sha_ctx;
	blk_SHA1_Final(tmp_digest, &tmp_ctx);

#ifdef COMPILE_TRACE
	if (tracing) {
		trace2(nm, "hex dump of 20 byte digest follows ...");
		hexdump(tmp_digest, 20, '=');
		trace2(nm, "chew() done");
	}
#endif
	return memcmp(tmp_digest, bin_digest, 20) == 0 ? 0 : 1;
}

/*
 *  Synopsis:
 *	Update the partial digest with a chunk read from a get request.
 *  Returns:
 *	-1	invalid signature
 *	0	ok, no more to read, blob seen
 *	1	more to read, continue, blob not seen
 */
static int
sha_get_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
sha_take_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
sha_took(char *reply)
{
	(void)reply;
	return 0;
}

/*
 *  Synopsis:
 *	Update the digest with a chunk from in a put request.
 *  Returns:
 *	0	id blob finished
 *	1	continue putting blob.
 *	-1	chunk not part of the blob.
 */
static int
sha_put_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
sha_give_update(unsigned char *src, int src_size)
{
	return sha_put_update(src, src_size);
}

static int
sha_gave(char *reply)
{
	UNUSED_ARG(reply);
	return 0;
}

static int
sha_close()
{
	return 0;
}

/*
 *  blk_SHA1 digest is 40 characters of 0-9 or a-f.
 */
static int
sha_syntax()
{
	char *p, c;

	p = digest;
	while ((c = *p++)) {
		if (p - digest > 40)
			return 0;
		if (('0' > c||c > '9') && ('a' > c||c > 'f'))
			return 0;
	}
	return p - digest == 41 ? (strcmp(digest, empty) == 0 ? 2 : 1) : 0;
}

/*
 *  Is the digest == to well-known empty digest
 *  da39a3ee5e6b4b0d3255bfef95601890afd80709 
 */
static int
sha_empty()
{
	return strcmp("da39a3ee5e6b4b0d3255bfef95601890afd80709", digest) == 0?
						1 : 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static void
_trace(char *msg)
{
	trace2("sha", msg);
}

/*
 *  Digest data on input and update the global digest[129] array.
 */
static char *
sha_eat_input()
{
	unsigned char buf[PIPE_MAX], *q, *q_end;
	char *p;
	int nread;

	_TRACE("request to sha_eat_input()");

	while ((nread = uni_read(input_fd, buf, sizeof buf)) > 0)
		blk_SHA1_Update(&sha_ctx, buf, nread);
	if (nread < 0)
		return strerror(errno);
	blk_SHA1_Final(bin_digest, &sha_ctx);

	p = digest;
	q = bin_digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;

	_TRACE("sha_eat_input() done");
	return (char *)0;
}

/*
 *  Convert an ascii digest to a file system path.
 */
static char *
sha_fs_path(char *file_path, int size)
{
	char *dp, *fp;

	if (size < 47)
		return "file path size too small: size < 47 bytes";

	dp = digest;
	fp = file_path;

	*fp++ = '/';
	*fp++ = *dp++;

	*fp++ = '/';
	*fp++ = *dp++; *fp++ = *dp++;

	*fp++ = '/';
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;

	*fp++ = '/';
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;

	*fp++ = '/';
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;

	*fp++ = '/';
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++; *fp++ = *dp++;
	*fp++ = *dp++;
	*fp = 0;

	return (char *)0;
}

static int
_mkdir(char *path, int len)
{
	path[len] = 0;
	if (mkdir(path, 0777) == 0 || errno == EEXIST)
		return 0;
	return -1;
}

/*
 *  Make the directory path to a file system blob an append to full_path.
 */
static char *
sha_fs_mkdir(char *root)
{
	char path[PATH_MAX];
	char *dp, *dirp;

	dp = digest;
	*path = 0;

	dirp = bufcat(path, sizeof path, root);
	*dirp++ = '/';

	//  first level directory: one char

	*dirp++ = *dp++;
	if (_mkdir(path, dirp - path))
		return strerror(errno);
	*dirp++ = '/';

	//  second level directory: two chars

	*dirp++ = *dp++; *dirp++ = *dp++;
	if (_mkdir(path, dirp - path))
		return strerror(errno);
	*dirp++ = '/';

	//  third level directory: four chars

	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	if (_mkdir(path, dirp - path))
		return strerror(errno);
	*dirp++ = '/';

	//  fourth level directory: eight chars

	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	if (_mkdir(path, dirp - path))
		return strerror(errno);
	*dirp++ = '/';

	//  fifth level directory: 16 chars
	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	*dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++; *dirp++ = *dp++;
	if (_mkdir(path, dirp - path))
		return strerror(errno);

	return (char *)0;
}

struct digest	sha_digest =
{
	.algorithm	=	"sha",

	.init		=	sha_init,
	.get_update	=	sha_get_update,
	.take_update	=	sha_take_update,
	.took		=	sha_took,
	.put_update	=	sha_put_update,
	.give_update	=	sha_give_update,
	.gave		=	sha_gave,
	.close		=	sha_close,

	.eat_input	=	sha_eat_input,

	.syntax		=	sha_syntax,
	.empty		=	sha_empty,

	.fs_path	=	sha_fs_path,
	.fs_mkdir	=	sha_fs_mkdir
};

#endif
