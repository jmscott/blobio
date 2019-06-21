/*
 *  Synopsis:
 *	Hash Digest Modules
 */
#include "blobio.h"

#ifdef SHA_FS_MODULE
extern struct digest	sha_digest;
#endif

#ifdef BC160_FS_MODULE
extern struct digest	bc160_digest;
#endif

struct digest *digests[] =
{
#ifdef SHA_FS_MODULE
	&sha_digest,
#endif

#ifdef BC160_FS_MODULE
	&bc160_digest,
#endif

	(struct digest *)0
};

#ifdef NO_COMPILE
/*
 *  Copy input to output up to end of file,
 *  croaking if the input does not match the
 */
char *
digest_copy_eof(int in_fd, int out_fd)
{
	int nread;
	char buf[PIPE_MAX];
	char *status;

	if ((status = digest->init()))
		return status;
	while ((nread = uni_read(in_fd, buf, sizeof buf)) > 0) {
		status = digest->get_update();
		if (strlen(status) > 1)
			return status;
		if (status[0] == '0')
			break;
	}
	if
	if (status[0] != '0')
		return "digest_copy_eof: end of stream does not match digest";
	|| status[0] != '0')	//  no one char errors
		return status;

	if ((err = digest->sha_get_update()))
		return err;

	while ((nread = uni_read(input_fd, buf, sizeof buf)) > 0)
		if (!SHA1_Update(&sha_ctx, buf, nread))
			return "SHA1_Update(chunk) failed";
	if (nread < 0)
		return strerror(errno);
	if (!SHA1_Final(bin_digest, &sha_ctx))
		return "SHA1_Final() failed";
}
#endif
