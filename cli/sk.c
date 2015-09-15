/*
 *  Synopsis:
 *	Skein digest module interface for blobio client.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#ifdef SK_FS_MODULE    

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "blobio.h"
#include "skein.h"

struct sk_request
{
	unsigned char		digest[32];
	Skein_512_Ctxt_t	ctx;
};

extern void	skein_digest2nab(unsigned char *, char *);
extern int	skein_nab2digest(char *, unsigned char *);
extern int	skein_is_nab(char *);

static int
sk_open(struct request *rp)
{
	struct sk_request *sp;
	static char nm[] = "sk: open";

	sp = (struct sk_request *)malloc(sizeof *sp);
	if (!sp)
		return ENOMEM;
	if (strcmp("roll", rp->verb)) {
		if (Skein_512_Init(&sp->ctx, 256) != SKEIN_SUCCESS) {
			if (tracing)
				trace2(nm, "SK1_Init() failed");
			return -1;
		}

		/*
		 *  Convert the 43 character "nab" signature to 32 byte binary.
		 */
		if (rp->digest[0]) {
			if (skein_nab2digest(rp->digest, sp->digest)) {
				if (tracing)
					trace(
					 "sk: open: skein_nab2digest() failed");
				return EINVAL;
			}
			if (tracing) {
				trace2(nm,
					"dump of expected digest follows ...");
				dump(sp->digest, 32, '=');
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
chew(struct sk_request *sp, unsigned char *chunk, int size)
{
	static char nm[] = "sk: chew";

	/*
	 *  Incremental digest.
	 */
	unsigned char tmp_digest[32];
	Skein_512_Ctxt_t tmp_ctx;

	if (Skein_512_Update(&sp->ctx, chunk, size) != SKEIN_SUCCESS) {
		if (tracing)
			trace("sk: chew: Skein_512_Update() failed");
		return -1;
	}
	/*
	 *  Copy current digest state to a temporary state,
	 *  finalize and then compare to expected state.
	 */
	tmp_ctx = sp->ctx;
	if (Skein_512_Final(&tmp_ctx, tmp_digest) != SKEIN_SUCCESS) {
		if (tracing)
			trace("sk: chew: Skein_512_Final() failed");
		return -1;
	}
	if (tracing) {
		trace2(nm, "dump of 32 byte digest follows ...");
		dump(tmp_digest, 32, '=');
	}
	return memcmp(tmp_digest, sp->digest, 32) == 0 ? 0 : 1;
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
sk_get_update(struct request *rp, unsigned char *src, int src_size)
{
	return chew((struct sk_request *)rp->open_data, src, src_size);
}

static int
sk_take_update(struct request *rp, unsigned char *src, int src_size)
{
	return sk_get_update(rp, src, src_size);
}

static int
sk_took(struct request *rp, char *reply)
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
sk_put_update(struct request *rp, unsigned char *src, int src_size)
{
	return chew((struct sk_request *)rp->open_data, src, src_size);
}

static int
sk_give_update(struct request *rp, unsigned char *src, int src_size)
{
	return sk_put_update(rp, src, src_size);
}

static int
sk_gave(struct request *rp, char *reply)
{
	UNUSED_ARG(rp);
	UNUSED_ARG(reply);
	return 0;
}

static int
sk_close(struct request *rp)
{
	struct sk_request *sp = (struct sk_request *)rp->open_data;

	free((void *)sp);
	rp->open_data = (void *)0;
	return 0;
}

/*
 *  SK1 digest is 40 characters of 0-9 or a-f.
 */
static int
sk_is_digest(char *digest)
{
	return skein_is_nab(digest);
}

/*
 *  SK1 digest is 40 characters of 0-9 or a-f.
 */
static int
sk_is_empty(char *digest)
{
	return strcmp("7W3GlyqKvCUqLw03HHrwZ63l0DGGfSH3zV0xij24Yk3", digest)
		== 0 ? 1 : 0;
}

/*
 *  Read from input file.  No timeouts!!!
 */
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

/*
 *  Digest data on input and generate ascii digest.
 */
static int
sk_eat(struct request *rp)
{
	struct sk_request *sp;
	unsigned char buf[1024 * 64];
	int nread;

	sp = (struct sk_request *)rp->open_data;
	while ((nread = _read(rp->in_fd, buf, sizeof buf)) > 0)
		if (Skein_512_Update(&sp->ctx, buf, nread) != SKEIN_SUCCESS) {
			if (tracing)
				trace("sk: eat: Skein_512_Update() failed");
			return -1;
		}
	if (nread < 0) {
		if (tracing)
			trace("sk: eat: _read() failed");
		return -1;
	}
	if (Skein_512_Final(&sp->ctx, sp->digest) != SKEIN_SUCCESS) {
		if (tracing)
			trace("sk: eat: Skein_512_Final() failed");
		return -1;
	}
	skein_digest2nab(sp->digest, rp->digest);
	return 0;
}

struct module	sk_module =
{
	.algorithm	=	"sk",

	.open		=	sk_open,
	.get_update	=	sk_get_update,
	.take_update	=	sk_take_update,
	.took		=	sk_took,
	.put_update	=	sk_put_update,
	.give_update	=	sk_give_update,
	.gave		=	sk_gave,
	.close		=	sk_close,

	.eat		=	sk_eat,

	.is_digest	=	sk_is_digest,
	.is_empty	=	sk_is_empty
};

#endif
