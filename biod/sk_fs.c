#ifdef SK_FS_MODULE
/*
 *  Synopsis:
 *	Module that manages sk digested blobs in POSIX style file system.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "skein.h"
#include "biod.h"

/*
 *  How many bytes to read and write the blobs from either client or fs.
 *  Note: might want to make the get chunk size be much bigger than the put.
 */
#define CHUNK_SIZE		(64 * 1024)

extern char		*getenv();
extern int		skein_nab2digest(char *ascii, unsigned char *digest);
extern void		skein_digest2nab(unsigned char *, char *);
extern int		skein_is_nab(char *);

static char	sk_fs_root[]	= "data/sk_fs";

struct sk_fs_request
{
	/*
	 *  Directory path to where blob file will be stored,
	 *  relative to BLOBIO_ROOT directory.
	 */
	char		blob_dir_path[MAX_FILE_PATH_LEN];
	char		blob_path[MAX_FILE_PATH_LEN];
	unsigned char	digest[32];
};

static struct sk_fs_boot
{
	char		root_dir_path[MAX_FILE_PATH_LEN];
	char		tmp_dir_path[MAX_FILE_PATH_LEN];
} boot_data;

static void
_panic(struct request *rp, char *msg)
{
	char buf[MSG_SIZE];

	if (rp)
		panic(log_strcpy3(buf, sizeof buf, rp->verb,rp->algorithm,msg));
	else
		panic2("sk", msg);
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

	if (rp)
		error(log_strcpy3(buf, sizeof buf, rp->verb,rp->algorithm,msg));
	else
		error2("sk", msg);
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

static int
sk_fs_open(struct request *rp)
{
	struct sk_fs_request *sp;

	sp = (struct sk_fs_request *)malloc(sizeof *sp);
	if (sp == NULL)
		_panic2(rp, "malloc(sk_fs_request) failed", strerror(errno));
	memset(sp, 0, sizeof *sp);

	if (strcmp("wrap", rp->verb))
		if (skein_nab2digest(rp->digest, sp->digest)) {
			_error2(rp, "syntax error in digest", rp->digest);
			return -1;
		}
	rp->open_data = (void *)sp;
	return 0;
}

static int
_unlink(struct request *rp, char *path, int *exists)
{
again:
	if (unlink(path)) {
		char buf[MSG_SIZE];
		int err = errno;

		if (err == EINTR) {
			snprintf(buf, sizeof buf, "unlink(%s) interupted",path);
			_warn2(rp, "_unlink", buf);
			goto again;
		}
		if (err == ENOENT) { 
			if (exists)
				*exists = 0;
			return 0;
		}
		snprintf(buf, sizeof buf, "unlink(%s) failed", path);
		_panic3(rp, "_unlink", buf, strerror(err));
	}
	if (exists)
		*exists = 1;
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
		nread = read(fd, b, b_end - b);
		if (nread < 0) {
			int err = errno;

			if (err == EINTR) {
				_warn(rp, "_read: read() interupted");
				continue;
			}
			_panic2(rp, "_read: read() failed", strerror(err));
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
again:
	if (close(fd)) {
		if (errno == EINTR) {
			_warn(rp, "_close: close() interupted");
			goto again;
		}
		_panic2(rp, "_close: close() failed", strerror(errno));
	}
	return 0;
}

/*
 *  Open a file to read, retrying on interupt and logging errors.
 */
static int
_open(struct request *rp, char *path, int *p_fd)
{
	int fd;

again:
	if ((fd = open(path, O_RDONLY, 0)) < 0) {
		int err = errno;
		char buf[MSG_SIZE];

		if (err == ENOENT)
			return ENOENT;
		if (err == EINTR) {
			snprintf(buf, sizeof buf, "open(%s) interupted", path);

			_warn2(rp, "_open", buf);
			goto again;
		}
		snprintf(buf, sizeof buf, "open(%s) failed", path);
		_panic3(rp, "_open", buf, strerror(err));
	}
	*p_fd = fd;
	return 0;
}

static int
_stat(struct request *rp, char *path, struct stat *p_st)
{
again:
	if (stat(path, p_st)) {
		char buf[MSG_SIZE];
		int err = errno;

		switch (err = errno) {
		case ENOENT:
			return ENOENT;
		case EINTR:
			snprintf(buf, sizeof buf, "stat(%s) interupted", path);
			_warn2(rp, "_stat", buf);
			goto again;
		default:
			snprintf(buf, sizeof buf, "stat(%s) failed", path);
			_panic3(rp, "_stat", buf, strerror(err));
		}
	}
	return 0;
}

static int
_mkdir(struct request *rp, char *path, int exists_ok)
{
again:
	if (io_mkdir(path, S_IRWXU | S_IXGRP)) {
		char buf[MSG_SIZE];
		int e = errno;

		if (e == EINTR || e == EAGAIN)
			goto again;
		if (e == EEXIST) {
			struct stat st;

			if (!exists_ok)
				return EEXIST;

			/*
			 *  Verify the path is a directory.
			 */
			if (_stat(rp, path, &st)) {
				_error(rp, "_mkdir: _stat() failed");
				return EEXIST;
			}
			return S_ISDIR(st.st_mode) ? 0 : ENOTDIR;
		}
		snprintf(buf, sizeof buf, "mkdir(%s) failed", path);
		_panic3(rp, "_mkdir", buf, strerror(e));
	}
	return 0;
}

/*
 *  Assemble the blob path, optionally creating the directory entries.
 *  Each new directory entry is twice the length of the previous entry.
 *
 *  The 'make_path' indicates if the full path should be present.
 */
static int
blob_path(struct request *rp, int make_path, char *digest)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	char *p, *q;

	/*
	 *  Derive a path to the blob from the digest.
	 *  Each directory component in the path doubles the
	 *  length of it's parent component.  For example, given
	 *  the digest
	 *
	 *	7WCGlyqKvCSqLw03HHqwZ6Cl0DGGfSK3zV0xij84Yk3
	 *
	 *  we derive the path to the blob file
	 *
	 *	7/WC/Glyq/KvCSqLw0/3HHqwZ6Cl0DGGfSK/3zV0xij84Yk3
	 *
	 *  where the final component, '3zV0xij84Yk3' is the file containing
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
	  *	Single character /[0-9a-zA-Z_-]
	  */
	*p++ = '/';
	*p++ = *q++;
	if (make_path) {
		*p = 0;
		if (_mkdir(rp, sp->blob_dir_path, 1)) {
			_error(rp, "blob_dir_path: _mkdir() failed");
			return -1;
		}
	}

	/*
	 *  Directory 2:
	 *	2 Character: /[0-9a-zA-Z_-][0-9a-zA-Z_-]
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;
	if (make_path) {
		*p = 0;
		if (_mkdir(rp, sp->blob_dir_path, 1)) {
			_error(rp, "blob_dir_path: _mkdir() failed");
			return -1;
		}
	}

	/*
	 *  Directory 3:
	 *	4 Characters:
	 *		[0-9a-zA-Z_-][0-9a-zA-Z_-][0-9a-zA-Z_-][0-9a-zA-Z_-]
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	if (make_path) {
		*p = 0;
		if (_mkdir(rp, sp->blob_dir_path, 1)) {
			_error(rp, "blob_dir_path: _mkdir() failed");
			return -1;
		}
	}

	/*
	 *  Directory 4:
	 *	8 Characters: /[0-9a-zA-Z_-]{8}
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	if (make_path) {
		*p = 0;
		if (_mkdir(rp, sp->blob_dir_path, 1)) {
			_error(rp, "blob_dir_path: _mkdir() failed");
			return -1;
		}
	}

	/*
	 *  Directory 5:
	 *	16 Characters: /[0-9a-zA-Z_-]{16}
	 */
	*p++ = '/';
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;	*p++ = *q++;
	*p = 0;
	if (make_path)
		if (_mkdir(rp, sp->blob_dir_path, 1)) {
			_error(rp, "blob_dir_path: _mkdir() failed");
			return -1;
		}
	/*
	 *  Build the path to the final resting place of the blob file.
	 */
	strcpy(sp->blob_path, sp->blob_dir_path);
	strcat(sp->blob_path, "/");
	strcat(sp->blob_path, digest + 31);

	return 0;
}

static int
sk_fs_get(struct request *rp)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	int status = 0;
	Skein_512_Ctxt_t ctx;
	unsigned char digest[32];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	if (blob_path(rp, 0, rp->digest)) {
		_panic(rp, "blob_path() failed");
	}
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
		_panic(rp, "_open() failed");
	}

	/*
	 *  Tell the client we have the blob.
	 *  In principle, we ought to read ahead a chunk from the file
	 *  before repsonding ok, on the premise that often a blob will
	 *  fit in the first read chunk.
	 */
	if (write_ok(rp)) {
		_error(rp, "write_ok() failed");
		goto croak;
	}

	if (Skein_512_Init(&ctx, 256) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Init(256) failed");

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
		if (Skein_512_Update(&ctx, chunk, nread) != SKEIN_SUCCESS)
			_panic(rp, "Skein_512_Update(eat) failed");
	}
	if (nread < 0)
		_panic(rp, "_read() failed");
	/*
	 *  Finalize the digest.
	 */
	if (Skein_512_Final(&ctx, digest) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Final() failed");
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
	if (memcmp(sp->digest, digest, 32))
		_panic2(rp, "stored blob doesn't match digest", rp->digest);
	goto bye;
croak:
	status = -1;
bye:
	if (_close(rp, &fd))
		_panic(rp, "_close() failed");
	return status;
}

static int
sk_fs_eat(struct request *rp)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	int status = 0;
	Skein_512_Ctxt_t ctx;
	unsigned char digest[32];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	if (blob_path(rp, 0, rp->digest))
		_panic(rp, "blob_path() failed");
	/*
	 *  Open the file to the blob.
	 */
	switch (_open(rp, sp->blob_path, &fd)) {
	case 0:
		break;
	case ENOENT:
		/*
		 *  Don't log the error.
		_warn2(rp, "not found", rp->digest);
		 */
		return 1;
	default:
		_panic(rp, "_open() failed");
	}

	if (Skein_512_Init(&ctx, 256) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Init(256) failed");

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 *
	 *  In principle, we ought to first scan the blob file
	 *  before sending "ok" to the requestor.
	 */
	while ((nread = _read(rp, fd, chunk, sizeof chunk)) > 0)
		/*
		 *  Update the incremental digest.
		 */
		if (Skein_512_Update(&ctx, chunk, nread) != SKEIN_SUCCESS)
			_panic(rp, "Skein_512_Update(eat) failed");
	if (nread < 0)
		_panic(rp, "_read() failed");
	/*
	 *  Finalize the digest.
	 */
	if (Skein_512_Final(&ctx, digest) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Final() failed");
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
	if (memcmp(sp->digest, digest, 32))
		_panic2(rp, "stored blob doesn't match digest", rp->digest);
	goto bye;
bye:
	if (_close(rp, &fd))
		_panic(rp, "_close() failed");
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
		nwrite = write(fd, b, b_end - b);
		if (nwrite < 0) {
			int err = errno;

			if (err == EINTR) {
				_warn(rp, "do_write: write() interupted");
				continue;
			}
			_panic2(rp, "do_write: write() failed", strerror(err));
		}
		if (nwrite == 0) {
			_panic(rp, "do_write: write() returned unexpected 0");
		}
		b += nwrite;
	}
	return 0;
}

/*
 *  Rename a file.
 */
static int
_rename(struct request *rp, char *old, char *new)
{
again:
	if (rename(old, new)) {
		char buf[MSG_SIZE];
		int e = errno;

		if (e == EINTR)
			goto again;
		snprintf(buf, sizeof buf, "rename(%s, %s) failed", old, new);
		_panic3(rp, "_rename", buf, strerror(e));
	}
	return 0;
}

/*
 *  Write a portion of a blob to local storage and derive a partial digest.
 *  Return 1 if the accumulated digest matches the expected digest,
 *  0 if the do not match, and -1 on error.
 */
static int
eat_chunk(struct request *rp, Skein_512_Ctxt_t *p_ctx, int fd,
	  unsigned char *buf, int buf_size)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	Skein_512_Ctxt_t ctx;
	unsigned char digest[32];

	/*
	 *  Update the incremental digest.
	 */
	if (Skein_512_Update(p_ctx, buf, buf_size) != SKEIN_SUCCESS)
		_panic(rp, "eat_chunk: Skein_512_Update() failed");
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
	if (Skein_512_Final(&ctx, digest) != SKEIN_SUCCESS)
		_panic(rp, "eat_chunk: Skein_512_Final() failed");
	return memcmp(sp->digest, digest, 32) == 0 ? 1 : 0;
}
/*
 *  Move a temporary blob into blob path.
 *
 *  This is ugly and needs rumination.
 *
 *  ENOENT normally means one of the directory components
 *  disappeared since the blob_path() call above.
 *  Gargabge collection would be the cuprit.
 *  So, we recreate the full directory path using
 *  blob_path() one more time.  If that failed, then punt.
 */
static int
rename_blob(struct request *rp, char *tmp_path, char *digest)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	int status, trys = 3;

	/*
	 *  Move the temp blob file to the final blob path.
	 *
	 *  First ensure the blob path exists.
	 */
	if (blob_path(rp, 1, digest))
		_panic(rp, "rename-blob: blob_path(1) failed");
again:
	status = _rename(rp, tmp_path, sp->blob_path);
	if (status == 0) {
		tmp_path[0] = 0;
		return 0;
	}
	if (--trys <= 0)
		_panic(rp, "rename_blob: rename(retry) failed");
	/*
	 *  This is ugly and needs rumination.
	 *
	 *  ENOENT normally means one of the directory components
	 *  disappeared since the blob_path() call above.
	 *  Gargabge collection would be the cuprit.
	 *  So, we recreate the full directory path using
	 *  blob_path() one more time.  If that failed, then punt.
	 */
	if (status == ENOENT) {
		_warn(rp, "rename_blob: _rename() returned ENOENT");
		_warn(rp, "calling blob_path() again to remake dir path");
		if (blob_path(rp, 1, digest) == 0)
			goto again;
		_panic(rp, "rename_blob: retry call to blob_path() failed");
	}
	_panic(rp, "rename_blob: _rename() failed");
	return -1;					/* not reached */
}

static int
sk_fs_put(struct request *rp)
{
	Skein_512_Ctxt_t ctx;
	char tmp_path[MAX_FILE_PATH_LEN];
	unsigned char chunk[CHUNK_SIZE], *cp, *cp_end;
	int fd;
	int status = 0;
	char buf[MSG_SIZE];

	/*
	 *  Open a temporary file in $sk_fs_root/tmp to accumulate the
	 *  blob read from the client.  The file looks like
	 *
	 *	time-pid-digest.
	 */
	snprintf(tmp_path, sizeof tmp_path, "%s/%u-%u-%s",
					boot_data.tmp_dir_path,
					/*
					 *  Error:
					 *	Casting time as int is wrong!
					 */
					(unsigned int)time((time_t *)0),
					getpid(),
					rp->digest);
	/*
	 *  Open the file ... need O_LARGEFILE support!!
	 */
	fd = open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND, S_IRUSR);
	if (fd < 0) {
		int err = errno;

		snprintf(buf, sizeof buf, "open(%s) failed", tmp_path);
		_panic2(rp, buf, strerror(err));
	}

	/*
	 *  Initialize digest of blob being scanned from the client.
	 */
	if (Skein_512_Init(&ctx, 256) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Init() failed");
	/*
	 *  An empty blob is always put.
	 *  Note: the caller has already ensure that no more data has
	 *  been written by the client, so no need to check rp->scan_size.
	 */
	if (skein_is_nab(rp->digest) == 2)
		goto blob_sighted;

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
			goto blob_sighted;
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
			_error(rp, "empty read before signed blob seen");
			goto croak;
		}
		switch (eat_chunk(rp, &ctx, fd, cp, nread)) {
		case -1:
			_panic(rp, "eat_chunk() failed");
		case 1:
			goto blob_sighted;
		}
		cp += nread;
	}
	cp = chunk;
	goto again;

blob_sighted:
	if (_close(rp, &fd))
		_panic(rp, "_close(blob) failed");
	if (rename_blob(rp, tmp_path, rp->digest))
		_panic(rp, "rename_blob() failed");
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
sk_fs_take_blob(struct request *rp)
{
	return sk_fs_get(rp);
}

static int
_rmdir(char *path)
{
again:
	if (rmdir(path)) {
		int e = errno;

		if (e == EINTR || e == EAGAIN)
			goto again;
		return e;
	}
	return 0;
}

/*
 *  Synopsis:
 *	Handle reply from client after client took blob.
 */
static int
sk_fs_take_reply(struct request *rp, char *reply)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;

	/*
	 *  If client replies 'ok', then delete the blob.
	 *  Eventually need to lock the file.
	 */
	if (*reply == 'o') {
		int status, exists;
		char *p;

		if (_unlink(rp, sp->blob_path, &exists))
			_panic(rp, "_unlink() failed");
		if (!exists)
			_warn2(rp, "expected blob file does not exist",
							sp->blob_path);
		/*
		 *  Unlink the final directory component.
		 *  Eventually need to reap all empty directories.
		 */
		status = _rmdir(sp->blob_dir_path);
		if (status > 0 && status != ENOTEMPTY)
			_panic(rp, "_rmdir(final dir component) failed");

		/*
		 *  Unlink the second to the last directory component.
		 */
		p = rindex(sp->blob_dir_path, '/');
		if (p) {
			*p = 0;
			status = _rmdir(sp->blob_dir_path);
			if (status > 0 && status != ENOTEMPTY)
				_panic(rp, "_rmdir(-2) failed");
		}
	}
	return 0;
}

/*
 *  Synopsis:
 *	Client is giving us a blob.
 */
static int
sk_fs_give_blob(struct request *rp)
{
	return sk_fs_put(rp);
}

/*
 *  Synopsis:
 *	Handle client reply from give_blob().
 */
static int
sk_fs_give_reply(struct request *rp, char *reply)
{
	UNUSED_ARG(rp);
	UNUSED_ARG(reply);

	return 0;
}

static int
sk_fs_close(struct request *rp, int status)
{
	UNUSED_ARG(rp);
	UNUSED_ARG(status);
	return 0;
}

static void
binfo(char *msg)
{
	info2("sk: boot", msg);
}

static void
binfo2(char *msg1, char *msg2)
{
	info3("sk: boot", msg1, msg2);
}

static int
sk_fs_boot()
{
	char buf[MSG_SIZE];

	binfo("starting");
	binfo("storage is file system");

	strcpy(boot_data.root_dir_path, sk_fs_root);
	binfo2("fs root directory", boot_data.root_dir_path);

	snprintf(buf, sizeof buf, "SKEIN_SCHEMA_VER=0x%llx", 
				(long long unsigned int)SKEIN_SCHEMA_VER);
	binfo(buf);

	/*
	 *  Verify or create root directory path.
	 */
	if (_mkdir((struct request *)0, boot_data.root_dir_path, 1))
		panic("sk: boot: _mkdir(root_dir) failed");

	/*
	 *  Create the temporary scratch directory data/sk_fs/tmp
	 */
	strcpy(boot_data.tmp_dir_path, boot_data.root_dir_path);
	strcat(boot_data.tmp_dir_path, "/tmp");

	if (_mkdir((struct request *)0, boot_data.tmp_dir_path, 1))
		panic("sk: boot: _mkdir(tmp) failed");
	binfo2("sk temporary directory", boot_data.tmp_dir_path);

	return 0;
}

static int
sk_fs_shutdown()
{
	info("sk: shutdown: bye");
	return 0;
}


static int
sk_fs_digest(struct request *rp, int fd, char *nab_digest, int do_put)
{
	char unsigned buf[4096], digest[32];
	Skein_512_Ctxt_t ctx;
	int nread;
	int tmp_fd = -1;
	char tmp_path[MAX_FILE_PATH_LEN];
	int status = 0;
	static int drift = 0;

	tmp_path[0] = 0;
	if (do_put) {
		if (drift++ >= 999)
			drift = 0;

		/*
		 *  Open a temporary file in $sk_fs_root/tmp to accumulate the
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
		tmp_fd = open(tmp_path, O_CREAT|O_EXCL|O_WRONLY|O_APPEND,
								S_IRUSR);
		if (tmp_fd < 0)
			_panic3(rp, "digest: open(tmp) failed", tmp_path,
							strerror(errno));
	}
	if (Skein_512_Init(&ctx, 256) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Init(256) failed");
	while ((nread = read_buf(fd, buf, sizeof buf, rp->read_timeout)) > 0) {
		/*
		 *  Update the incremental digest.
		 */
		if (Skein_512_Update(&ctx, buf, nread) != SKEIN_SUCCESS)
			_panic(rp, "Skein_512_Update(chunk) failed");
		if (do_put && write_buf(tmp_fd, buf, nread, rp->write_timeout))
			_panic(rp, "digest: write_buf(tmp) failed");
	}
	if (nread < 0) {
		_error(rp, "digest: _read() failed");
		goto croak;
	}
	if (Skein_512_Final(&ctx, digest) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Final() failed");
	if (do_put) {
		status = close(tmp_fd);
		tmp_fd = -1;
		if (status)
			_panic2(rp,"digest: close(tmp) failed",strerror(errno));
	}
	skein_digest2nab(digest, nab_digest);

	if (do_put) {
		/*
		 *  Move the blob from the temporary file to the blob file.
		 */
		if (rename_blob(rp, tmp_path, nab_digest))
			_panic(rp, "digest: rename_blob() failed");
	}

	goto cleanup;
croak:
	status = -1;
cleanup:
	if (tmp_fd > -1 && close(tmp_fd))
		_panic2(rp, "digest: close(tmp) failed", strerror(errno));
	if (tmp_path[0] && unlink(tmp_path))
		_panic2(rp, "digest: unlink(tmp) failed", tmp_path);
	return status;
	return 0;
}

static int
sk_fs_is_digest(char *digest)
{
	static char empty[] = "7W3GlyqKvCUqLw03HHrwZ63l0DGGfSH3zV0xij24Yk3";

	if (digest[0] == '7' && digest[1] == 'W')
		return strcmp(digest, empty) == 0 ? 2 : skein_is_nab(digest)==1;
	return skein_is_nab(digest) == 1;
}

/*
 *  Write a blob to a stream.
 *  Return 0 if stream matches signature, -1 otherwise.
 *  Note: this needs to be folded into sk_fs_get().
 */
static int
sk_fs_write(struct request *rp, int out_fd)
{
	struct sk_fs_request *sp = (struct sk_fs_request *)rp->open_data;
	int status = 0;
	Skein_512_Ctxt_t ctx;
	unsigned char digest[32];
	int fd;
	unsigned char chunk[CHUNK_SIZE];
	int nread;

	if (blob_path(rp, 0, rp->digest))
		_panic(rp, "blob_path() failed");
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

	if (Skein_512_Init(&ctx, 256) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Init(256) failed");

	/*
	 *  Read a chunk from the file, write chunk to client,
	 *  update incremental digest.
	 */
	while ((nread = _read(rp, fd, chunk, sizeof chunk)) > 0) {
		if (write_buf(out_fd, chunk, nread, rp->write_timeout)) {
			_error(rp, "write_buf() failed");
			goto croak;
		}
		/*
		 *  Update the incremental digest.
		 */
		if (Skein_512_Update(&ctx, chunk, nread) != SKEIN_SUCCESS)
			_panic(rp, "Skein_512_Update(eat) failed");
	}
	if (nread < 0)
		_panic(rp, "_read(blob) failed");
	/*
	 *  Finalize the digest.
	 */
	if (Skein_512_Final(&ctx, digest) != SKEIN_SUCCESS)
		_panic(rp, "Skein_512_Final() failed");

	/*
	 *  If the calculated digest does NOT match the stored digest,
	 *  then zap the blob from storage and get panicy.
	 *  A corrupt blob is a bad, bad thang.
	 */
	if (memcmp(sp->digest, digest, 20))
		_panic2(rp, "stored blob doesn't match digest", rp->digest);
	goto cleanup;
croak:
	status = -1;
cleanup:
	if (_close(rp, &fd))
		_panic(rp, "_close(blob) failed");
	return status;
}

struct digest_module sk_fs_module =
{
	.name		=	"sk",

	.boot		=	sk_fs_boot,
	.open		=	sk_fs_open,

	.get		=	sk_fs_get,
	.take_blob	=	sk_fs_take_blob,
	.take_reply	=	sk_fs_take_reply,
	.write		=	sk_fs_write,

	.put		=	sk_fs_put,
	.give_blob	=	sk_fs_give_blob,
	.give_reply	=	sk_fs_give_reply,

	.eat		=	sk_fs_eat,
	.digest		=	sk_fs_digest,
	.is_digest	=	sk_fs_is_digest,

	.close		=	sk_fs_close,
	.shutdown	=	sk_fs_shutdown
};

#endif
