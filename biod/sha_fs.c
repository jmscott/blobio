#ifdef SHA_FS_MODULE
/*
 *  Synopsis:
 *	Module that manages sha digested blobs in POSIX file system.
 *  Note:
 *  	The digest is logged during an error.  This is a security hole.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "openssl/opensslv.h"
#include "openssl/sha.h"
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
} boot_data;

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

static void
_panic(struct request *r, char *msg)
{
	char buf[MSG_SIZE];

	if (r)
		panic(log_strcpy3(buf, sizeof buf, r->verb,r->algorithm,msg));
	else
		panic2("sha", msg);
}

static void
_panic2(struct request *r, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_panic(r, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_panic3(struct request *r, char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	_panic(r, log_strcpy3(buf, sizeof buf, msg1, msg2, msg3));
}

static void
_error(struct request *r, char *msg)
{
	char buf[MSG_SIZE];

	if (r) {
		error(log_strcpy3(buf, sizeof buf, r->verb,r->algorithm,msg));
		if (r->digest)
			error2("digest", r->digest);
	} else
		error2("sha", msg);
}

static void
_error2(struct request *r, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_error(r, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_warn(struct request *r, char *msg)
{
	char buf[MSG_SIZE];

	warn(log_strcpy3(buf, sizeof buf, r->verb, r->algorithm, msg));
}

static void
_warn2(struct request *r, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_warn(r, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_warn3(struct request *r, char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	_warn2(r, log_strcpy2(buf, sizeof buf, msg1, msg2), msg3);
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
sha_fs_open(struct request *r)
{
	struct sha_fs_request *sp;

	sp = (struct sha_fs_request *)malloc(sizeof *sp);
	if (sp == NULL)
		_panic2(r, "malloc(sha_fs_request) failed", strerror(errno));
	memset(sp, 0, sizeof *sp);
	if (strcmp("wrap", r->verb))
		nib2digest(r->digest, sp->digest);
	r->open_data = (void *)sp;

	return 0;
}

static int
_unlink(struct request *r, char *path, int *exists)
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
		_panic2(r, buf, strerror(err));
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
zap_blob(struct request *r)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
	int exists = 0;

	if (_unlink(r, sp->blob_path, &exists)) {
		_panic(r, "zap_blob: _unlink() failed");
		return -1;
	}
	return 0;
}

/*
 *  Read a chunk from a local stream, reading up to end of either the stream
 *  or the chunk.
 */
static int
_read(struct request *r, int fd, unsigned char *buf, int buf_size)
{
	int nread;
	unsigned char *b, *b_end;

	b_end = buf + buf_size;
	b = buf;
	while (b < b_end) {
		nread = io_read(fd, b, b_end - b);
		if (nread < 0) {
			_error2(r, "read() failed", strerror(errno));
			return -1;
		}
		if (nread == 0)
			return b - buf;
		b += nread;
	}
	return buf_size;
}

static int
_close(struct request *r, int *p_fd)
{
	int fd = *p_fd;

	*p_fd = -1;
	if (io_close(fd))
		_panic2(r, "close() failed", strerror(errno));
	return 0;
}

/*
 *  Open a local file to read, retrying on interrupt and logging errors.
 */
static int
_open(struct request *r, char *path, int *p_fd)
{
	int fd;

	if ((fd = io_open(path, O_RDONLY, 0)) < 0) {
		char buf[MSG_SIZE];

		if (errno == ENOENT)
			return ENOENT;
		snprintf(buf, sizeof buf, "open(%s) failed: %s", path,
							strerror(errno));
		_panic(r, buf);
	}
	*p_fd = fd;
	return 0;
}

static int
_stat(struct request *r, char *path, struct stat *p_st)
{
	if (io_stat(path, p_st)) {
		char buf[MSG_SIZE];

		if (errno == ENOENT)
			return ENOENT;
		snprintf(buf, sizeof buf, "stat(%s) failed: %s", path,
							strerror(errno));
		_panic(r, buf);
	}
	return 0;
}

static int
_mkdir(struct request *r, char *path, int exists_ok)
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
			if (_stat(r, path, &st)) {
				_error(r, "_mkdir: _stat() failed");
				return ENOENT;
			}
			return S_ISDIR(st.st_mode) ? 0 : ENOTDIR;
		}
		snprintf(buf, sizeof buf, "mkdir(%s) failed: %s", path,
							strerror(err));
		_panic(r, buf);
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
blob_path(struct request *r, char *digest)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
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
sha_fs_get(struct request *r)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
	int status = 0;
	SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	blob_path(r, r->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(r, sp->blob_path, &fd)) {
	case 0:
		break;
	case ENOENT:
		return 1;
	default:
		_panic(r, "_open(blob) failed");
	}

	/*
	 *  Tell the client we have the blob.
	 */
	if (write_ok(r)) {
		_error(r, "write_ok() failed");
		goto croak;
	}

	SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 *
	 *  In principle, we ought to first scan the blob file
	 *  before sending "ok" to the requestor.
	 */
	while ((nread = _read(r, fd, chunk, sizeof chunk)) > 0) {
		if (blob_write(r, chunk, nread)) {
			_error(r, "blob_write(blob chunk) failed");
			goto croak;
		}
		/*
		 *  Update the incremental digest.
		 */
		SHA1_Update(&ctx, chunk, nread);
	}
	if (nread < 0)
		_panic(r, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	SHA1_Final(digest, &ctx);
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
		_error2(r, "PANIC: stored blob doesn't match digest",
								r->digest);
		if (zap_blob(r))
			_panic(r, "zap_blob() failed");
		goto croak;
	}
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (_close(r, &fd))
		_panic(r, "_close(blob) failed");
	return status;
}

/*
 *  Copy a local blob to a local stream.
 *
 *  Return 0 if stream matches signature, -1 otherwise.
 *  Note: this needs to be folded into sha_fs_get().
 */
static int
sha_fs_copy(struct request *r, int out_fd)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
	int status = 0;
	SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;
	static char n[] = "sha_fs_write";

	blob_path(r, r->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(r, sp->blob_path, &fd)) {
	case 0:
		break;
	case ENOENT:
		_warn3(r, n, "open(blob): not found", r->digest);
		return 1;
	default:
		_panic2(r, n, "_open(blob) failed");
	}

	SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file, write chunk to local stream,
	 *  update incremental digest.
	 */
	while ((nread = _read(r, fd, chunk, sizeof chunk)) > 0) {
		if (io_write_buf(out_fd, chunk, nread)) {
			_error2(r, n, "write_buf() failed");
			goto croak;
		}
			
		/*
		 *  Update the incremental digest.
		 */
		SHA1_Update(&ctx, chunk, nread);
	}
	if (nread < 0)
		_panic2(r, n, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	SHA1_Final(digest, &ctx);

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 */
	if (memcmp(sp->digest, digest, 20))
		_panic3(r, n, "stored blob doesn't match digest", r->digest);
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (_close(r, &fd))
		_panic2(r, n, "_close(blob) failed");
	return status;
}

static int
sha_fs_eat(struct request *r)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
	int status = 0;
	SHA_CTX ctx;
	unsigned char digest[20];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	blob_path(r, r->digest);

	/*
	 *  Open the file to the blob.
	 */
	switch (_open(r, sp->blob_path, &fd)) {
	case 0:
		break;
	/*
	 *  Blob not found.
	 */
	case ENOENT:
		return 1;
	default:
		_panic(r, "_open(blob) failed");
	}

	SHA1_Init(&ctx);

	/*
	 *  Read a chunk from the file and chew.
	 */
	while ((nread = _read(r, fd, chunk, sizeof chunk)) > 0)
		/*
		 *  Update the incremental digest.
		 */
		SHA1_Update(&ctx, chunk, nread);
	if (nread < 0)
		_panic(r, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	SHA1_Final(digest, &ctx);

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
		_panic2(r, "stored blob doesn't match digest", r->digest);
	if (_close(r, &fd))
		_panic(r, "_close(blob) failed");
	return status;
}

/*
 *  Write a portion of a blob to local storage and derive a partial digest.
 *  Return 1 if the accumulated digest matches the expected digest,
 *  0 if the partial digest does not match do not match.
 */
static int
eat_chunk(struct request *r, SHA_CTX *p_ctx, int fd, unsigned char *buf,
	  int buf_size)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;
	SHA_CTX ctx;
	unsigned char digest[20];

	/*
	 *  Update the incremental digest.
	 */
	SHA1_Update(p_ctx, buf, buf_size);

	/*
	 *  Write the chunk to the local temp file.
	 */
	if (io_write_buf(fd, buf, buf_size))
		_panic2(r, "eat_chunk: write(tmp) failed", strerror(errno));
	/*
	 *  Determine if we have seen the whole blob
	 *  by copying the incremental digest, finalizing it,
	 *  then comparing to the expected blob.
	 */
	memcpy(&ctx, p_ctx, sizeof *p_ctx);
	SHA1_Final(digest, &ctx);
	return memcmp(sp->digest, digest, 20) == 0 ? 1 : 0;
}

static int
sha_fs_put(struct request *r)
{
	SHA_CTX ctx;
	char tmp_path[MAX_FILE_PATH_LEN];
	unsigned char chunk[CHUNK_SIZE], *cp, *cp_end;
	int fd = -1;
	int status = 0;
	char buf[MSG_SIZE];

	/*
	 *  Open a temporary file in tmp to accumulate the
	 *  blob read from the client.  The file looks like
	 *
	 *	[put|give]-time-pid-digest
	 */
	snprintf(tmp_path, sizeof tmp_path, "%s/%s-%d-%u-%s",
					tmp_get(r->algorithm, r->digest),
					r->verb,
					/*
					 *  Warning:
					 *	Casting time() to int is
					 *	incorrect!!
					 */
					(int)time((time_t *)0),
					getpid(),
					r->digest);
	/*
	 *  Open the file ... need O_LARGEFILE support!!
	 *  Need to catch EINTR!!!!
	 */
	fd = io_open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND, S_IRUSR);
	if (fd < 0) {
		snprintf(buf, sizeof buf, "open(%s) failed: %s", tmp_path,
							strerror(errno));
		_panic(r, buf);
	}

	/*
	 *  Initialize digest of blob being scanned from the client.
	 */
	SHA1_Init(&ctx);

	/*
	 *  An empty blob is always put.
	 *  Note: the caller has already ensured that no more data has
	 *  been written by the client, so no need to check r->scan_size.
	 */
	if (strcmp(r->digest, empty_ascii) == 0)
		goto digested;

	/*
	 *  Copy what we have already read into the first chunk buffer.
	 *
	 *  If we've read ahead more than we can chew,
	 *  then croak.  This should never happen.
	 */
	if (r->scan_size > 0) {

		//  Note: regress, sanity test ... remove later.
		if ((u8)r->scan_size != r->blob_size)
			_panic(r, "r->scan_size != r->blob_size");

		if (r->scan_size > (int)(sizeof chunk - 1)) {
			snprintf(buf, sizeof buf, "max=%lu", 
					(long unsigned)(sizeof chunk - 1));
			_panic2(r, "scanned chunk too big", buf);
		}

		/*
		 *  See if the entire blob fits in the first read.
		 */
		if (eat_chunk(r, &ctx, fd, r->scan_buf, r->scan_size))
			goto digested;
	}
	cp = chunk;
	cp_end = &chunk[sizeof chunk];

	/*
	 *  Read more chunks until we see the blob.
	 */
again:
	while (cp < cp_end) {
		int nread = blob_read(r, cp, cp_end - cp);

		/*
		 *  Read error from client, 
		 *  so zap the partial, invalid blob.
		 */
		if (nread < 0) {
			_error(r, "blob_read() failed");
			goto croak;
		}
		if (nread == 0) {
			_error(r, "blob_read() returns 0 before digest seen");
			goto croak;
		}
		switch (eat_chunk(r, &ctx, fd, cp, nread)) {
		case -1:
			_panic(r, "eat_chunk(local) failed");
		case 1:
			goto digested;
		}
		cp += nread;
	}
	cp = chunk;
	goto again;

digested:
	if (fd >= 0)
		_close(r, &fd);

	/*
	 *  Rename the temp blob file to the final blob path.
	 */
	blob_path(r, r->digest);

	arbor_rename(tmp_path,
		((struct sha_fs_request *)r->open_data)->blob_path);
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (fd > -1)
		_panic(r, "_close() failed");
	if (tmp_path[0] && _unlink(r, tmp_path, (int *)0))
		_panic(r, "_unlink() failed");
	return status; 
}

static int
sha_fs_take_blob(struct request *r)
{
	return sha_fs_get(r);
}

/*
 *  Synopsis:
 *	Handle reply from client after client took blob.
 */
static int
sha_fs_take_reply(struct request *r, char *reply)
{
	struct sha_fs_request *sp = (struct sha_fs_request *)r->open_data;

	/*
	 *  If client replies 'ok', then delete the blob.
	 *  Eventually need to lock the file.
	 */
	if (*reply == 'o') {
		int exists = 1;
		char *slash;

		if (_unlink(r, sp->blob_path, &exists))
			_panic(r, "_unlink() failed");
		if (!exists)
			_warn2(r, "expected blob file does not exist",
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
			_panic2(r, "slash missing from blob path",
							sp->blob_path);
	}
	return 0;
}

/*
 *  Synopsis:
 *	Client is giving us a blob.
 */
static int
sha_fs_give_blob(struct request *r)
{
	return sha_fs_put(r);
}

/*
 *  Synopsis:
 *	Handle client reply from give_blob().
 */
static int
sha_fs_give_reply(struct request *r, char *reply)
{
	(void)r;
	(void)reply;
	return 0;
}

/*
 *  Digest a local blob stream and store the digested blob.
 */
static int
sha_fs_digest(struct request *r, int fd, char *hex_digest)
{
	char unsigned buf[4096], digest[20], *d, *d_end;
	char *h;
	SHA_CTX ctx;
	int nread;
	int tmp_fd = -1;
	char tmp_path[MAX_FILE_PATH_LEN];
	int status = 0;
	static int drift = 0;

	tmp_path[0] = 0;

	if (drift++ >= 999)
		drift = 0;

	/*
	 *  Open a temporary file in tmp to accumulate the
	 *  blob read from the stream.  The file looks like
	 *
	 *	digest-time-pid-drift
	 */
	snprintf(tmp_path, sizeof tmp_path, "%s/digest-%d-%u-%d",
					tmp_get(r->algorithm, r->digest),
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
		_panic3(r, "digest: open(tmp) failed", tmp_path,
						strerror(errno));
	SHA1_Init(&ctx);
	while ((nread = io_read(fd, buf, sizeof buf)) > 0) {
		SHA1_Update(&ctx, buf, nread);
		if (io_write_buf(tmp_fd, buf, nread) != 0)
			_panic2(r, "digest: write_buf(tmp) failed",
						strerror(errno));
	}
	if (nread < 0) {
		_error(r, "digest: _read() failed");
		goto croak;
	}
	SHA1_Final(digest, &ctx);

	status = io_close(tmp_fd);
	tmp_fd = -1;
	if (status)
		_panic2(r,"digest: close(tmp) failed",strerror(errno));

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
	blob_path(r, hex_digest);
	arbor_move(tmp_path,
		((struct sha_fs_request *)r->open_data)->blob_path);
	tmp_path[0] = 0;

	goto cleanup;
croak:
	status = -1;
cleanup:
	if (tmp_fd > -1 && io_close(tmp_fd))
		_panic2(r, "digest: close(tmp) failed", strerror(errno));
	if (tmp_path[0] && io_unlink(tmp_path))
		_panic3(r, "digest: unlink(tmp) failed", tmp_path,
						strerror(errno));
	return status;
}

static int
sha_fs_close(struct request *r, int status)
{
	(void)r;
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
	binfo2("openssl sha1 hash algorithm", OPENSSL_VERSION_TEXT);

	strcpy(boot_data.root_dir_path, sha_fs_root);
	binfo2("fs root directory", boot_data.root_dir_path);

	/*
	 *  Verify or create root directory path.
	 */
	if (_mkdir((struct request *)0, boot_data.root_dir_path, 1))
		panic("sha: boot: _mkdir(root_dir) failed");

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
	.copy		=	sha_fs_copy,

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
