#ifdef FS_BTC20_MODULE
/*
 *  Synopsis:
 *	Module of bitcoin inspired hash ripemd160(sha256(sha256(blob)) in POSIX files.
 *  Description:
 *	Implement a digest inspired by bitcoin wallet:
 *
 * 		openssl dgst -binary -sha256 |
 * 			openssl dgst -binary -sha256 |
 *			openssl dgst -ripemd160 -r | cut -c 1-40
 *
 *  Note:
 *	Code needs to refactoring for common posix functions, like hex
 *	string manipulation.
 *
 *	In boot up, verify dir data/fs_btc20/tmp is on the same device as dir
 *	data/fs_btc20.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "openssl/opensslv.h"
#include "openssl/sha.h"
#include "openssl/ripemd.h"
#include "bio4d.h"

/*
 *  How many bytes to read and write the blobs from either client or fs.
 */
#define CHUNK_SIZE		(4 * 1024)

static char	fs_btc20_root[]	= "data/fs_btc20";
static char	empty_ascii[]	= "fd7b15dc5dc2039556693555c2b81b36c8deec15";

typedef struct
{
	SHA256_CTX	sha256;
	SHA256_CTX	sha256_sha256;
	RIPEMD160_CTX	ripemd160;
} BTC20_CTX;

struct fs_btc20_request
{
	/*
	 *  Directory path to where blob file will be stored,
	 *  relative to BLOBIO_ROOT directory.
	 */
	char		blob_dir_path[MAX_FILE_PATH_LEN];
	char		blob_path[MAX_FILE_PATH_LEN];
	unsigned char	digest[20];
	int		blob_fd;
};

static struct fs_btc20_boot
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

	if (r) {
		if (r->step)
			panic(log_strcpy4(
				buf,
				sizeof buf,
				r->verb,
				r->step,
				r->algorithm,
				msg
			));
		else
			panic(log_strcpy3(
				buf,
				sizeof buf,
				r->verb,
				r->algorithm,
				msg
			));
	} else
		panic2("btc20", msg);
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
		if (r->step)
			error(log_strcpy4(
				buf,
				sizeof buf,
				r->verb,
				r->step,
				r->algorithm,
				msg
			));
		else
			error(log_strcpy3(
				buf,
				sizeof buf,
				r->verb,
				r->algorithm,
				msg
			));
	} else
		error2("btc20", msg);
}

static void
_error2(struct request *r, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_error(r, log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
_error3(struct request *r, char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	_error2(r, msg1, log_strcpy2(buf, sizeof buf, msg2, msg3));
}

static void
_warn(struct request *r, char *msg)
{
	char buf[MSG_SIZE];

	if (r->step)
		warn(log_strcpy4(
			buf,
			sizeof buf,
			r->verb,
			r->step,
			r->algorithm,
			msg
		));
	else
		warn(log_strcpy3(buf, sizeof buf, r->verb, r->algorithm, msg));
}

static void
_warn2(struct request *r, char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	_warn(r, log_strcpy2(buf, sizeof buf, msg1, msg2));
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
fs_btc20_open(struct request *r)
{
	struct fs_btc20_request *s;

	//  why malloc()?
	s = (struct fs_btc20_request *)malloc(sizeof *s);
	if (s == NULL)
		_panic2(r, "malloc(fs_btc20_request) failed", strerror(errno));
	memset(s, 0, sizeof *s);
	s->blob_fd = -1;
	if (strcmp("wrap", r->verb))
		nib2digest(r->digest, s->digest);
	r->open_data = (void *)s;

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
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int exists = 0;

	if (_unlink(r, s->blob_path, &exists)) {
		_panic(r, "zap_blob: _unlink() failed");
		return -1;
	}
	return 0;
}

/*
 *  Read a chunk from a local stream, reading up to either the end of the stream
 *  or the end of the digested chunk.
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

	if (fd < 0)
		return 0;
	*p_fd = -1;
	if (io_close(fd))
		_panic2(r, "close() failed", strerror(errno));
	return 0;
}

/*
 *  Open a local file to read.
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
 *  Possibly create the 2 subdirectories that comprise the path to a blob
 *  and the string of the path to the blob file.
 */
static void
make_path(struct request *r, char *digest)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	char *p, *q;

	/*
	 *  Derive a path to the blob from the digest.
	 *  Each directory component in the path doubles the
	 *  length of it's parent component.  For example, given
	 *  the digest
	 *
	 *	57cd5957fbc764c5ee9862f76287d09d2170b9ef
	 *
	 *  we derive the path to the blob file as
	 *
	 *	57c/d59/57cd5957fbc764c5ee9862f76287d09d2170b9ef
	 */
	strcpy(s->blob_dir_path, boot_data.root_dir_path);
	p = s->blob_dir_path + strlen(s->blob_dir_path);
	q = digest;

	/*
	 *   Build the directory path to the blob.
	 */

	 /*
	  *  Directory 1:
	  *	Three characters /[0-9a-f][0-9a-f][0-9a-f]
	  */
	*p++ = '/';
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;

	/*
	 *  Directory 2:
	 *	3 Characters: /[0-9a-f][0-9a-f]
	 */
	*p++ = '/';
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p++ = '/';

	/*
	 *  Build the path to the final resting place of the blob file.
	 */
	s->blob_path[0] = 0;
	jmscott_strcat2(s->blob_path, sizeof s->blob_path,
			s->blob_dir_path,
			digest
	);
}

static int
fs_btc20_get_request(struct request *r)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;

	make_path(r, r->digest);
	return _open(r, s->blob_path, &s->blob_fd);
}

static int
fs_btc20_get_trusted_bytes(struct request *r)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int status = 0;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 *
	 *  In principle, we ought to first scan the blob file
	 *  before sending "ok" to the requestor.
	 */
	while ((nread = _read(r, s->blob_fd, chunk, sizeof chunk)) > 0) {
		if (blob_write(r, chunk, nread))
			goto croak;
	}
	if (nread < 0)
		_panic(r, "_read(blob) failed");
	goto cleanup;
croak:
	status = -1;
cleanup:
	_close(r, &s->blob_fd);
	return status;
}

static int
fs_btc20_get_bytes(struct request *r)
{
	if (trust_fs == 1)
		return fs_btc20_get_trusted_bytes(r);

	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int status = 0;
	BTC20_CTX ctx;
	unsigned char sha_digest[32];
	unsigned char digest[20];
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	if (!SHA256_Init(&ctx.sha256))
		_panic(r, "SHA256_Init() failed");

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 *
	 *  In principle, we ought to first scan the blob file
	 *  before sending "ok" to the requestor.
	 */
	while ((nread = _read(r, s->blob_fd, chunk, sizeof chunk)) > 0) {
		if (blob_write(r, chunk, nread))
			goto croak;
		/*
		 *  Update the incremental digest.
		 */
		if (!SHA256_Update(&ctx.sha256, chunk, nread))
			_panic(r, "SHA256_Update(write) failed");
	}
	if (nread < 0)
		_panic(r, "_read(blob) failed");
	/*
	 *  Finalize the SHA256 digest of the entire blob.
	 */
	if (!SHA256_Final(sha_digest, &ctx.sha256))
		_panic(r, "SHA256_Final() failed");

	/*
	 *  Take the sha256 of the 32 byte sha256 of the entire blob
	 */
	unsigned char sha_sha_digest[32];
	if (!SHA256_Init(&ctx.sha256_sha256))
		_panic(r, "SHA256_Init(sha256) failed");
	if (!SHA256_Update(&ctx.sha256_sha256, sha_digest, 32))
		_panic(r, "SHA256_Update(sha256) failed");
	if (!SHA256_Final(sha_sha_digest, &ctx.sha256_sha256))
		_panic(r, "SHA256_Final(sha256) failed");

	if (!RIPEMD160_Init(&ctx.ripemd160))
		_panic(r, "RIPEMD160_Init() failed");
	if (!RIPEMD160_Update(&ctx.ripemd160, sha_sha_digest, 32))
		_panic(r, "RIPEMD160_Update(SHA256) failed");
	if (!RIPEMD160_Final(digest, &ctx.ripemd160))
		_panic(r, "RIPEMD160_Final(SHA256) failed");

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 *
	 *  Note:
	 *	No record of the deleted blob will exist until the next failed
	 *	get/take/eat request, which is problematic.
	 */
	if (memcmp(s->digest, digest, 20)) {
		_error2(r, "PANIC: fs blob fails udig digest", r->digest);
		if (zap_blob(r))
			_panic(r, "zap_blob(get_bytes) failed");
		_panic2(r, "zapped corrupted blob file", s->blob_path);
	}
	goto cleanup;
croak:
	status = -1;
cleanup:
	_close(r, &s->blob_fd);
	return status;
}

/*
 *  Copy a local blob to a local stream.
 *
 *  Return 0 if stream matches signature, -1 otherwise.
 *  Note: this needs to be folded into fs_btc20_get().
 */
static int
fs_btc20_trusted_copy(struct request *r, int out_fd)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int status = 0;
	unsigned char chunk[CHUNK_SIZE];
	int nread;
	static char n[] = "fs_btc20_trusted_copy";

	make_path(r, r->digest);

	/*
	 *  Open the file to the blob.
	 */
	if (_open(r, s->blob_path, &s->blob_fd) == ENOENT)
		return 1;

	/*
	 *  Read a chunk from the file, write chunk to local stream,
	 *  update incremental digest.
	 */
	while ((nread = _read(r, s->blob_fd, chunk, sizeof chunk)) > 0)
		if (io_write_buf(out_fd, chunk, nread)) {
			_error2(r, n, "write_buf() failed");
			goto croak;
		}
	if (nread < 0)
		_panic2(r, n, "_read(blob) failed");

	goto cleanup;
croak:
	status = -1;
cleanup:
	_close(r, &s->blob_fd);
	return status;
}

/*
 *  Copy a local blob to a local stream.
 *
 *  Return 0 if stream matches signature, -1 otherwise.
 *  Note: this needs to be folded into fs_btc20_get().
 */
static int
fs_btc20_copy(struct request *r, int out_fd)
{
	if (trust_fs == 1)
		return fs_btc20_trusted_copy(r, out_fd);

	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int status = 0;
	BTC20_CTX ctx;
	unsigned char sha_digest[32];
	unsigned char sha_sha_digest[32];
	unsigned char digest[20];
	unsigned char chunk[CHUNK_SIZE];
	int nread;
	static char n[] = "fs_btc20_copy";

	make_path(r, r->digest);

	/*
	 *  Open the file to the blob.
	 */
	if (_open(r, s->blob_path, &s->blob_fd) == ENOENT)
		return 1;

	if (!SHA256_Init(&ctx.sha256))
		_panic2(r, n, "SHA256_Init() failed");

	/*
	 *  Read a chunk from the file, write chunk to local stream,
	 *  update incremental digest.
	 */
	while ((nread = _read(r, s->blob_fd, chunk, sizeof chunk)) > 0) {
		if (io_write_buf(out_fd, chunk, nread)) {
			_error2(r, n, "write_buf() failed");
			goto croak;
		}
			
		/*
		 *  Update the incremental digest.
		 */
		if (!SHA256_Update(&ctx.sha256, chunk, nread))
			_panic2(r, n, "SHA256_Update(write) failed");
	}
	if (nread < 0)
		_panic2(r, n, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	if (!SHA256_Final(sha_digest, &ctx.sha256))
		_panic2(r, n, "SHA256_Final() failed");

	if (!SHA256_Init(&ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Init(sha256) failed");
	if (!SHA256_Update(&ctx.sha256_sha256, sha_digest, 32))
		_panic2(r, n, "SHA256_Update(sha256) failed");
	if (!SHA256_Final(sha_sha_digest, &ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Final(sha256) failed");

	/*
	 *  Calulate RIPEMD160(SHA256)
	 */
	if (!RIPEMD160_Init(&ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Init() failed");
	if (!RIPEMD160_Update(&ctx.ripemd160, sha_sha_digest, 32))
		_panic2(r, n, "RIPEMD160_Update(sha256) failed");
	if (!RIPEMD160_Final(digest, &ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Final(sha256) failed");

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 *
	 *  Why not zap the blob?
	 */
	if (memcmp(s->digest, digest, 20))
		_panic3(r, n, "stored blob doesn't match digest", r->digest);
	goto cleanup;
croak:
	status = -1;
cleanup:
	_close(r, &s->blob_fd);
	return status;
}

static int
fs_trust_btc20_eat(struct request *r)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	struct stat st;

	make_path(r, r->digest);
	if (_stat(r, s->blob_path, &st) == ENOENT)
		return 1;
	return 0;
}

static int
fs_btc20_eat(struct request *r)
{
	if (trust_fs == 1)
		return fs_trust_btc20_eat(r);

	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	int status = 0;
	BTC20_CTX ctx;
	unsigned char digest[20];
	unsigned char sha_digest[20];
	unsigned char sha_sha_digest[20];
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	make_path(r, r->digest);

	if (_open(r, s->blob_path, &s->blob_fd) == ENOENT)
		return 1;

	if (!SHA256_Init(&ctx.sha256))
		_panic(r, "SHA256_Init() failed");

	/*
	 *  Read a chunk from the file and chew.
	 */
	while ((nread = _read(r, s->blob_fd, chunk, sizeof chunk)) > 0)
		/*
		 *  Update the incremental digest.
		 */
		if (!SHA256_Update(&ctx.sha256, chunk, nread))
			_panic(r, "SHA256_Update(read) failed");
	if (nread < 0)
		_panic(r, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	if (!SHA256_Final(sha_digest, &ctx.sha256))
		_panic(r, "SHA256_Final() failed");

	if (!SHA256_Init(&ctx.sha256_sha256))
		_panic(r, "SHA256_Init(sha256) failed");
	if (!SHA256_Update(&ctx.sha256_sha256, sha_digest, 32))
		_panic(r, "SHA256_Update(sha256) failed");
	if (!SHA256_Final(sha_sha_digest, &ctx.sha256_sha256))
		_panic(r, "SHA256_Final(sha256) failed");

	/*
	 *  Calulate RIPEMD160(SHA256)
	 */
	if (!RIPEMD160_Init(&ctx.ripemd160))
		_panic(r, "RIPEMD160_Init() failed");
	if (!RIPEMD160_Update(&ctx.ripemd160, sha_sha_digest, 32))
		_panic(r, "RIPEMD160_Update(SHA256) failed");
	if (!RIPEMD160_Final(digest, &ctx.ripemd160))
		_panic(r, "RIPEMD160_Final(SHA256) failed");

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 *
	 *  Note: unfortunately we've already deceived the client
	 *        by sending "ok".  Probably need to improve for
	 *	  the special case when the entire blob is read
	 *        in first chunk.  is this still true?
	 */
	if (memcmp(s->digest, digest, 20))
		_panic2(r, "stored blob doesn't match digest", r->digest);
	_close(r, &s->blob_fd);
	return status;
}

/*
 *  Write a portion of a blob to local storage and derive a partial digest.
 *  Return 1 if the accumulated digest matches the expected digest,
 *  0 if the partial digest does not match do not match.
 */
static int
eat_chunk(struct request *r, SHA256_CTX *sha_ctx, int fd, unsigned char *buf,
	  int buf_size)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;

	BTC20_CTX ctx;
	unsigned char sha_digest[32];
	unsigned char sha_sha_digest[32];
	unsigned char digest[20];
	char n[] = "eat_chunk";

	/*
	 *  Update the incremental digest.
	 */
	if (!SHA256_Update(sha_ctx, buf, buf_size))
		_panic2(r, n, "SHA256_Update() failed");

	/*
	 *  Write the chunk to the local temp file.
	 */
	if (io_write_buf(fd, buf, buf_size))
		_panic3(r, n, "write(tmp) failed", strerror(errno));
	/*
	 *  Determine if we have seen the whole blob
	 *  by copying the incremental digest, finalizing it,
	 *  then comparing to the expected blob.
	 */
	ctx.sha256 = *sha_ctx;
	if (!SHA256_Final(sha_digest, &ctx.sha256))
		_panic2(r, n, "SHA256_Final() failed");

	/*
	 *  Take the sha256(sha25).
	 */
	if (!SHA256_Init(&ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Init(sha256) failed");
	if (!SHA256_Update(&ctx.sha256_sha256, sha_digest, 32))
		_panic2(r, n, "SHA256_Update(sha256) failed");
	if (!SHA256_Final(sha_sha_digest, &ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Final(sha256) failed");

	/*
	 *  Calulate RIPEMD160(SHA256) of incremental digest.
	 */
	if (!RIPEMD160_Init(&ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Init() failed");
	if (!RIPEMD160_Update(&ctx.ripemd160, sha_sha_digest, 32))
		_panic2(r, n, "RIPEMD160_Update(sha256) failed");
	if (!RIPEMD160_Final(digest, &ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Final(sha256) failed");

	return memcmp(s->digest, digest, 20) == 0 ? 1 : 0;
}

static int
fs_btc20_put_request(struct request *r)
{
	(void)r;
	return 0;	//  fs drivers require no pre setup for put
}

static int
fs_btc20_put_bytes(struct request *r)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	BTC20_CTX ctx;
	char tmp_path[MAX_FILE_PATH_LEN];
	unsigned char chunk[CHUNK_SIZE], *cp, *cp_end;
	int status = 0;
	char buf[MSG_SIZE*2];

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
	s->blob_fd = io_open(
			tmp_path,
			O_CREAT|O_EXCL|O_WRONLY|O_APPEND, S_IRUSR
	);
	if (s->blob_fd < 0) {
		snprintf(buf, sizeof buf, "open(%s) failed: %s", tmp_path,
							strerror(errno));
		_panic(r, buf);
	}

	/*
	 *  Initialize digest of blob being scanned from the client.
	 */
	if (!SHA256_Init(&ctx.sha256))
		_panic(r, "SHA256_Init() failed");

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
		if ((i64)r->scan_size != r->blob_size)
			_panic(r, "r->scan_size != r->blob_size");

		if (r->scan_size > (int)(sizeof chunk - 1)) {
			snprintf(buf, sizeof buf, "max=%lu", 
					(long unsigned)(sizeof chunk - 1));
			_panic2(r, "scanned chunk too big", buf);
		}

		/*
		 *  See if the entire blob fits in the first read.
		 */
		if (eat_chunk(
				r,
				&ctx.sha256,
				s->blob_fd,
				r->scan_buf,
				r->scan_size
		))
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
		if (eat_chunk(r, &ctx.sha256, s->blob_fd, cp, nread))
			goto digested;
		cp += nread;
	}
	cp = chunk;
	goto again;

digested:
	/*
	 *  Rename the temp blob file to the final blob path.
	 */
	make_path(r, r->digest);

	arbor_rename(tmp_path,
			((struct fs_btc20_request *)r->open_data)->blob_path);
	goto cleanup;
croak:
	status = -1;
cleanup:
	_close(r, &s->blob_fd);
	if (tmp_path[0] && _unlink(r, tmp_path, (int *)0))
		_panic(r, "_unlink() failed");
	return status; 
}

static int fs_btc20_take_request(struct request *r)
{
	return fs_btc20_get_request(r);
}

static int
fs_btc20_take_bytes(struct request *r)
{
	return fs_btc20_get_bytes(r);
}

/*
 *  Synopsis:
 *	Handle reply from client after client took blob.
 */
static int
fs_btc20_take_reply(struct request *r, char *reply)
{
	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;

	/*
	 *  If client replies 'ok', then delete the blob.
	 *  Eventually need to lock the file.
	 */
	if (*reply == 'o') {
		int exists = 1;
		char *slash;

		if (_unlink(r, s->blob_path, &exists))
			_panic(r, "_unlink() failed");
		if (!exists)
			_warn2(r, "expected blob file does not exist",
							s->blob_path);
		/*
		 *  Request trimming the empty directories.
		 */
		if ((slash = rindex(s->blob_path, '/'))) {
			*slash = 0;
			arbor_trim(s->blob_path);
			*slash = '/';
		}
		else
			_panic2(r, "slash missing from blob path",
							s->blob_path);
	}
	return 0;
}

static int
fs_btc20_give_request(struct request *r)	// no pre-proc for fs give
{
	(void)r;

	return 0;
}

/*
 *  Synopsis:
 *	Client is giving us a blob.
 */
static int
fs_btc20_give_bytes(struct request *r)
{
	return fs_btc20_put_bytes(r);
}

/*
 *  Synopsis:
 *	Handle client reply from fs_btc20_give_bytes().
 */
static int
fs_btc20_give_reply(struct request *r, char *reply)
{
	(void)r;
	(void)reply;

	return 0;
}

/*
 *  Digest a local blob stream and store the digested blob.
 */
static int
fs_btc20_digest(struct request *r, int fd, char *hex_digest)
{
	char unsigned buf[4096], digest[20], *d, *d_end;
	char *h;
	BTC20_CTX ctx;
	int nread;
	int tmp_fd = -1;
	char tmp_path[MAX_FILE_PATH_LEN];
	int status = 0;
	static int drift = 0;
	static char n[] = "digest";
	unsigned char sha_digest[32];
	unsigned char sha_sha_digest[32];

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
	tmp_fd = io_open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND, S_IRUSR);
	if (tmp_fd < 0) {
		_error3(r, n, "open(tmp) failed", tmp_path);
		_panic3(r, n, "open(tmp) failed", strerror(errno));
	}
	if (!SHA256_Init(&ctx.sha256))
		_panic2(r, n, "SHA256_Init() failed");
	while ((nread = io_read(fd, buf, sizeof buf)) > 0) {
		if (!SHA256_Update(&ctx.sha256, buf, nread))
			_panic2(r, n, "SHA256_Update(read) failed");
		if (io_write_buf(tmp_fd, buf, nread) != 0)
			_panic3(r, n, "write_buf(tmp) failed", strerror(errno));
	}
	if (nread < 0) {
		_error2(r, n, "read() failed");
		goto croak;
	}
	if (!SHA256_Final(sha_digest, &ctx.sha256))
		_panic2(r, n, "SHA256_Final() failed");

	status = io_close(tmp_fd);
	tmp_fd = -1;
	if (status)
		_panic3(r, n, "close(tmp) failed", strerror(errno));

	/*
	 *  Take the sha256(sha256)
	 */
	if (!SHA256_Init(&ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Init(sha256) failed");
	if (!SHA256_Update(&ctx.sha256_sha256, sha_digest, 32))
		_panic2(r, n, "SHA256_Update(sha256) failed");
	if (!SHA256_Final(sha_sha_digest, &ctx.sha256_sha256))
		_panic2(r, n, "SHA256_Final(sha256) failed");

	/*
	 *  Calulate RIPEMD160(sha_digest)
	 */
	if (!RIPEMD160_Init(&ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Init() failed");
	if (!RIPEMD160_Update(&ctx.ripemd160, sha_sha_digest, 32))
		_panic2(r, n, "RIPEMD160_Update(sha256) failed");
	if (!RIPEMD160_Final(digest, &ctx.ripemd160))
		_panic2(r, n, "RIPEMD160_Final() failed");

	/*
	 *  Convert the binary btc20=ripemd160(sha256(sha256(blob))) digest
	 *  to asci hex text.
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
	make_path(r, hex_digest);
	arbor_move(tmp_path,
		((struct fs_btc20_request *)r->open_data)->blob_path);
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
fs_btc20_close(struct request *r, int status)
{
	(void)r;
	(void)status;

	struct fs_btc20_request *s = (struct fs_btc20_request *)r->open_data;
	_close(r, &s->blob_fd);

	return 0;
}

static void
binfo(char *msg)
{
	info2("btc20: boot", msg);
}

static void
binfo2(char *msg1, char *msg2)
{
	info3("btc20: boot", msg1, msg2);
}

static int
fs_btc20_boot()
{
	binfo("starting");
	binfo("storage is file system");
	binfo2("openssl algorithm", OPENSSL_VERSION_TEXT);
	binfo("bitcoin ripemd160(sha256) hash");

	strcpy(boot_data.root_dir_path, fs_btc20_root);
	binfo2("fs root directory", boot_data.root_dir_path);

	/*
	 *  Verify or create root directory path.
	 */
	if (_mkdir((struct request *)0, boot_data.root_dir_path, 1))
		panic("btc20: boot: _mkdir(root_dir) failed");
	if (trust_fs == 1)
		binfo("trust filesystem is enabled");

	return 0;
}

/*
 *  Is the digest syntactically valid, empty, or invalid?
 *
 *  Returns:
 *	0	invalid sha digest.
 *	1	is valid and non empty
 *	2	is valid and empty
 */
static int
fs_btc20_is_digest(char *digest)
{
	char *d, *d_end, c;

	d = digest;
	d_end = d + 40;
	while (d < d_end) {
		c = *d++;
		if (!c || !isxdigit(c) || (isalpha(c) && !islower(c)))
			return 0;
	}

	//  40th digit is not null so not a valid digest
	if (*d_end)
		return 0;

	//  quick check of first three chars for empty
	if (digest[0] == 'f' && digest[1] == 'd' && digest[2] == '7')
		return strcmp(digest, empty_ascii) == 0 ? 2 : 1;
	return 1;
}

static int
fs_btc20_shutdown()
{
	info("btc20: shutdown: bye");
	return 0;
}

struct digest_module fs_btc20_module =
{
	.name		=	"btc20",

	.boot		=	fs_btc20_boot,
	.open		=	fs_btc20_open,

	.get_request	=	fs_btc20_get_request,
	.get_bytes	=	fs_btc20_get_bytes,

	.take_request	=	fs_btc20_take_request,
	.take_bytes	=	fs_btc20_take_bytes,
	.take_reply	=	fs_btc20_take_reply,
	.copy		=	fs_btc20_copy,

	.put_request	=	fs_btc20_put_request,
	.put_bytes	=	fs_btc20_put_bytes,

	.give_request	=	fs_btc20_give_request,
	.give_bytes	=	fs_btc20_give_bytes,
	.give_reply	=	fs_btc20_give_reply,

	.eat		=	fs_btc20_eat,

	.digest		=	fs_btc20_digest,
	.is_digest	=	fs_btc20_is_digest,

	.close		=	fs_btc20_close,
	.shutdown	=	fs_btc20_shutdown
};

#endif
