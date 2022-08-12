/*
 *  Synopsis:
 *	openssl SHA1 client digest module interface routines.
 *  Note:
 *  	Not a timed read() since assumed to be local file.
 */
#ifdef FS_SHA_MODULE    

#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "openssl/sha.h"

#include "blobio.h"

extern char	verb[];
extern char	ascii_digest[];
extern int	input_fd;

static unsigned char	bin_digest[20];
static SHA_CTX	sha_ctx;

static char	empty[]		= "da39a3ee5e6b4b0d3255bfef95601890afd80709";

static char *
sha_init()
{
	char *d40, c;
	unsigned char *d20;
	unsigned int i;

	if (strcmp("roll", verb)) {
		if (!SHA1_Init(&sha_ctx))
			return "SHA1_Init() failed";

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
				TRACE("hex dump of 20 byte bin digest ...");
				hexdump(d20, 20, '=');
			}
#endif
		}
	}
	return (char *)0;
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 *  Return
 *
 *	"0"	no more to chew, blob has digest
 *	"1"	more input to chew, digest not seen
 *	"..."	error message
 */
static char *
chew(unsigned char *chunk, int size)
{
	TRACE("request to chew()");

	/*
	 *  Incremental digest.
	 */
	unsigned char tmp_digest[20];
	SHA_CTX tmp_ctx;

	if (!SHA1_Update(&sha_ctx, chunk, size))
		return "SHA1_Update(chunk) failed";
	/*
	 *  Copy current digest state to a temporary state,
	 *  finalize and then compare to expected state.
	 */
	tmp_ctx = sha_ctx;
	if (!SHA1_Final(tmp_digest, &tmp_ctx))
		return "SHA1_Final(tmp) failed";

#ifdef COMPILE_TRACE
	if (tracing) {
		TRACE("hex dump of 20 byte bin digest ...");
		hexdump(tmp_digest, 20, '=');
		TRACE("chew() done");
	}
#endif
	return memcmp(tmp_digest, bin_digest, 20) == 0 ? "0" : "1";
}

/*
 *  Synopsis:
 *	Update the partial digest with a chunk read from a get request.
 *  Returns:
 *	"0"	ok, no more to read, blob seen
 *	"1"	more to read, continue, blob not seen
 *	"..."	error message
 */
static char *
sha_get_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
sha_take_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
sha_took(char *reply)
{
	(void)reply;
	return 0;
}

/*
 *  Synopsis:
 *	Update the digest with a chunk from a put request.
 *  Returns:
 *	"0"	done, digest seen input
 *	"1"	not done, digest not seen
 *	"..."	error message
 */
static char *
sha_put_update(unsigned char *src, int src_size)
{
	return chew(src, src_size);
}

static char *
sha_give_update(unsigned char *src, int src_size)
{
	return sha_put_update(src, src_size);
}

static char *
sha_gave(char *reply)
{
	(void)reply;
	return "0";
}

static int
sha_close()
{
	return 0;
}

/*
 *  SHA1 digest is 40 characters of 0-9 or a-f.
 */
static int
sha_syntax()
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
 *  da39a3ee5e6b4b0d3255bfef95601890afd80709 
 */
static int
sha_empty()
{
	return strcmp(empty, ascii_digest) == 0? 1 : 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

/*
 *  Digest data on input and update the global ascii_digest[129] array.
 *
 *  Note:
 *	Not a timed read(), since assumed to be local file.
 */
static char *
sha_eat_input(int fd)
{
	unsigned char buf[MAX_ATOMIC_MSG], *q, *q_end;
	char *p;
	int nread;

	TRACE("request to sha_eat_input()");

	while ((nread = jmscott_read(fd, buf, sizeof buf)) > 0)
		if (!SHA1_Update(&sha_ctx, buf, nread))
			return "SHA1_Update(chunk) failed";
	if (nread < 0)
		return strerror(errno);
	if (!SHA1_Final(bin_digest, &sha_ctx))
		return "SHA1_Final() failed";

	p = ascii_digest;
	q = bin_digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;

	TRACE("sha_eat_input() done");
	return (char *)0;
}

/*
 *  Convert an ascii digest to a name of blob file.
 */
static char *
fs_sha_name(char *name, int size)
{
	if (size < 41)
		return "file name size too small: size < 41 bytes";
	memcpy(name, ascii_digest, 40);
	name[40] = 0;

	return (char *)0;
}

static int
_mkdir(char *path)
{
	return jmscott_mkdirp(path, 0710);	// g=rwx,g=x,o=
}

/*
 *  For a sha blob, assemble the directory path AND make all the needed
 *  directory entries.
 */
static char *
fs_sha_mkdir(char *path, int size)
{
	char *dp, *p;

	if (size < 10)
		return "fs_mkdir: size < 10 bytes";

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
 *  Convert an ascii digest to a file system path that looks like
 *  for blob sha:4f39dfffcfe2e4cc1b089b3e4b5b595cf904b7b2
 *
 *	4f3/9df/4f39dfffcfe2e4cc1b089b3e4b5b595cf904b7b2
 *
 *  for a blob with sha digest 4f39dfffcfe2e4cc1b089b3e4b5b595cf904b7b2.
 */
static char *
fs_sha_path(char *file_path, int size)
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
sha_empty_digest()
{
	return empty;
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
	.empty_digest	=	sha_empty_digest,

	.fs_name	=	fs_sha_name,
	.fs_mkdir	=	fs_sha_mkdir,
	.fs_path	=	fs_sha_path
};

#endif
