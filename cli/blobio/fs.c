/*
 *  Synopsis:
 *	Driver for a fast, trusted posix file system blobio service.
 *  Note:
 *	Review chat history construction.
 *
 *	Double check default (0777) dir perms in various mkdir calls.
 *
 *	Under linux perhaps consult /proc file system to verify that no process
 *	is writing to the frozen wrap brr log after freezing.  clever.
 *
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

static char *
fs_open()
{
	if (verb[0] == 'w') {
		if (!algo[0])
			return "wrap requires algo query arg in service uri";
		digest_module = find_digest(algo);
		if (!digest_module)
			return "unknown digest module in query arg \"algo\"";
		algorithm[0] = 0;
		jmscott_strcat(algorithm, sizeof algorithm, algo);
	}

	if (input_path && strcmp(input_path, null_device) == 0)
		return "/dev/null can not be input device for fs service";

	char *end_point = fs_service.end_point;

	TRACE2("end point directory", end_point);

	//  verify endpoint is full path.

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
fs_hard_link(int *ok_no, int src_fd, char *src_path, int tgt_fd, char *tgt_path)
{
	TRACE("entered");

	TRACE2("src path", src_path);
	TRACE2("tgt path", tgt_path);

	int status = jmscott_linkat(
			src_fd,
			src_path,
			tgt_fd,
			tgt_path,
			0
	);
	if (status)
		switch (errno) {
		case ENOENT:
			TRACE("src blob does not exist");
			*ok_no = 1;
			return (char *)0;
		case EEXIST:
			TRACE("tgt blob exists (ok)");
			break;
		case EXDEV:
			TRACE("src blob on different device");
			return "";
		default:
			return strerror(errno);
		}

	TRACE("hard linked src->tgt");

	TRACE("set blob size to tgt size");
	off_t sz;
	char *err = jmscott_fsizeat(tgt_fd, tgt_path, &sz);
	if (err)
		return err;
	blob_size = (long long)sz;
	TRACE_LL("blob size", blob_size);

	*ok_no = 0;
	return (char *)0;
}

static char *
fs_blob_path(char *blob_path, int path_size)
{
	blob_path[0] = 0;
	char *p = jmscott_strcat2(blob_path, path_size, fs_data, "/");
	size_t len = path_size - (p - blob_path);

	char *err = digest_module->fs_path(p, len);
	if (err)
		return err;
	TRACE2("blob path", blob_path);
	return (char *)0;
}

static void
fs_add_chat(char *chat)
{
	TRACE(chat);
	if (chat_history[0])
		jmscott_strcat2(chat_history, sizeof chat_history, ",", chat);
	else
		jmscott_strcat(chat_history, sizeof chat_history, chat);
}

/*
 *  Trusted "get" of a blob.
 */
static char *
fs_get(int *ok_no)
{
	TRACE("entered");

	char blob_path[BLOBIO_MAX_FS_PATH+1];

	char *err = fs_blob_path(blob_path, sizeof blob_path);
	if (err)
		return err;

	if (output_path == null_device) {
		TRACE("output /dev/null, so no write|link");

		off_t sz;
		err = jmscott_fsizeat(end_point_fd, blob_path, &sz);
		if (err) {
			if (errno != ENOENT)
				return err;
			fs_add_chat("no");
			*ok_no = 1;
			return (char *)0;
		}
		blob_size = sz;
		*ok_no = 0;
		fs_add_chat("no");
		return (char *)0;
	}
	if (output_path) {
		err = fs_hard_link(
				ok_no,
				end_point_fd,
				blob_path,
				AT_FDCWD,
				output_path
		);
		if (err) {
			if (!err[0])
				return err;
		} else {
			fs_add_chat("ok");
			return (char *)0;
		}
	}

	TRACE("xdev failed, so copy blob->output");

	int blob_fd = jmscott_openat(end_point_fd, blob_path, O_RDONLY, 0777);
	if (blob_fd < 0) {
		if (errno == ENOENT) {
			TRACE("hmm, blob disappeared");
			*ok_no = 1;
			fs_add_chat("no");
			return (char *)0;
		}
		return strerror(errno);
	}
	fs_add_chat("ok");
	*ok_no = 0;

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
	if (err)
		TRACE2("send_file failed", err);
	TRACE_LL("send blob size", blob_size);
BYE:
	if (blob_fd >= 0 && jmscott_close(blob_fd) && !err)
		err = strerror(errno);
	blob_fd = -1;

	if (output_path && output_fd >= 0 && jmscott_close(output_fd) && !err)
		err = strerror(errno);
	output_fd = -1;
	return err;
}

static char *
fs_take(int *ok_no)
{
	TRACE("entered");

	char *err = fs_get(ok_no);
	if (err)
		return err;
	if (*ok_no == 1) {
		fs_add_chat("no");
		TRACE("blob does not exist");
		return (char *)0;
	} else
		fs_add_chat("ok");

	/*
	 *  Construct path to blob in data/fs_<algo>/...
	 */
	char blob_path[BLOBIO_MAX_FS_PATH+1];
	err = fs_blob_path(blob_path, sizeof blob_path);
	if (err)
		return err;

	if (jmscott_unlinkat(end_point_fd, blob_path, 0)) {
		if (errno != ENOENT)
			return strerror(errno);
		fs_add_chat("ok");
	}
	fs_add_chat("ok,ok,ok");
	*ok_no = 0;
	return (char *)0;
}

/*
 *  "put" of a trusted file to a blob store.
 */
static char *
fs_put(int *ok_no)
{
	TRACE("entered");

	char blob_path[BLOBIO_MAX_FS_PATH+1];
	char *err = fs_blob_path(blob_path, sizeof blob_path);
	if (err)
		return err;
	TRACE2("blob path", blob_path);

	char blob_dir_path[BLOBIO_MAX_FS_PATH+1];
	blob_dir_path[0] = 0;
	char *p = jmscott_strcat2(blob_dir_path, sizeof blob_dir_path,
					fs_data,
					"/"
	);

	err = digest_module->fs_dir_path(
				p,
				sizeof blob_dir_path - (p - blob_dir_path)
	);
	if (err)
		return err;

	TRACE2("blob dir path", blob_dir_path);

	err = jmscott_mkdirat_path(end_point_fd, blob_dir_path, 0777);
	if (err)
		return err;

	fs_add_chat("ok");

	if (input_path) {
		err = fs_hard_link(
				ok_no,
				AT_FDCWD,
				input_path,
				end_point_fd,
				blob_path
		);
		if (err) {
			if (errno == EEXIST) {
				fs_add_chat("ok");
				return (char *)0;
			}
			if (err[0])
				return err;
		} else {
			TRACE("hard linked");
			fs_add_chat("ok");
			return (char *)0;
		}
	}
	TRACE("xdev failed, so copy input->blob");

	/*
	 *  First copy blob into $BLOBIO_ROOT/tmp/put-<udig>-<pid>.blob
	 */
	err = jmscott_mkdirat_path(end_point_fd, "tmp", 0777);
	if (err)
		return err;

	//  copy blob to tmp/put-<udig>.<pid> before renaming to data/
	//  assume data/ and tmp/ on same volume, which may not be true!

	char tmp_path[BLOBIO_MAX_FS_PATH+1];
	tmp_path[0] = 0;

	char pid[21], epoch[21];
	*jmscott_ulltoa((unsigned long long)getpid(), pid) = 0;
	*jmscott_ulltoa((unsigned long long)time((time_t *)0), epoch) = 0;

	jmscott_strcat7(tmp_path, sizeof tmp_path,
				"tmp/put-",
				udig,
				"-",
				epoch,
				"-",
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

	TRACE("sendfile input>tmp");
	err = jmscott_send_file(input_fd, tmp_fd, &blob_size);
	if (err)
		DEFER;

	TRACE("rename tmp->data");
	//  Note: assume $BLOBIO_ROOT/tmp on same device as $blob_path
	if (jmscott_renameat(end_point_fd, tmp_path, end_point_fd, blob_path))
		DEFER;
	fs_add_chat("ok");
BYE:
	if (tmp_fd >= 0) {
		if (jmscott_unlinkat(end_point_fd, tmp_path, 0) && !err) {
			if (errno != ENOENT)
				err = strerror(errno);
		}
		if (jmscott_close(tmp_fd) && !err)
			err = strerror(errno);
	}
	if (!err)
		*ok_no = 0;
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
		fs_add_chat("no");
		TRACE("could not put blob");
		return (char *)0;
	}

	//  Note: ignoring ENOENT needs a think.
	if (input_path && jmscott_unlink(input_path) && errno != ENOENT)
		return strerror(errno);
	fs_add_chat("ok");
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
			fs_add_chat("no");
			*ok_no = 1;
			return (char *)0;
		}
		return strerror(errno);
	}
	TRACE("blob exists");
	fs_add_chat("ok");
	*ok_no = 0;
	return (char *)0;
}

static char *
fs_eat_input(int fd)
{
	TRACE("entered");
	ascii_digest[0] = 0;
	char *err = digest_module->init();
	if (err)
		return err;
	err = digest_module->eat_input(fd);
	if (err)
		return err;
	TRACE2("digest", ascii_digest);
	return jmscott_rewind(fd);
}

/*
 *  Synopsis:
 *	Wrap the contents of spool/wrap after moving spool/fs.brr to spool/wrap.
 *  Description:
 *	Wrap occurs in two steps.
 *
 *	First, move a frozen version of spool/bio4d.brr file into the file
 *	spool/wrap/<udig>.brr, where <udig> is the digest of the frozen
 *	blobified brr file.  Do a trusted "put" on the frozen file.
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
 * 	and then digest THAT set into a blob and do a "put" of that udig set.
 *	The udig of THAT set is returned to the client and the set is moved to
 *	spool/roll/<udig>.uset.
 *
 *  Note:
 *	Do a stat on the frozen file before and after digesting, since
 *	we are not positive another process has that file open!
 *
 *	Triple check existence handling regarding uniqueness of renamed
 *	frozen/wrap brr log files.  Having the previous wrap udig in current
 *	wrap pretty much insures uniquesness ... but edge cases may exist.
 *
 *	FIX ME: Dangling unclosed files or tmp/ exist!
 */
static char *
fs_wrap(int *ok_no)
{
	TRACE("entered");

	/*
	 *  Insure dir $BLOBIO_ROOT/spool exists
	 */
	if (jmscott_mkdirat_EEXIST(end_point_fd, "spool", 0777))
		return strerror(errno);

	/*
	 *  Freeze spool/fs.brr by moving
	 *
	 *	spool/fs.brr -> spool/FROZEN-fs-<epoch>-<pid>.brr
	 *
	 *  Note:
	 *	How to insure no other process have open, other than locking?
	 */
	char brr_path[BLOBIO_MAX_FS_PATH+1];
	brr_path[0] = 0;
	jmscott_strcat(brr_path, sizeof brr_path, "spool/fs.brr");

	TRACE2("brr path", brr_path);

	char pid[21], epoch[21];
	*jmscott_ulltoa((unsigned long long)getpid(), pid) = 0;
	*jmscott_ulltoa((unsigned long long)time((time_t *)0), epoch) = 0;

	char frozen_path[BLOBIO_MAX_FS_PATH+1];
	frozen_path[0] = 0;
	jmscott_strcat5(frozen_path, sizeof frozen_path,
			"spool/FROZEN-fs-",
			epoch,
			"-",
			pid,
			".brr"
	);
	TRACE2("fs.brr -> frozen path", frozen_path);
	int status = jmscott_renameat(
				end_point_fd,
				brr_path,
				end_point_fd,
				frozen_path
	);
	if (status) {
		if (errno != ENOENT)
			return strerror(errno);

		/*
		 *  Blob Request Record file does not exist, so no wrap.
		 */
		TRACE("spool/fs.brr gone (ok)");
		fs_add_chat("no");
		*ok_no = 1;
		return (char *)0;
	}

	/*
	 *  Derive the digest of the frozen BRR log file, using the digest
	 *  algorithm in query arg "algo=".
	 */
	TRACE("open frozen brr log for digesting");
	int frozen_fd = jmscott_openat(
				end_point_fd,
				frozen_path,
				O_RDONLY,
				0
	);
	if (frozen_fd < 0)
		return strerror(errno);

	char *err = fs_eat_input(frozen_fd);
	if (err)
		return err;
	TRACE2("frozen digest", ascii_digest);

	/*
	 *  Insure dir $BLOBIO_ROOT/spool/wrap exists
	 */
	if (jmscott_mkdirat_EEXIST(end_point_fd, "spool/wrap", 0777))
		return strerror(errno);

	/*
	 *  Do a "put" on the frozen *.brr file.
	 *
	 *  Note:  really, really need to refactor code to be properly
	 *         reentrant via libblobio.a.  really.
	 */
	udig[0] = 0;
	jmscott_strcat3(udig, sizeof udig, algorithm, ":", ascii_digest);
	TRACE("\"put\" frozen brr file");
	input_fd = frozen_fd;
	err = fs_put(ok_no);
	if (jmscott_close(frozen_fd) && !err)
		err = strerror(errno);
	if (err)
		return err;
	if (*ok_no == 1) {
		TRACE("\"put\" failed, so \"wrap\" failed");
		return (char *)0;
	}

	/*
	 *  Move frozen Blob Request Records log file to
	 *
	 *	spool/wrap/<udig>.brr
	 */
	char wrap_path[BLOBIO_MAX_FS_PATH+1];
	wrap_path[0] = 0;
	jmscott_strcat5(wrap_path, sizeof wrap_path,
			"spool/wrap/",
			algorithm,
			":",
			ascii_digest,
			".brr"
	);

	TRACE("move frozen->wrap/");
	status = jmscott_renameat(
			end_point_fd,
			frozen_path,
			end_point_fd,
			wrap_path
	);
	if (status)
		return strerror(errno);

	/*
	 *  Build the set of udigs (*.uset) by scanning file names in dir
	 *  spool/wrap/.brr.  The uset file is assembled in
	 *
	 *	tmp/wrap-set-<epoch>-<pid>.uset
	 */
	TRACE("build the wrap set tmp/wrap-set-<epoch>-<pid>.uset");
	char wrap_set_path[BLOBIO_MAX_FS_PATH+1];
	wrap_set_path[0] = 0;
	jmscott_strcat5(wrap_set_path, sizeof wrap_set_path,
				"tmp/wrap-set-",
				epoch,
				"-",
				pid,
				".uset"
	);
	TRACE2("wrap set path", wrap_set_path);
	int wrap_set_fd = jmscott_openat(
				end_point_fd,
				wrap_set_path,
				O_RDWR | O_EXCL | O_CREAT,
				0700
	);
	if (wrap_set_fd < 0)
		return strerror(errno);
	TRACE("build wrap set by scanning spool/wrap ...");
	int wrap_dir_fd = jmscott_openat(
				end_point_fd,
				"spool/wrap",
				O_DIRECTORY,
				0777
	);
	if (wrap_dir_fd < 0)
		return strerror(errno);
	TRACE("open wrap dir for scanning");
	DIR *wdp = jmscott_fdopendir(wrap_dir_fd);
	if (!wdp)
		return strerror(errno);

	TRACE("begin scan of spool/wrap");
	errno = 0;
	struct dirent *ep;
	while ((ep = jmscott_readdir(wdp))) {

		TRACE2("wrap dir entry", ep->d_name);

		//  skip over non-files or paths too short or path
		//  not ending in ".brr".

		if (ep->d_type != DT_REG) {
			TRACE2("dir entry a regular file", ep->d_name);
			continue;
		}

		char *nm = ep->d_name;

		//  must be >= min udig+len(.brr)
		int nlen = strlen(nm);
		if (nlen < 38 || nlen > 141) {
			TRACE2("WARN: dir entry length too large or small", nm);
			continue;
		}

		//  find the "." at end of <udig>.brr
		char *per = nm + (nlen - 4);
		if (strcmp(per, ".brr")) {
			TRACE2("WARN: dir entry not terminated by .brr", nm);
			continue;
		}
		
		//  pass to udig frisker by replacing "." with null
		*per = 0;
		if (jmscott_frisk_udig(nm)) {
			TRACE2("dir entry not a udig", nm);
			continue;
		}

		//  write udig with new-line terminator

		*per = '\n';
		status = jmscott_write_all(wrap_set_fd, nm, nlen - 3);
		*per = 0;
		if (status)
			return strerror(errno);
		TRACE2("added wrap entry to udig set", nm);
	}
	if (errno > 0)
		return strerror(errno);
	TRACE("scan finished");
	if (jmscott_close(wrap_dir_fd))
		return strerror(errno);
	err = jmscott_rewind(wrap_set_fd);
	if (err)
		return err;

	err = fs_eat_input(wrap_set_fd);
	if (err)
		return err;
	TRACE2("wrap set digest", ascii_digest);

	/*
	 *  Do a "put" on the wrap udig set and
	 *  write that udig to output_fd.
	 */
	udig[0] = 0;
	jmscott_strcat3(udig, sizeof udig, algorithm, ":", ascii_digest);
	input_fd = wrap_set_fd;
	TRACE("\"put\" wrap udig set");
	err = fs_put(ok_no);
	if (err)
		return err;

	fs_add_chat("ok");
	*ok_no = 0;
	return (char *)0;
}

/*
 *  Frisk the blob request record and open the spool/fs.brr file.
 *  The transport and chat history are set in the blob request record..
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
