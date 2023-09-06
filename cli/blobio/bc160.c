/*
 *  Synopsis:
 *	Deprecated hash similar to bitcoin wallet hash.
 */
#ifdef FS_BC160_MODULE    

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "openssl/sha.h"
#include "openssl/ripemd.h"
#include "jmscott/libjmscott.h"
#include "blobio.h"

extern char	verb[];
extern char	ascii_digest[];
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

	if (!SHA256_Init(&bc160_ctx.sha256))
		return "SHA256_Init() failed";
	if (!RIPEMD160_Init(&bc160_ctx.ripemd160))
		return "RIPEMD160_Init() failed";
	if (!ascii_digest[0])
		return (char *)0;

	/*
	 *  Convert the 40 character hex signature to 20 byte binary.
	 *  The ascii, hex version has already been scanned for 
	 *  syntactic correctness.
	 */
	d40 = ascii_digest;
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
		TRACE("hex dump of 20 byte bin digest ...");
		hexdump(d20, 20, '=');
	}
#endif
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
bc160_chew(unsigned char *chunk, int size)
{
	TRACE("entered");
	if (!SHA256_Update(&bc160_ctx.sha256, chunk, size))
		return "SHA256_Update(chunk) failed";

	/*
	 *  Finalize a temporary copy of sha256.
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
	if (tracing) {
		TRACE("hex dump of 20 byte RIPEMD160 follows ...");
		hexdump(tmp_ripemd_digest, 20, '=');
		TRACE("chew(tmp_ripemd) done");
	}
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
	return bc160_chew(src, src_size);
}

static char *
bc160_take_update(unsigned char *src, int src_size)
{
	return bc160_chew(src, src_size);
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
	return bc160_chew(src, src_size);
}

static char *
bc160_give_update(unsigned char *src, int src_size)
{
	return bc160_put_update(src, src_size);
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

	p = ascii_digest;
	while ((c = *p++)) {
		if (p - ascii_digest > 40)
			return 0;
		if (('0' > c||c > '9') && ('a' > c||c > 'f'))
			return 0;
	}
	return p - ascii_digest == 41 ?
		(strcmp(ascii_digest, empty) == 0 ? 2 : 1) : 0;
}

/*
 *  Is the ascii digest == to well-known empty digest
 *  b472a266d0bd89c13706a4132ccfb16f7c3b9fcb 
 */
static int
bc160_empty()
{
	return strcmp(empty, ascii_digest) == 0?  1 : 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

/*
 *  Digest data on input and update the global ascii_digest[129] array.
 *
 *  Returns:
 *	(char *)	input eaten success fully
 *	"..."		error message.
 */
static char *
bc160_eat_input(int fd)
{
	unsigned char buf[MAX_ATOMIC_MSG], *q, *q_end;
	char *p;
	int nread;
	unsigned char sha_digest[32];

	TRACE("entered");

	while ((nread = jmscott_read(fd, buf, sizeof buf)) > 0)
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

	p = ascii_digest;
	q = bin_digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;

	TRACE("done");
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
	memcpy(name, ascii_digest, 40);
	name[40] = 0;

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

	dp = ascii_digest;
	fp = file_path;

	*fp++ = '/';		//  first directory
	*fp++ = *dp++;
	*fp++ = *dp++;
	*fp++ = *dp++;

	*fp++ = '/';		//  second directory
	*fp++ = *dp++;
	*fp++ = *dp++;
	*fp++ = *dp++;

	dp = ascii_digest;	//  file name is 40 chars of sha digest
	*fp++ = '/';
	memcpy(fp, ascii_digest, 40);
	fp[40] = 0;

	return (char *)0;
}
/*
 *  Convert an ascii digest to a file system path.
 */
static char *
bc160_fs_dir_path(char *dir_path, int size)
{
	char *digp, *dirp;

	if (size < 8)
		return "dir path size too small: size < 8 bytes";

	digp = ascii_digest;
	dirp = dir_path;

	*dirp++ = *digp++;
	*dirp++ = *digp++;
	*dirp++ = *digp++;

	*dirp++ = '/';		//  second directory
	*dirp++ = *digp++;
	*dirp++ = *digp++;
	*dirp++ = *digp++;
	*dirp = 0;

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
	.close		=	bc160_close,

	.eat_input	=	bc160_eat_input,

	.syntax		=	bc160_syntax,
	.empty		=	bc160_empty,
	.empty_digest	=	bc160_empty_digest,

	.fs_name	=	bc160_fs_name,
	.fs_path	=	bc160_fs_path,
	.fs_dir_path	=	bc160_fs_dir_path
};

#endif
