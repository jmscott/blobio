/*
 *  Synopsis:
 *	Driver for a fast, trusted posix file system blobio service.
 *  Note:
 *	Not UTF-8 safe.  Various char counts are in ascii bytes, not UTF8 chars.
 */

#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "blobio.h"

#define DEFER {if (!err && errno > 0) err = strerror(errno); goto BYE;}

extern struct service fs_service;

static char fs_data[4 + 1 + 3 + 8 + 1];			// data/fs_<algo>
static int end_point_fd = -1;
static char *chat_history = (char *)0;

/*
 *  Verify the syntax of the end point of the file system service.
 *  The endpoint is the root directory of the source blobio file system.
 *  The query args (following '?') have already been removed.
 *  The end point is assumed be comprised of ascii chars.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
fs_end_point_syntax(char *root_dir)
{
	if (strlen(root_dir) > BLOBIO_MAX_FS_PATH - 1)
		return "too many chars in root directory path";
	return (char *)0;
}

//  open a request to posix file system.
//  assume global var "algo" has been extracted by caller.

static char *
fs_open()
{
	blob_size = 0;
	char *end_point = fs_service.end_point;

	TRACE2("end point directory", end_point);

	//  verify endpoint is full path.

	//  Note: why care about full path end point?
	if (*end_point != '/')
		return "first char of end point is not '/'";

	end_point_fd = jmscott_open(end_point, O_DIRECTORY, 0777);
        if (end_point_fd < 0)
                return strerror(errno);

	// assemble "fs_data" using global algorithm

	char *err;
	fs_data[0] = 0;
	jmscott_strcat2(fs_data, sizeof fs_data, "data/fs_", algorithm);
	TRACE2("mkdir: fs data", fs_data);
	err = jmscott_mkdirat_path(end_point_fd, fs_data, 0777);
	if (err)
		return err;

	char v = *verb;

	if ((v == 'g' && verb[1] == 'e') || v == 't' || v == 'e') {
		TRACE2("read only verb", verb);
		return (char *)0;
	}

	char *p;
	if (v == 'w')
		p = "spool/wrap";
	else if (v == 'r')
		p = "spool/roll";
	else
		p = "spool";		//  "put" or "give"
	TRACE2("mkdir", p);

	return jmscott_mkdirat_path(end_point_fd, p, 0777);
}

static char *
fs_close()
{
	TRACE("entered");
	if (end_point_fd > -1 && jmscott_close(end_point_fd))
		return strerror(errno);
	return (char *)0;
}

static char *
fs_hard_link(int *ok_no, char *blob_path)
{
	TRACE("entered");

	int status = jmscott_linkat(
			end_point_fd,
			blob_path,
			AT_FDCWD,
			output_path,
			0
	);
	if (status != 0) {
		if (errno == ENOENT) {
			TRACE("source blob does not exist");
			*ok_no = 1;
			chat_history = "no";
			return (char *)0;
		}
		if (errno == EXDEV) {
			TRACE("source blob on different device");
			return "";
		}
		return strerror(errno);
	}

	TRACE("hard linked blob ok to output path");

	off_t sz;
	char *err = jmscott_fsizeat(end_point_fd, blob_path, &sz);
	if (err)
		return err;
	blob_size = (long long)sz;
	TRACE_LL("blob size", blob_size);

	*ok_no = 0;
	chat_history = "ok";

	return (char *)0;
}

static char *
fs_get(int *ok_no)
{
	TRACE("entered");

	char blob_path[BLOBIO_MAX_FS_PATH+1];
	blob_path[0] = 0;
	char *p = jmscott_strcat2(blob_path, sizeof blob_path, fs_data, "/");

	size_t len = sizeof blob_path - (p - blob_path);
	char *err = digest_module->fs_path(p, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	err = fs_hard_link(ok_no, blob_path);
	if (!err)
		return (char *)0;
	if (err && err[0]) {
		TRACE2("hard link failed", err);
		return err;
	}

	TRACE("hard link failed, so try copy with sendfile");

	//  trusted copy of blob to output fd
	int blob_fd = jmscott_openat(end_point_fd, blob_path, O_RDONLY, 0777);
	if (blob_fd < 0) {
		if (errno == ENOENT) {		//  hmm, blob disappeared (ok)
			*ok_no = 1;
			chat_history = "no";
			return (char *)0;
		}
		return strerror(errno);
	}
	chat_history = "ok";
	*ok_no = 0;

	if (output_path == null_device) {
		TRACE("output path is null device, so no read of blob");
		goto BYE;
	}

	if (output_path) {
		TRACE2("open output path exists", output_path)
		output_fd = jmscott_open(
				output_path,
				O_WRONLY | O_CREAT | O_EXCL,
				S_IRUSR | S_IRGRP
		);
		if (output_fd < 0)
			DEFER;
	}

	TRACE("sendfile ...");
	blob_size = 0;
	err = jmscott_send_file(blob_fd, output_fd, &blob_size);
BYE:
	if (blob_fd >= 0 && jmscott_close(blob_fd) && !err)
		err = strerror(errno);
	blob_fd = -1;

	if (output_path && output_fd >= 0 && jmscott_close(output_fd) && !err)
		err = strerror(errno);
	output_fd = -1;

	if (err)
		return err;
	return (char *)0;
}

static char *
fs_take(int *ok_no)
{
	TRACE("entered");

	char *err = fs_get(ok_no);
	if (err)
		return err;
	if (*ok_no == 1) {
		chat_history = "no";
		TRACE("blob does not exist");
		return (char *)0;
	}

	/*
	 *  Construct path to blob in data/fs_<algo>/...
	 */
	char blob_path[BLOBIO_MAX_FS_PATH+1];
	blob_path[0] = 0;
	char *bp = jmscott_strcat3(blob_path, sizeof blob_path,
		"data/fs_",
		algorithm,
		"/"
	);
	int len = sizeof blob_path - (bp - blob_path);
	err = digest_module->fs_path(bp, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	if (jmscott_unlinkat(end_point_fd, blob_path, 0) && errno != ENOENT)
		return strerror(errno);
	chat_history = "ok,ok,ok";
	return (char *)0;
}

/*
 *  Note:
 *	Remove temp file on func return!
 */
static char *
fs_put(int *ok_no)
{
	TRACE("entered");

	/*
	 *  Construct path to blob in data/fs_<algo>/...
	 */

	char blob_path[BLOBIO_MAX_FS_PATH+1];
	blob_path[0] = 0;
	char *bp = jmscott_strcat3(blob_path, sizeof blob_path,
		"data/fs_",
		algorithm,
		"/"
	);
	int len = sizeof blob_path - (bp - blob_path);
	char *err = digest_module->fs_path(bp, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	/*
	 *  Attempt to hard link the input file directly to blob under
	 *  $BLOBIO_ROOT/data/fs_<algo>/path/to/blob.
	 */

	if (input_path) {
		TRACE2("attempt hard link: input path", input_path);
		int status = jmscott_linkat(
					AT_FDCWD,
					input_path,
					end_point_fd,
					blob_path,
					0
		);
		if (status == 0) {
			TRACE("hard linked, so no copy");
			chat_history = "ok,ok";
			*ok_no = 0;
			return (char *)0;
		}
		if (errno == EEXIST) {
			TRACE("blob already exists, so no copy");
			chat_history = "ok,ok";
			*ok_no = 0;
			return (char *)0;
		}
		if (errno != EXDEV)
			return strerror(errno);
	}

	TRACE("can not hard link across device, so copy");

	/*
	 *  First copy blob into $BLOBIO_ROOT/tmp/<udig>-<pid>.put
	 */
	err = jmscott_mkdirat_path(end_point_fd, "tmp", 0777);
	if (err)
		return err;

	//  copy blob to tmp/put-<udig>.<pid> before renaming to data/
	//  assume data/ and tmp/ on same volume, which may not be true!

	char tmp_path[BLOBIO_MAX_FS_PATH+1];
	tmp_path[0] = 0;

	char pid[21];
	pid[0] = '-';
	*jmscott_lltoa((long long)getpid(), pid + 1) = 0;
	jmscott_strcat6(tmp_path, sizeof tmp_path,
				"tmp/put-",
				algorithm,
				":",
				ascii_digest,
				pid,
				".blob"
	);
	TRACE2("tmp path", tmp_path);
	int tmp_fd = jmscott_openat(
			end_point_fd,
			tmp_path,
			O_WRONLY | O_EXCL | O_CREAT,
			0777
	);
	if (tmp_fd < 0)
		DEFER; 
	blob_size = 0;

	TRACE("sendfile input->tmp");
	err = jmscott_send_file(input_fd, tmp_fd, &blob_size);
	if (err)
		DEFER;
	if (jmscott_close(tmp_fd))
		DEFER;
	tmp_fd = -1;
	TRACE("rename tmp->data");
	if (jmscott_renameat(end_point_fd, tmp_path, end_point_fd, blob_path))
		DEFER;
	TRACE("ok");
BYE:
	if (tmp_fd >= 0) {
		if (jmscott_close(tmp_fd) && !err)
			err = strerror(errno);
		tmp_fd = -1;
		if (jmscott_unlinkat(end_point_fd, tmp_path, 0) && !err)
			err = strerror(errno);
	}
	if (!err) {
		*ok_no = 0;
		chat_history = "ok,ok";
	}
	return err;
}

static char *
fs_give(int *ok_no)
{
	TRACE("entered");

	char *err = fs_put(ok_no);
	if (err)
		return err;
	if (*ok_no == 1) {
		chat_history = "ok,no";
		TRACE("could not put blob");
		return (char *)0;
	}

	//  Note: ignoring ENOENT needs a think.
	if (input_path && jmscott_unlink(input_path) && errno != ENOENT)
		return strerror(errno);
	chat_history = "ok,ok,ok";
	return (char *)0;
}

//  "eat" of a blob in a trusted file system.

char *
fs_eat(int *ok_no)
{
	TRACE("entered");

	char blob_path[BLOBIO_MAX_FS_PATH+1];
	blob_path[0] = 0;
	char *bp = jmscott_strcat3(blob_path, sizeof blob_path,
		"data/fs_",
		algorithm,
		"/"
	);
	int len = sizeof blob_path - (bp - blob_path);
	char *err = digest_module->fs_path(bp, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	if (jmscott_faccessat(end_point_fd, blob_path, R_OK, 0)) {
		if (errno == ENOENT) {
			TRACE("blob does not exist");
			chat_history = "no";
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}
	TRACE("blob exists");
	chat_history = "ok";
	*ok_no = 0;
	return (char *)0;
}

/*
 *  Synopsis:
 *	Wrap the contents of spool/wrap after moving spool/fs.brr to spool/wrap.
 *  Description:
 *	Wrap occurs in two steps.
 *
 *	First, move a frozen version of spool/bio4d.brr file into the file
 *	spool/wrap/<udig>.brr, where <udig> is the digest of the frozen
 *	blobified brr file.
 *
 *		mv spool/bio4d.brr ->
 *		    spool/wrap/sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f.brr
 *
 *	where, say, sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f is the digest
 *	of the frozen, read-only brr log file.
 *
 *	Second, scan the spool/wrap directory to build a set of all the udigs
 *	of the frozen blobs in spool/wrap and then take the digest of THAT set
 *	of udigs.
 *
 *	For example, suppose we have three traffic logs ready for digestion.
 *
 *		spool/wrap/sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f.brr
 *		spool/wrap/sha:59a066dc8b2a633414c58a0ec435d2917bbf4b85.brr
 *		spool/wrap/sha:f768865fe672543ebfac08220925054db3aa9465.brr
 *
 *	To wrap we build a small blob consisting only of the set of the above
 *	udigs
 *
 *		sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f\n
 *		sha:59a066dc8b2a633414c58a0ec435d2917bbf4b85\n
 *		sha:f768865fe672543ebfac08220925054db3aa9465\n
 *
 * 	and then digest THAT set into a blob.  The udig of THAT set is returned
 * 	to the client and the set is moved to spool/roll/<udig>.uset.
 */
static char *
fs_wrap(int *ok_no)
{
	TRACE("entered");

	/*
	 *  Freeze spool/fs.brr by moving
	 *
	 *	spool/fs.brr -> spool/WRAPPING-fs-<epoch>-<pid>.brr
	 */
	char brr_path[BLOBIO_MAX_FS_PATH+1];
	brr_path[0] = 0;
	jmscott_strcat(brr_path, sizeof brr_path, "spool/fs.brr");

	char pid[21];
	jmscott_ulltoa((unsigned long long)getpid(), pid);

	char epoch[21];
	jmscott_ulltoa((unsigned long long)time((time_t *)0), epoch);

	char frozen_path[BLOBIO_MAX_FS_PATH+1];
	frozen_path[0] = 0;
	jmscott_strcat5(frozen_path, sizeof frozen_path,
			"spool/FROZEN-fs-",
			epoch,
			"-",
			pid,
			".pid"
	);
	TRACE2("frozen path", frozen_path);

	*ok_no = 0;
	return (char *)0;
}

/*
 *  Set file system transport as
 *
 *	fs~<endpoint/path/to/spool/fs.brr>.<pid>
 *
 *  and chat history.
 *
 *  and open fs.brr.
 */
char *
fs_brr_frisk(struct brr *brr)
{
	TRACE("entered");

	brr->chat_history[0] = 0;
	jmscott_strcat(
		brr->chat_history,
		sizeof brr->chat_history,
		chat_history
	);
	TRACE2("chat history", brr->chat_history);

	char pid[20];
	jmscott_lltoa((long long)getpid(), pid);
	brr->transport[0] = 0;
	jmscott_strcat4(brr->transport, sizeof brr->transport,
			"fs~",
			fs_service.end_point,
			"#",
			pid
	);
	TRACE2("transport", brr->transport);

	if (jmscott_mkdirat_EEXIST(end_point_fd, "spool", 0777))
		return strerror(errno);

	brr->log_fd = jmscott_openat(
			end_point_fd,
			"spool/fs.brr",
			O_WRONLY | O_CREAT | O_APPEND,
			0777
	);
	if (brr->log_fd < 0)
		return strerror(errno);
	return (char *)0;
}

struct service fs_service =
{
	.name			=	"fs",
	.end_point_syntax	=	fs_end_point_syntax,
	.open			=	fs_open,
	.close			=	fs_close,
	.get			=	fs_get,
	.put			=	fs_put,
	.eat			=	fs_eat,
	.take			=	fs_take,
	.give			=	fs_give,
	.wrap			=	fs_wrap,
	.brr_frisk		=	fs_brr_frisk
};
