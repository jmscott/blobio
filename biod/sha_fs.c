#ifdef SHA_FS_MODULE
/*
 *  Synopsis:
 *	Module that manages sha digested blobs in POSIX file system.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 *  Note:
 *  	The digest is logged during an error.  This is a security hole.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "../sha1/sha1.h"
#include "biod.h"

/*
 *  How many bytes to read and write the blobs from either client or fs.
 *  Note: might want to make the get chunk size be much bigger than the put.
 */
#define CHUNK_SIZE		(64 * 1024)

extern char			*getenv();

static char	sha_fs_root[]	= "data/sha_fs";
static char	empty_ascii[]	= "da39a3ee5e6b4b0d3255bfef95601890afd80709";

struct sha_fs_request
{
	/*
	 *  Directory path to where blob file will be stored,
	 *  relative to BLOBIO_ROOT directory.
	 */
	char		blob_dir_path[MAX_FILE_PATH_LEN];
	char		blob_path[MAX_FILE_PATH_LEN];
	unsigned char	digest[20];
};

static struct sha_fs_boot
{
	char		root_dir_path[MAX_FILE_PATH_LEN];
	char		tmp_dir_path[MAX_FILE_PATH_LEN];
} boot_data;

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static void
_panic(struct request *rp, char *msg)
{
	char buf[MSG_SIZE];

	if (rp)
		panic(log_strcpy3(buf, sizeof buf, rp->verb,rp->algorithm,msg));
	else
		panic2("sha", msg);
}

static void
_panic2(struct request *rp, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_panic(rp, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_panic3(struct request *rp, char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	_panic(rp, log_strcpy3(buf, sizeof buf, msg1, msg2, msg3));
}

static void
_error(struct request *rp, char *msg)
{
	char buf[MSG_SIZE];

	if (rp) {
		error(log_strcpy3(buf, sizeof buf, rp->verb,rp->algorithm,msg));
		if (rp->digest)
			error2("digest", rp->digest);
	} else
		error2("sha", msg);
}

static void
_error2(struct request *rp, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_error(rp, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_warn(struct request *rp, char *msg)
{
	char buf[MSG_SIZE];

	warn(log_strcpy3(buf, sizeof buf, rp->verb, rp->algorithm, msg));
}

static void
_warn2(struct request *rp, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_warn(rp, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_warn3(struct request *rp, char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	_warn2(rp, log_strcpy2(buf, sizeof buf, msg1, msg2), msg3);
}

static void
nib2digest(char *hex_digest, unsigned char *digest)
{
	char *h = hex_digest;
	unsigned char *d = digest;
	int i;

	/*
	 *  Derive the 20 byte, binary digest from the 40 
	 *  character version.
	 */
	for (i = 0;  i < 40;  i++) {
		char c = *h++;
		unsigned char nib;

		if (c >= '0' && c <= '9')
			nib = c - '0';
		else
			nib = c - 'a' + 10;
		if ((i & 1) == 0)
			nib <<= 4;
		d[i >> 1] |= nib;
	}
}

static int
sha_fs_open(struct request *rp)
{
	struct sha_fs_request *sp;

	sp = (struct sha_fs_request *)malloc(sizeof *sp);
	if (sp == NULL)
		_panic2(rp, "malloc(sha_fs_request) failed", strerror(errno));
	memset(sp, 0, sizeof *sp);
	if (strcmp("wrap", rp->verb))
		nib2digest(rp->digest, sp->digest);
	rp->open_data = (void *)sp;

	return 0;
}

static int
_unlink(struct request *rp, char *path, int *exists)
{
	if (io_unlink(path)) {
		char buf[MSG_SIZE];
		int err = errno;

		if (err == ENOENT) { 
			if (exists)
				*exists = 0;
			return 0;
		}
		snprintf(buf, sizeof buf, "unlink(%s) failed", path);
		_panic2(rp, buf, strerror(err));
	}
	if (exists)
		*exists = 1;
	return 0;
}

/*
 *  Do a hard delete of a corrupted blob.
 *  Calling zap blob indicates a panicy situation with the server.
 *  Eventually will want to remove an empty, enclosing directory.
 */
static int
zap_blob(struct request *rp)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	int exists = 0;

	if (_unlink(rp, sp->blob_path, &exists)) {
		_panic(rp, "zap_blob: _unlink() failed");
		return -1;
	}
	return 0;
}

static int
_read(struct request *rp, int fd, unsigned char *buf, int buf_size)
{
	int nread;
	unsigned char *b, *b_end;

	b_end = buf + buf_size;
	b = buf;
	while (b < b_end) {
		nread = io_read(fd, b, b_end - b);
		if (nread < 0) {
			_error2(rp, "read() failed", strerror(errno));
			return -1;
		}
		if (nread == 0)
			return b - buf;
		b += nread;
	}
	return buf_size;
}

static int
_close(struct request *rp, int *p_fd)
{
	int fd = *p_fd;

	*p_fd = -1;
	if (io_close(fd))
		_panic2(rp, "close() failed", strerror(errno));
	return 0;
}

/*
 *  Open a file to read, retrying on interupt and logging errors.
 */
static int
_open(struct request *rp, char *path, int *p_fd)
{
	int fd;

	if ((fd = io_open(path, O_RDONLY, 0)) < 0) {
		char buf[MSG_SIZE];

		if (errno == ENOENT)
			return ENOENT;
		snprintf(buf, sizeof buf, "open(%s) failed: %s", path,
							strerror(errno));
		_panic(rp, buf);
	}
	*p_fd = fd;
	return 0;
}

static int
_stat(struct request *rp, char *path, struct stat *p_st)
{
	if (io_stat(path, p_st)) {
		char buf[MSG_SIZE];

		if (errno == ENOENT)
			return ENOENT;
		snprintf(buf, sizeof buf, "stat(%s) failed: %s", path,
							strerror(errno));
		_panic(rp, buf);
	}
	return 0;
}

static int
_mkdir(struct request *rp, char *path, int exists_ok)
{
	if (io_mkdir(path, S_IRWXU | S_IXGRP)) {
		char buf[MSG_SIZE];
		int err = errno;

		if (err == EEXIST) {
			struct stat st;

			if (!exists_ok)
				return EEXIST;

			/*
			 *  Verify the path is a directory.
			 */
			if (_stat(rp, path, &st)) {
				_error(rp, "_mkdir: _stat() failed");
				return ENOENT;
			}
			return S_ISDIR(st.st_mode) ? 0 : ENOTDIR;
		}
		snprintf(buf, sizeof buf, "mkdir(%s) failed: %s", path,
							strerror(err));
		_panic(rp, buf);
	}
	return 0;
}

/*
 *  Assemble the blob path, optionally creating the directory entries.
 *
 *  Make the 5 subdirectories that comprise the path to a blob
 *  and the entire path to the blob file.
 */
static void
blob_path(struct request *rp, char *digest)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	char *p, *q;

	/*
	 *  Derive a path to the blob from the digest.
	 *  Each directory component in the path doubles the
	 *  length of it's parent component.  For example, given
	 *  the digest
	 *
	 *	57cd5957fbc764c5ee9862f76287d09d2170b9ef
	 *
	 *  we derive the path to the blob file
	 *
	 *	5/7c/d595/7fbc764c/5ee9862f76287d09/d2170b9ef
	 *
	 *  where the final component, 'd2170b9ef' is the file containing
	 *  the blob.
	 */
	strcpy(sp->blob_dir_path, boot_data.root_dir_path);
	p = sp->blob_dir_path + strlen(sp->blob_dir_path);
	q = digest;

	/*
	 *   Build the directory path to the blob.
	 */

	 /*
	  *  Directory 1:
	  *	Single character /[0-9a-f]
	  */
	*p++ = '/';
	*p++ = *q++;

	/*
	 *  Directory 2:
	 *	2 Character: /[0-9a-f][0-9a-f]
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;

	/*
	 *  Directory 3:
	 *	4 Characters: /[0-9a-f][0-9a-f][0-9a-f][0-9a-f]
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;

	/*
	 *  Directory 4:
	 *	8 Characters: /[0-9a-f]{8}
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;

	/*
	 *  Directory 5:
	 *	16 Characters: /[0-9a-f]{16}
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p = 0;

	/*
	 *  Build the path to the final resting place of the blob file.
	 */
	strcpy(sp->blob_path, sp->blob_dir_path);
	strcat(sp->blob_path, "/");
	strcat(sp->blob_path, digest + 31);
}

static int
sha_fs_get(struct request *rp)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	int status = 0;
	blk_SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	blob_path(rp, rp->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(rp, sp->blob_path, &fd)) {
	case 0:
		break;
	case ENOENT:
		_warn2(rp, "not found", rp->digest);
		return 1;
	default:
		_panic(rp, "_open(blob) failed");
	}

	/*
	 *  Tell the client we have the blob.
	 */
	if (write_ok(rp)) {
		_error(rp, "write_ok() failed");
		goto croak;
	}

	blk_SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 *
	 *  In principle, we ought to first scan the blob file
	 *  before sending "ok" to the requestor.
	 */
	while ((nread = _read(rp, fd, chunk, sizeof chunk)) > 0) {
		if (write_blob(rp, chunk, nread)) {
			_error(rp, "write_blob() failed");
			goto croak;
		}
		/*
		 *  Update the incremental digest.
		 */
		blk_SHA1_Update(&ctx, chunk, nread);
	}
	if (nread < 0)
		_panic(rp, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	blk_SHA1_Final(digest, &ctx);
	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 *
	 *  Note: unfortunately we've already deceived the client
	 *        by sending "ok".  Probably need to improve for
	 *	  the special case when the entire blob is read
	 *        in first chunk.
	 */
	if (memcmp(sp->digest, digest, 20)) {
		_error2(rp, "PANIC: stored blob doesn't match digest",
								rp->digest);
		if (zap_blob(rp))
			_panic(rp, "zap_blob() failed");
		goto croak;
	}
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (_close(rp, &fd))
		_panic(rp, "_close(blob) failed");
	return status;
}

/*
 *  Write a blob to a stream.
 *  Return 0 if stream matches signature, -1 otherwise.
 *  Note: this needs to be folded into sha_fs_get().
 */
static int
sha_fs_write(struct request *rp, int out_fd)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	int status = 0;
	blk_SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;
	static char nm[] = "sha_fs_write";

	blob_path(rp, rp->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(rp, sp->blob_path, &fd)) {
	case 0:
		break;
	case ENOENT:
		_warn3(rp, nm, "open(blob): not found", rp->digest);
		return 1;
	default:
		_panic2(rp, nm, "_open(blob) failed");
	}

	blk_SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 */
	while ((nread = _read(rp, fd, chunk, sizeof chunk)) > 0) {
		if (write_buf(out_fd, chunk, nread, rp->write_timeout)) {
			_error2(rp, nm, "write_buf() failed");
			goto croak;
		}
		/*
		 *  Update the incremental digest.
		 */
		blk_SHA1_Update(&ctx, chunk, nread);
	}
	if (nread < 0)
		_panic2(rp, nm, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	blk_SHA1_Final(digest, &ctx);

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 */
	if (memcmp(sp->digest, digest, 20))
		_panic3(rp, nm, "stored blob doesn't match digest", rp->digest);
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (_close(rp, &fd))
		_panic2(rp, nm, "_close(blob) failed");
	return status;
}

static int
sha_fs_eat(struct request *rp)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	int status = 0;
	blk_SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	blob_path(rp, rp->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(rp, sp->blob_path, &fd)) {
	case 0:
		break;
	/*
	 *  Blob not found.
	 */
	case ENOENT:
		/*
		 *  Don't log missiing entries on eat.
		 *  Shouldn't the caller determine if logging should be done.
		_warn2(rp, "not found", rp->digest);
		*/
		return 1;
	default:
		_panic(rp, "_open(blob) failed");
	}

	blk_SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file and chew.
	 */
	while ((nread = _read(rp, fd, chunk, sizeof chunk)) > 0)
		/*
		 *  Update the incremental digest.
		 */
		blk_SHA1_Update(&ctx, chunk, nread);
	if (nread < 0)
		_panic(rp, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	blk_SHA1_Final(digest, &ctx);

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 *
	 *  Note: unfortunately we've already deceived the client
	 *        by sending "ok".  Probably need to improve for
	 *	  the special case when the entire blob is read
	 *        in first chunk.
	 */
	if (memcmp(sp->digest, digest, 20))
		_panic2(rp, "stored blob doesn't match digest", rp->digest);
	if (_close(rp, &fd))
		_panic(rp, "_close(blob) failed");
	return status;
}

static int
do_write(struct request *rp, int fd, unsigned char *buf, int buf_size)
{
	int nwrite;
	unsigned char *b, *b_end;

	b_end = buf + buf_size;
	b = buf;
	while (b < b_end) {
		nwrite = io_write(fd, b, b_end - b);
		if (nwrite < 0)
			_panic2(rp, "write(blob) failed", strerror(errno));
		if (nwrite == 0)
			_panic(rp, "write(blob) returned unexpected 0");
		b += nwrite;
	}
	return 0;
}

/*
 *  Write a portion of a blob to local storage and derive a partial digest.
 *  Return 1 if the accumulated digest matches the expected digest,
 *  0 if the do not match, and -1 on error.
 */
static int
eat_chunk(struct request *rp, blk_SHA_CTX *p_ctx, int fd, unsigned char *buf,
	  int buf_size)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;
	blk_SHA_CTX ctx;
	unsigned char digest[20];

	/*
	 *  Update the incremental digest.
	 */
	blk_SHA1_Update(p_ctx, buf, buf_size);

	/*
	 *  Write the chunk to the temp file.
	 */
	if (do_write(rp, fd, buf, buf_size))
		_panic(rp, "eat_chunk: do_write() failed");
	/*
	 *  Determine if we have seen the whole blob
	 *  by copying the incremental digest, finalizing it,
	 *  then comparing to the expected blob.
	 */
	memcpy(&ctx, p_ctx, sizeof *p_ctx);
	blk_SHA1_Final(digest, &ctx);
	return memcmp(sp->digest, digest, 20) == 0 ? 1 : 0;
}

static int
sha_fs_put(struct request *rp)
{
	blk_SHA_CTX ctx;
	char tmp_path[MAX_FILE_PATH_LEN];
	unsigned char chunk[CHUNK_SIZE], *cp, *cp_end;
	int fd;
	int status = 0;
	char buf[MSG_SIZE];

	/*
	 *  Open a temporary file in $sha_fs_root/tmp to accumulate the
	 *  blob read from the client.  The file looks like
	 *
	 *	[put|give]-time-pid-digest
	 */
	snprintf(tmp_path, sizeof tmp_path, "%s/%s-%d-%u-%s",
					boot_data.tmp_dir_path,
					rp->verb,
					/*
					 *  Warning:
					 *	Casting time() to int is
					 *	incorrect!!
					 */
					(int)time((time_t *)0),
					getpid(),
					rp->digest);
	/*
	 *  Open the file ... need O_LARGEFILE support!!
	 *  Need to catch EINTR!!!!
	 */
	fd = io_open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND, S_IRUSR);
	if (fd < 0) {
		snprintf(buf, sizeof buf, "open(%s) failed: %s", tmp_path,
							strerror(errno));
		_panic(rp, buf);
	}

	/*
	 *  Initialize digest of blob being scanned from the client.
	 */
	blk_SHA1_Init(&ctx);

	/*
	 *  An empty blob is always put.
	 *  Note: the caller has already ensured that no more data has
	 *  been written by the client, so no need to check rp->scan_size.
	 */
	if (strcmp(rp->digest, empty_ascii) == 0)
		goto digested;

	/*
	 *  Copy what we have already read into the first chunk buffer.
	 *
	 *  If we've read ahead more than we can chew,
	 *  then croak.  This should never happen.
	 */
	if (rp->scan_size > 0) {
		if (rp->scan_size > (int)(sizeof chunk - 1)) {
			snprintf(buf, sizeof buf, "max=%lu", 
					(long unsigned)(sizeof chunk - 1));
			_panic2(rp, "scanned chunk too big", buf);
		}

		/*
		 *  See if the entire blob fits in the first read.
		 */
		switch (eat_chunk(rp, &ctx, fd, rp->scan_buf, rp->scan_size)) {
		case -1:
			_panic(rp, "eat_chunk() failed");
		case 1:
			goto digested;
		}
	}
	cp = chunk;
	cp_end = &chunk[sizeof chunk];

	/*
	 *  Read more chunks until we see the blob.
	 */
again:
	while (cp < cp_end) {
		int nread = read_blob(rp, cp, cp_end - cp);

		/*
		 *  Read error from client, 
		 *  so zap the partial, invalid blob.
		 */
		if (nread < 0) {
			_error(rp, "read_blob() failed");
			goto croak;
		}
		if (nread == 0) {
			_error(rp, "read_blob() empty");
			goto croak;
		}
		switch (eat_chunk(rp, &ctx, fd, cp, nread)) {
		case -1:
			_panic(rp, "eat_chunk() failed");
		case 1:
			goto digested;
		}
		cp += nread;
	}
	cp = chunk;
	goto again;

digested:
	if (_close(rp, &fd))
		_panic(rp, "_close(blob) failed");
	/*
	 *  Move the temp blob file to the final blob path.
	 */
	blob_path(rp, rp->digest);
	arbor_rename(tmp_path,
		((struct sha_fs_request *)rp->open_data)->blob_path);
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (fd > -1 && _close(rp, &fd))
		_panic(rp, "_close() failed");
	if (tmp_path[0] && _unlink(rp, tmp_path, (int *)0))
		_panic(rp, "_unlink() failed");
	return status; 
}

static int
sha_fs_take_blob(struct request *rp)
{
	return sha_fs_get(rp);
}

/*
 *  Synopsis:
 *	Handle reply from client after client took blob.
 */
static int
sha_fs_take_reply(struct request *rp, char *reply)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)rp->open_data;

	/*
	 *  If client replies 'ok', then delete the blob.
	 *  Eventually need to lock the file.
	 */
	if (*reply == 'o') {
		int exists = 1;
		char *slash;

		if (_unlink(rp, sp->blob_path, &exists))
			_panic(rp, "_unlink() failed");
		if (!exists)
			_warn2(rp, "expected blob file does not exist",
							sp->blob_path);
		/*
		 *  Request trimming the empty directories.
		 */
		if ((slash = rindex(sp->blob_path, '/'))) {
			*slash = 0;
			arbor_trim(sp->blob_path);
			*slash = '/';
		}
		else
			_panic2(rp, "slash missing from blob path",
							sp->blob_path);
	}
	return 0;
}

/*
 *  Synopsis:
 *	Client is giving us a blob.
 */
static int
sha_fs_give_blob(struct request *rp)
{
	return sha_fs_put(rp);
}

/*
 *  Synopsis:
 *	Handle client reply from give_blob().
 */
static int
sha_fs_give_reply(struct request *rp, char *reply)
{
	(void)rp;
	(void)reply;
	return 0;
}

/*
 *  Digest a blob stream and store the digested blob.
 */
static int
sha_fs_digest(struct request *rp, int fd, char *hex_digest, int do_put)
{
	char unsigned buf[4096], digest[20], *d, *d_end;
	char *h;
	blk_SHA_CTX ctx;
	int nread;
	int tmp_fd = -1;
	char tmp_path[MAX_FILE_PATH_LEN];
	int status = 0;

	tmp_path[0] = 0;
	if (do_put) {
		static int drift = 0;

		if (drift++ >= 999)
			drift = 0;

		/*
		 *  Open a temporary file in $sha_fs_root/tmp to accumulate the
		 *  blob read from the stream.  The file looks like
		 *
		 *	digest-time-pid-drift
		 */
		snprintf(tmp_path, sizeof tmp_path, "%s/digest-%d-%u-%d",
						boot_data.tmp_dir_path,
						/*
						 *  Warning:
						 *	Casting time() to int is
						 *	incorrect!!
						 */
						(int)time((time_t *)0),
						getpid(), drift);
		/*
		 *  Open the file ... need O_LARGEFILE support!!
		 *  Need to catch EINTR!!!!
		 */
		tmp_fd = io_open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND,
								S_IRUSR);
		if (tmp_fd < 0)
			_panic3(rp, "digest: open(tmp) failed", tmp_path,
							strerror(errno));
	}
	blk_SHA1_Init(&ctx);
	while ((nread = read_buf(fd, buf, sizeof buf, rp->read_timeout)) > 0) {
		blk_SHA1_Update(&ctx, buf, nread);
		if (do_put && write_buf(tmp_fd, buf, nread, rp->write_timeout))
			_panic(rp, "digest: write_buf(tmp) failed");
	}
	if (nread < 0) {
		_error(rp, "digest: _read() failed");
		goto croak;
	}
	blk_SHA1_Final(digest, &ctx);

	if (do_put) {
		status = io_close(tmp_fd);
		tmp_fd = -1;
		if (status)
			_panic2(rp,"digest: close(tmp) failed",strerror(errno));
	}

	/*
	 *  Convert the binary sha digest to text.
	 */
	h = hex_digest;
	d = digest;
	d_end = d + 20;
	while (d < d_end) {
		*h++ = nib2hex[(*d & 0xf0) >> 4];
		*h++ = nib2hex[*d & 0xf];
		d++;
	}
	*h = 0;

	/*
	 *  Move the blob from the temporary file to the blob file.
	 */
	if (do_put) {
		blob_path(rp, hex_digest);
		arbor_rename(tmp_path,
			((struct sha_fs_request *)rp->open_data)->blob_path);
		tmp_path[0] = 0;
	}

	goto cleanup;
croak:
	status = -1;
cleanup:
	if (tmp_fd > -1 && io_close(tmp_fd))
		_panic2(rp, "digest: close(tmp) failed", strerror(errno));
	if (tmp_path[0] && io_unlink(tmp_path))
		_panic3(rp, "digest: unlink(tmp) failed", tmp_path,
						strerror(errno));
	return status;
}

static int
sha_fs_close(struct request *rp, int status)
{
	(void)rp;
	(void)status;
	return 0;
}

static void
binfo(char *msg)
{
	info2("sha: boot", msg);
}

static void
binfo2(char *msg1, char *msg2)
{
	info3("sha: boot", msg1, msg2);
}

static int
sha_fs_boot()
{
	binfo("starting");
	binfo("storage is file system");
	binfo("git blk_SHA1() hash algorithm");

	strcpy(boot_data.root_dir_path, sha_fs_root);
	binfo2("fs root directory", boot_data.root_dir_path);

	/*
	 *  Verify or create root directory path.
	 */
	if (_mkdir((struct request *)0, boot_data.root_dir_path, 1))
		panic("sha: boot: _mkdir(root_dir) failed");

	/*
	 *  Create the temporary scratch directory data/sha_fs/tmp
	 */
	strcpy(boot_data.tmp_dir_path, boot_data.root_dir_path);
	strcat(boot_data.tmp_dir_path, "/tmp");

	if (_mkdir((struct request *)0, boot_data.tmp_dir_path, 1))
		panic("sha: boot: _mkdir(tmp) failed");
	binfo2("sha temporary directory", boot_data.tmp_dir_path);
	return 0;
}

static int
sha_fs_is_digest(char *digest)
{
	static char empty[] = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
	char *d, *d_end;

	if (strlen(digest) != 40)
		return 0;
	d = digest;
	d_end = d + 40;
	while (d < d_end) {
		int c = *d++;

		if (('0' <= c&&c <= '9') || ('a' <= c&&c <= 'f'))
			continue;
		return 0;
	}
	if (digest[0] == 'd' && digest[1] == 'a' && digest[2] == '3')
		return strcmp(digest, empty) == 0 ? 2 : 1;
	return 1;
}

static int
sha_fs_shutdown()
{
	info("sha: shutdown: bye");
	return 0;
}

struct digest_module sha_fs_module =
{
	.name		=	"sha",

	.boot		=	sha_fs_boot,
	.open		=	sha_fs_open,

	.get		=	sha_fs_get,
	.take_blob	=	sha_fs_take_blob,
	.take_reply	=	sha_fs_take_reply,
	.write		=	sha_fs_write,

	.put		=	sha_fs_put,
	.give_blob	=	sha_fs_give_blob,
	.give_reply	=	sha_fs_give_reply,

	.eat		=	sha_fs_eat,
	.digest		=	sha_fs_digest,
	.is_digest	=	sha_fs_is_digest,

	.close		=	sha_fs_close,
	.shutdown	=	sha_fs_shutdown
};

#endif
