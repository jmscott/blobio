/*
 *  Synopsis:
 *	Bitcoin Hash RIPEMD160(SHA256(blob))
 *  Note:
 *	Thinks about a version of bc160 named bc20 which uses base64 encoding.
 *	Important that openssl be able to encode in base58
 *
 *  	Not a timed read() since assumed to be local file.
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

#define _TRACE(msg)		if (tracing) _trace(msg)
#define _TRACE2(msg1,msg2)	if (tracing) _trace2(msg1,msg2)

#else

#define _TRACE(msg)
#define _TRACE2(msg1,msg2)

#endif

extern char	*verb;
extern char	digest[];
extern int	input_fd;

static unsigned char	bin_digest[20];

typedef struct
{
	RIPEMD160_CTX	ripemd160;
	SHA256_CTX	sha256;
} BC160_CTX;

static BC160_CTX	bc160_ctx;

static char	empty[]		= "b472a266d0bd89c13706a4132ccfb16f7c3b9fcb";

static char *
bc160_init()
{
	char *d40, c;
	unsigned char *d20;
	unsigned int i;
	static char n[] = "bc160: init";

	if (strcmp("roll", verb)) {
		if (!SHA256_Init(&bc160_ctx.sha256))
			return "SHA256_Init() failed";
		if (!RIPEMD160_Init(&bc160_ctx.ripemd160))
			return "RIPEMD160_Init() failed";

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
				trace2(n, "hex dump of 20 byte digest:");
				hexdump(d20, 20, '=');
			}
#endif
		}
	}
	return (char *)0;
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 *
 *  Return
 *
 *	"0"	no more to chew since digest matches requested digest;
 *	"1"	more to chew since digest does not match request digest
 *	"..."	error
 */
static char *
chew(unsigned char *chunk, int size)
{
	static char n[] = "bc160: chew";

	TRACE2(n, "request to chew()");

	if (!SHA256_Update(&bc160_ctx.sha256, chunk, size))
		return "SHA256_Update(chunk) failed";

	/*
	 *  Finalize a temporay copy of sha256.
	 */
	SHA256_CTX tmp_sha_ctx;
	unsigned char tmp_sha_digest[32];

	tmp_sha_ctx = bc160_ctx.sha256;
	if (!SHA256_Final(tmp_sha_digest, &tmp_sha_ctx))
		return "SHA256_Final(tmp) failed";

	/*
	 *  Take RIPEMD160 of temp sha256 digest.
	 */
	unsigned char tmp_ripemd_digest[20];
	RIPEMD160_CTX tmp_ripemd_ctx;

	if (!RIPEMD160_Init(&tmp_ripemd_ctx))
		return "RIPEMD160_Init(tmp) failed";
	if (!RIPEMD160_Update(&tmp_ripemd_ctx, tmp_sha_digest, 32))
		return "RIPEMD160_Update(tmp SHA256) failed";
	if (!RIPEMD160_Final(tmp_ripemd_digest, &tmp_ripemd_ctx))
		return "RIPEMD160_Final(tmp SHA256) failed";
#ifdef COMPILE_TRACE
	if (tracing) {
		trace2(n, "hex dump of 20 byte RIPEMD160 follows ...");
		hexdump(tmp_ripemd_digest, 20, '=');
		trace2(n, "chew(tmp_ripemd) done");
	}
#endif
	return memcmp(tmp_ripemd_digest, bin_digest, 20) == 0 ? "0" : "1";
}

/*
 *  Synopsis:
 *	Update the partial digest with a chunk read from a get request.
 *  Returns:
 *	"0"	ok, no more to read, blob seen
 *	"1"	more blob to read
 *	"..."	error message
 */
static char *
bc160_get_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
bc160_take_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
bc160_took(char *reply)
{
	(void)reply;
	return "0";
}

/*
 *  Synopsis:
 *	Update the digest with a chunk from in a put request.
 *  Returns:
 *	0	id blob finished
 *	1	continue putting blob.
 *	-1	chunk not part of the blob.
 */
static char *
bc160_put_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
bc160_give_update(unsigned char *src, int src_size)
{
	return bc160_put_update(src, src_size);
}

static char *
bc160_gave(char *reply)
{
	(void)reply;
	return "0";
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
 *  b472a266d0bd89c13706a4132ccfb16f7c3b9fcb 
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

static void
_trace2(char *msg1, char *msg2)
{
	trace3("bc160", msg1, msg2);
}

/*
 *  Digest data on input and update the global digest[129] array.
 *
 *  Returns:
 *	(char *)	input eaten success fully
 *	"..."		error message.
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
		if (!SHA256_Update(&bc160_ctx.sha256, buf, nread))
			return "SHA256_Update(chunk) failed";
	if (nread < 0)
		return strerror(errno);
	if (!SHA256_Final(sha_digest, &bc160_ctx.sha256))
		return "SHA256_Final() failed";

	if (!RIPEMD160_Update(&bc160_ctx.ripemd160, sha_digest, 32))
		return "RIPEMD160_Update(SHA256) failed";
	if (!RIPEMD160_Final(bin_digest, &bc160_ctx.ripemd160))
		return "RIPEMD160_Final(SHA256) failed";

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
	if (size < 41)
		return "file name size too small: size < 41 bytes";
	memcpy(name, digest, 40);
	name[40] = 0;

	return (char *)0;
}

static int
_mkdir(char *path)
{
	return (uni_mkdir(path, 0777) == 0 || errno == EEXIST) ? 0 : -1;
}

/*
 *  Make the directory path to a file system blob.
 */
static char *
bc160_fs_mkdir(char *path, int size)
{
	char *dp, *p;

	if (size < 10)
		return "size < 10 bytes";

	_TRACE2("fs_mkdir: path", path);

	dp = digest;

	p = bufcat(path, size, "/");

	*p++ = *dp++;    *p++ = *dp++;    *p++ = *dp++; 
	*p = 0;
	if (_mkdir(path))
		return strerror(errno);

	*p++ = '/';
	*p++ = *dp++;    *p++ = *dp++;    *p++ = *dp++;  
	*dp = 0;
	if (_mkdir(path))
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

	if (size < 49)
		return "file path size too small: size < 49 bytes";

	dp = digest;
	fp = file_path;

	*fp++ = '/';		//  first directory
	*fp++ = *dp++;
	*fp++ = *dp++;
	*fp++ = *dp++;

	*fp++ = '/';		//  second directory
	*fp++ = *dp++;
	*fp++ = *dp++;
	*fp++ = *dp++;

	dp = digest;		//  file name is 40 chars of sha digest
	*fp++ = '/';
	memcpy(fp, digest, 40);
	fp[40] = 0;

	return (char *)0;
}

static char *
bc160_empty_digest()
{
	return empty;
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
	.empty_digest	=	bc160_empty_digest,

	.fs_name	=	bc160_fs_name,
	.fs_mkdir	=	bc160_fs_mkdir,
	.fs_path	=	bc160_fs_path
};

#endif
