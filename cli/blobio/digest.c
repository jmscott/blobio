/*
 *  Synopsis:
 *	Hooks for all Hash Digest Modules
 *  Note:
 *	Why are the digest #defines prefixed with FS_ ?
 *	What about network access?
 */
#include <string.h>
#include <errno.h>
#include "blobio.h"

extern struct digest	*digest_module;
extern int		errno;

#ifdef FS_SHA_MODULE
extern struct digest	sha_digest;
#endif

#ifdef FS_BC160_MODULE
extern struct digest	bc160_digest;
#endif

#ifdef FS_BTC20_MODULE
extern struct digest	btc20_digest;
#endif

struct digest *digests[] =
{
#ifdef FS_BTC20_MODULE
	&btc20_digest,
#endif

#ifdef FS_SHA_MODULE
	&sha_digest,
#endif

#ifdef FS_BC160_MODULE
	&bc160_digest,
#endif
	(struct digest *)0
};

/*
 *  Digest the input stream explicity up to end of stream, incrementally
 *  writing to output stream.
 *
 *	return == (char *)0	successfully copied data matching digest
 *	strlen(return) > 1	error message
 *
 *  Note:
 *	Timeout not supported.
 */
char *
digest_copy(int in_fd, int out_fd)
{
	int nread;
	unsigned char buf[PIPE_MAX];
	char *status = "";

	if ((status = digest_module->init()))
		return status;
	while ((nread = uni_read(in_fd, buf, sizeof buf)) > 0) {
		status = digest_module->get_update(buf, sizeof buf);
		if (strlen(status) > 1)
			return status;
		if (uni_write_buf(out_fd, buf, nread))
			return strerror(errno);
		if (status[0] == '0')
			break;
	}
	if (nread > 0)				//  status must == "0"
		return (char *)0;
	if (nread < 0)
		return strerror(errno);

	//  read returned 0

	if (status[0] == '1')
		return "unexpected end of file before digest seen";

	if (status[0])
		return (char *)0;

	//  first read returned 0 so digest must be empty to be correct

	if (digest_module->empty())
		return (char *)0;
	return "empty stream for full digest";
}
