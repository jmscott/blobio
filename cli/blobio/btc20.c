/*
 *  Synopsis:
 *	20 byte Bitcoin Wallet Hash RIPEMD160(SHA256(SHA256(blob)))
 */
#ifdef FS_BTC20_MODULE    

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

extern char	verb[];
extern char	ascii_digest[];
extern int	input_fd;

static unsigned char	bin_digest[20];

typedef struct
{
	SHA256_CTX	sha256;
	SHA256_CTX	sha256_sha256;
	RIPEMD160_CTX	ripemd160;
} BTC20_CTX;

static BTC20_CTX	btc20_ctx;

static char	empty[]		= "fd7b15dc5dc2039556693555c2b81b36c8deec15";

static char *
btc20_init()
{
	char *d40, c;
	unsigned char *d20;
	unsigned int i;
	static char n[] = "btc20: init";

	if (strcmp("roll", verb)) {
		if (!SHA256_Init(&btc20_ctx.sha256))
			return "SHA256_Init(blob) failed";
		if (!SHA256_Init(&btc20_ctx.sha256_sha256))
			return "SHA256_Init(sha256) failed";
		if (!RIPEMD160_Init(&btc20_ctx.ripemd160))
			return "RIPEMD160_Init() failed";

		/*
		 *  Convert the 40 character hex signature to 20 byte binary.
		 *  The ascii, hex version has already been scanned for 
		 *  syntactic correctness.
		 */
		if (ascii_digest[0]) {
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
				trace2(n, "hex dump of 20 byte bin digest:");
				hexdump(d20, 20, '=');
			}
#endif
		}
	}
	return (char *)0;
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 *  Check that the running digest matches the requested digest, implying
 *  were a done.
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
	static char n[] = "btc20: chew";

	TRACE2(n, "request to chew()");

	if (!SHA256_Update(&btc20_ctx.sha256, chunk, size))
		return "SHA256_Update(chunk) failed";

	/*
	 *  Finalize a temporary copy of sha256.
	 */
	SHA256_CTX tmp_sha_ctx;
	unsigned char tmp_sha_digest[32];

	tmp_sha_ctx = btc20_ctx.sha256;
	if (!SHA256_Final(tmp_sha_digest, &tmp_sha_ctx))
		return "SHA256_Final(tmp) failed";

	/*
	 *  Now take a temporary SHA256(SHA256) of currently seen SHA256.
	 */
	SHA256_CTX tmp_sha_sha_ctx;
	unsigned char tmp_sha_sha_digest[32];
	if (!SHA256_Init(&tmp_sha_sha_ctx))
		return "SHA256_Init(sha256) failed";
	if (!SHA256_Update(&tmp_sha_sha_ctx, tmp_sha_digest, 32))
		return "SHA256_Update(sha256) failed";
	if (!SHA256_Final(tmp_sha_sha_digest, &tmp_sha_sha_ctx))
		return "SHA256_Final(sha256) failed";

	/*
	 *  Take RIPEMD160 of temp sha256 of sha256 digest.
	 */
	unsigned char tmp_ripemd_digest[20];
	RIPEMD160_CTX tmp_ripemd_ctx;

	if (!RIPEMD160_Init(&tmp_ripemd_ctx))
		return "RIPEMD160_Init(tmp sha256 sha256) failed";
	if (!RIPEMD160_Update(&tmp_ripemd_ctx, tmp_sha_sha_digest, 32))
		return "RIPEMD160_Update(tmp sha256 sha256) failed";
	if (!RIPEMD160_Final(tmp_ripemd_digest, &tmp_ripemd_ctx))
		return "RIPEMD160_Final(tmp sha256 sha256) failed";
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
btc20_get_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
btc20_take_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
btc20_took(char *reply)
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
btc20_put_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
btc20_give_update(unsigned char *src, int src_size)
{
	return btc20_put_update(src, src_size);
}

static char *
btc20_gave(char *reply)
{
	(void)reply;
	return "0";
}

static int
btc20_close()
{
	return 0;
}

/*
 *  BCH digest is 40 characters of 0-9 or a-f.
 */
static int
btc20_syntax()
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
btc20_empty()
{
	return strcmp(empty, ascii_digest) == 0?  1 : 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static void
_trace(char *msg)
{
	trace2("btc20", msg);
}

static void
_trace2(char *msg1, char *msg2)
{
	trace3("btc20", msg1, msg2);
}

/*
 *  Digest data on input and update the global ascii_digest[129] array.
 *
 *  Returns:
 *	(char *)	input eaten success fully
 *	"..."		error message.
 */
static char *
btc20_eat_input(int fd)
{
	unsigned char buf[MAX_ATOMIC_MSG], *q, *q_end;
	char *p;
	int nread;

	_TRACE("request to btc20_eat_input()");

	while ((nread = jmscott_read(fd, buf, sizeof buf)) > 0)
		if (!SHA256_Update(&btc20_ctx.sha256, buf, nread))
			return "SHA256_Update(chunk) failed";
	if (nread < 0)
		return strerror(errno);

	unsigned char sha_digest[32];
	if (!SHA256_Final(sha_digest, &btc20_ctx.sha256))
		return "SHA256_Final(blob) failed";

	//  take the sha256 of the 32 byte sha256 of the entire blob
	if (!SHA256_Update(&btc20_ctx.sha256_sha256, sha_digest, 32))
		return "SHA256_Update(sha256) failed";

	unsigned char sha_sha_digest[32];
	if (!SHA256_Final(sha_sha_digest, &btc20_ctx.sha256_sha256))
		return "SHA256_Final(sha256) failed";

	//  calculate the 20 byte ripemd of the composed sha256(sha256(blob))
	if (!RIPEMD160_Update(&btc20_ctx.ripemd160, sha_sha_digest, 32))
		return "RIPEMD160_Update(SHA256) failed";
	unsigned char ripe_digest[32];
	if (!RIPEMD160_Final(ripe_digest, &btc20_ctx.ripemd160))
		return "RIPEMD160_Final(SHA256) failed";

	//  convert the ripemd 20 byte to 40 byte ascii

	p = ascii_digest;
	q = ripe_digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;

	_TRACE("btc20_eat_input() done");
	return (char *)0;
}

/*
 *  Convert an ascii digest to a file system path.
 */
static char *
fs_btc20_name(char *name, int size)
{

	if (size < 41)
		return "file name size too small: size < 41 bytes";
	memcpy(name, ascii_digest, 40);
	name[40] = 0;

	return (char *)0;
}

//  Note: not sure about the mode: u=rwx,g=rx,o=
static int
_mkdir(char *path)
{
	return jmscott_mkdirp(path, 0710);
}

/*
 *  Make the directory path to a file system blob, appending "/" at end.
 */
static char *
fs_btc20_mkdir(char *path, int size)
{
	char *dp, *p;

	if (size < 10)
		return "size < 10 bytes";

	_TRACE2("fs_mkdir: path", path);
	dp = ascii_digest;

	p = bufcat(path, size, "/");

	*p++ = *dp++;    *p++ = *dp++;    *p++ = *dp++; 
	*p = 0;
	if (_mkdir(path))
		return strerror(errno);

	*p++ = '/';
	*p++ = *dp++;    *p++ = *dp++;    *p++ = *dp++;  
	*p++ = '/';
	*p = 0;
	if (_mkdir(path))
		return strerror(errno);
	return (char *)0;
}

/*
 *  Convert an ascii digest to a file system path.
 */
static char *
fs_btc20_path(char *file_path, int size)
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

static char *
btc20_empty_digest()
{
	return empty;
}

struct digest	btc20_digest =
{
	.algorithm	=	"btc20",

	.init		=	btc20_init,
	.get_update	=	btc20_get_update,
	.take_update	=	btc20_take_update,
	.took		=	btc20_took,
	.put_update	=	btc20_put_update,
	.give_update	=	btc20_give_update,
	.gave		=	btc20_gave,
	.close		=	btc20_close,

	.eat_input	=	btc20_eat_input,

	.syntax		=	btc20_syntax,
	.empty		=	btc20_empty,
	.empty_digest	=	btc20_empty_digest,

	.fs_name	=	fs_btc20_name,
	.fs_mkdir	=	fs_btc20_mkdir,
	.fs_path	=	fs_btc20_path
};

#endif
