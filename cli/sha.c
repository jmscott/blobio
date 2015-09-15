/*
 *  Synopsis:
 *	blk_SHA1 client module interface routines.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#ifdef SHA_FS_MODULE    

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "../sha1/sha1.h"
#include "blobio.h"

struct sha_request
{
	unsigned char		digest[20];
	blk_SHA_CTX		ctx;
};

static char	empty[]		= "da39a3ee5e6b4b0d3255bfef95601890afd80709";

static int
sha_open(struct request *rp)
{
	struct sha_request *sp;
	char *d40, c;
	unsigned char *d20;
	unsigned int i;
	static char nm[] = "sha: open";

	sp = (struct sha_request *)malloc(sizeof *sp);
	if (!sp)
		return ENOMEM;
	if (strcmp("roll", rp->verb)) {
		blk_SHA1_Init(&sp->ctx);

		/*
		 *  Convert the 40 character hex signature to 20 byte binary.
		 *  The ascii, hex version has already been scanned for 
		 *  syntactic correctness.
		 */
		if (rp->digest[0]) {
			d40 = rp->digest;
			d20 = sp->digest;
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
			if (tracing) {
				trace2(nm,
					"dump of expected digest follows ...");
				dump(sp->digest, 20, '=');
			}
		}
	}
	rp->open_data = (void *)sp;
	return 0;
}

/*
 *  Update the digest with a chunk of the blob being put to the server.
 */
static int
chew(struct sha_request *sp, unsigned char *chunk, int size)
{
	static char nm[] = "sha: chew";

	/*
	 *  Incremental digest.
	 */
	unsigned char tmp_digest[20];
	blk_SHA_CTX tmp_ctx;

	blk_SHA1_Update(&sp->ctx, chunk, size);
	/*
	 *  Copy current digest state to a temporary state,
	 *  finalize and then compare to expected state.
	 */
	tmp_ctx = sp->ctx;
	blk_SHA1_Final(tmp_digest, &tmp_ctx);
	if (tracing) {
		trace2(nm, "dump of 20 byte digest follows ...");
		dump(tmp_digest, 20, '=');
	}
	return memcmp(tmp_digest, sp->digest, 20) == 0 ? 0 : 1;
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
sha_get_update(struct request *rp, unsigned char *src, int src_size)
{
	return chew((struct sha_request *)rp->open_data, src, src_size);
}

static int
sha_take_update(struct request *rp, unsigned char *src, int src_size)
{
	return sha_get_update(rp, src, src_size);
}

static int
sha_took(struct request *rp, char *reply)
{
	UNUSED_ARG(rp);
	UNUSED_ARG(reply);
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
sha_put_update(struct request *rp, unsigned char *src, int src_size)
{
	return chew((struct sha_request *)rp->open_data, src, src_size);
}

static int
sha_give_update(struct request *rp, unsigned char *src, int src_size)
{
	return sha_put_update(rp, src, src_size);
}

static int
sha_gave(struct request *rp, char *reply)
{
	UNUSED_ARG(rp);
	UNUSED_ARG(reply);
	return 0;
}

static int
sha_close(struct request *rp)
{
	struct sha_request *sp = (struct sha_request *)rp->open_data;

	free((void *)sp);
	rp->open_data = (void *)0;
	return 0;
}

/*
 *  blk_SHA1 digest is 40 characters of 0-9 or a-f.
 */
static int
sha_is_digest(char *digest)
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
sha_is_empty(char *digest)
{
	return strcmp("da39a3ee5e6b4b0d3255bfef95601890afd80709", digest) == 0?
						1 : 0;
}

static int
_read(int fd, unsigned char *buf, int buf_size)
{
	int nread;
again:
	nread = read(fd, buf, buf_size);
	if (nread < 0) {
		if (errno == EAGAIN || errno == EINTR)
			goto again;
		return -1;
	}
	return nread;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};


/*
 *  Digest data on input and generate ascii digest.
 */
static int
sha_eat(struct request *rp)
{
	struct sha_request *sp;
	unsigned char buf[1024 * 64], *q, *q_end;
	char *p;
	int nread;

	sp = (struct sha_request *)rp->open_data;
	while ((nread = _read(rp->in_fd, buf, sizeof buf)) > 0)
		blk_SHA1_Update(&sp->ctx, buf, nread);
	if (nread < 0)
		return -1;
	blk_SHA1_Final(sp->digest, &sp->ctx);

	p = rp->digest;
	q = sp->digest;
	q_end = q + 20;

	while (q < q_end) {
		*p++ = nib2hex[(*q & 0xf0) >> 4];
		*p++ = nib2hex[*q & 0xf];
		q++;
	}
	*p = 0;
	return 0;
}

struct module	sha_module =
{
	.algorithm	=	"sha",

	.open		=	sha_open,
	.get_update	=	sha_get_update,
	.take_update	=	sha_take_update,
	.took		=	sha_took,
	.put_update	=	sha_put_update,
	.give_update	=	sha_give_update,
	.gave		=	sha_gave,
	.close		=	sha_close,

	.eat		=	sha_eat,

	.is_digest	=	sha_is_digest,
	.is_empty	=	sha_is_empty
};

#endif
