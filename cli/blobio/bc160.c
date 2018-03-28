/*
 *  Synopsis:
 *	Bitcoin Hash RIPEMD160(SHA256(blob))
 */
#ifdef BC160_FS_MODULE    

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "openssl/sha.h"
#include "openssl/ripemd.h"
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

typedef struct
{
	RIPEMD160_CTX	ripemd_ctx;
	SHA256_CTX	sha_ctx;
} BC160_CTX;

static BC160_CTX	bc160_ctx;

static char	empty[]		= "b472a266d0bd89c13706a4132ccfb16f7c3b9fcb";

static void
BC160_Init(BC160_CTX *ctx)
{
	SHA256_Init(&ctx->sha_ctx);
	RIPEMD160_Init(&ctx->ripemd_ctx);
}

static char *
bc160_init()
{
	char *d40, c;
	unsigned char *d20;
	unsigned int i;
	static char nm[] = "bc160: init";

	if (strcmp("roll", verb)) {
		BC160_Init(&bc160_ctx);

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

static void
BC160_Update(BC160_CTX *ctx, unsigned char *chunk, int size)
{
	SHA256_CTX tmp_ctx;
	unsigned char tmp_digest[32];

	SHA256_Update(&ctx->sha_ctx, chunk, size);

	tmp_ctx = ctx->sha_ctx;
	SHA256_Final(tmp_digest, &tmp_ctx);

	RIPEMD160_Update(&ctx->ripemd_ctx, tmp_digest, (unsigned long)32);
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 *  Return 0 if digest matches requested digest;  otherwise return 1.
 */
static int
chew(unsigned char *chunk, int size)
{
	static char nm[] = "bc160: chew";

	TRACE2(nm, "request to chew()");

	/*
	 *  Incremental ripemd160 digest.
	 */
	unsigned char tmp_digest[20];
	RIPEMD160_CTX tmp_ctx;

	BC160_Update(&bc160_ctx, chunk, size);
	/*
	 *  Copy current ripemd digest state to a temporary state,
	 *  finalize and then compare to expected state.
	 */
	tmp_ctx = bc160_ctx.ripemd_ctx;
	RIPEMD160_Final(tmp_digest, &tmp_ctx);

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
bc160_get_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
bc160_take_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
bc160_took(char *reply)
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
bc160_put_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static int
bc160_give_update(unsigned char *src, int src_size)
{
	return bc160_put_update(src, src_size);
}

static int
bc160_gave(char *reply)
{
	UNUSED_ARG(reply);
	return 0;
}

static int
bc160_close()
{
	return 0;
}

/*
 *  BCH digest is 40 characters of 0-9 or a-f.
 */
static int
bc160_syntax()
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
bc160_empty()
{
	return strcmp(empty, digest) == 0?  1 : 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static void
_trace(char *msg)
{
	trace2("bc160", msg);
}

/*
 *  Digest data on input and update the global digest[129] array.
 */
static char *
bc160_eat_input()
{
	unsigned char buf[PIPE_MAX], *q, *q_end;
	char *p;
	int nread;
	unsigned char sha_digest[32];

	_TRACE("request to bc160_eat_input()");

	while ((nread = uni_read(input_fd, buf, sizeof buf)) > 0)
		SHA256_Update(&bc160_ctx.sha_ctx, buf, nread);
	if (nread < 0)
		return strerror(errno);
	SHA256_Final(sha_digest, &bc160_ctx.sha_ctx);
	RIPEMD160_Update(&bc160_ctx.ripemd_ctx, sha_digest, 32);
	RIPEMD160_Final(bin_digest, &bc160_ctx.ripemd_ctx);

	p = digest;
	q = bin_digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;

	_TRACE("bc160_eat_input() done");
	return (char *)0;
}

/*
 *  Convert an ascii digest to a file system path.
 */
static char *
bc160_fs_name(char *name, int size)
{
	char *dp, *np;

	if (size < 10)
		return "file name size too small: size < 10 bytes";

	dp = digest + 31;
	np = name;

	*np++ = *dp++;  *np++ = *dp++;  *np++ = *dp++;  *np++ = *dp++;
	*np++ = *dp++;  *np++ = *dp++;  *np++ = *dp++;  *np++ = *dp++;
	*np++ = *dp++;

	*np = 0;

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
 *  Make the directory path to a file system blob.
 */
static char *
bc160_fs_mkdir(char *path, int size)
{
	char *dp, *dirp;

	if (size < 37)
		return "fs_mkdir: size < 37 bytes";

	dp = digest;

	dirp = bufcat(path, size, "/");

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

/*
 *  Convert an ascii digest to a file system path.
 */
static char *
bc160_fs_path(char *file_path, int size)
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

struct digest	bc160_digest =
{
	.algorithm	=	"bc160",

	.init		=	bc160_init,
	.get_update	=	bc160_get_update,
	.take_update	=	bc160_take_update,
	.took		=	bc160_took,
	.put_update	=	bc160_put_update,
	.give_update	=	bc160_give_update,
	.gave		=	bc160_gave,
	.close		=	bc160_close,

	.eat_input	=	bc160_eat_input,

	.syntax		=	bc160_syntax,
	.empty		=	bc160_empty,

	.fs_name	=	bc160_fs_name,
	.fs_mkdir	=	bc160_fs_mkdir,
	.fs_path	=	bc160_fs_path
};

#endif
