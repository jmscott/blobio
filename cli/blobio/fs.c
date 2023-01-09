/*
 *  Synopsis:
 *	A driver for a fast, trusting posix file system blobio service.
 *  Note:
 *	In func fs_wrap() need to move write of udig for uset above the driver.
 *
 *	Why does fs_wrap need to created the dir <brr-path>/wrap ?
 *
 *	Need to properly use separator char in tmp_path.
 *
 *	Only ascii is acccepted in the file system path!
 *
 *	In a "take" with a brr path a race condition exists between when a
 *	blob is deleted and and the stat() is done for the blob_size of the
 *	brr.
 *
 *  	Long (>= PATH_MAX) file paths are still problematic.
 *
 *	The input stream is not drained when the blob exists during a put/give.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>

#include "jmscott/libjmscott.h"
#include "blobio.h"

//  does the verb imply a possible write to the file system?

#define IS_WRITE_VERB()	(*verb == 'p' || (*verb == 'g' && *verb == 'i'))

extern char	verb[];
extern char	BR[];
extern char	algorithm[];
extern char	algo[];			//  service query arg "algo"
extern char	chat_history[9];
extern char	transport[129];
extern char	*output_path;
extern int	output_fd;
extern char	*input_path;
extern int	input_fd;
extern char	*null_device;
extern char	ascii_digest[];
extern struct digest		*digests[];
extern unsigned long long	blob_size;

//  extracted from $BLOBIO_SERVICE=fs:<fs_path><qargs>?

/*
 *  Note:
 *	"blob_path" is partially assembled in more than a single function(),
 *	which is confusing and error prone.
 */
static char	blob_path[PATH_MAX] = {0};

extern struct service fs_service;

/*
 *  The end point is the root directory of the source blobio file system.
 *  The query args (following '?') have already been removed.
 *
 *  Note:
 *	Eventually UTF8 will be allowed.
 */
static char *
fs_end_point_syntax(char *root_dir)
{
	char *p = root_dir;

	if (p - root_dir >= (PATH_MAX - 59))
		return "too many chars in root directory path";
	return (char *)0;
}

/*
 *  Insure all dirs exist: $BLOBIO_ROOT/{data,spool}
 */
static char *
fs_mkdirs()
{
	char *err;
	char path[PATH_MAX];

	//  $BLOBIO_ROOT
	path[0] = 0;
	jmscott_strcat(path, sizeof path, BR);
	TRACE2("BLOBIO_ROOT", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;

	char *root_slash = path + strlen(path);

	//  $BLOBIO_ROOT/spool/
	jmscott_strcat(path, sizeof path, "/spool");
	TRACE2("spool/ path", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;
	char *spool_slash = root_slash + 6;

	//  $BLOBIO_ROOT/spool/wrap
	*spool_slash = 0;
	jmscott_strcat(path, sizeof path, "/wrap");
	TRACE2("spool/wrap path", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;

	//  $BLOBIO_ROOT/spool/roll
	*spool_slash = 0;
	jmscott_strcat(path, sizeof path, "/roll");
	TRACE2("spool/roll path", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;

	//  $BLOBIO_ROOT/data/
	*root_slash = 0;
	jmscott_strcat(path, sizeof path, "/data");
	TRACE2("data/ path", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;
	char *data_slash = root_slash + 5;

	//  make all dirs for $BLOBIO_ROOT/data/fs_<algo>
	for (int i = 0;  digests[i];  i++) {
		struct digest *dp = digests[i]; 

		*data_slash = 0;
		jmscott_strcat2(path, sizeof path, "/fs_", dp->algorithm);
		TRACE2("data/fs_<algo>/ path", path);
		if ((err = jmscott_mkdir_p(path)))
			return err;
	}

	//  $BLOBIO_ROOT/tmp/
	*root_slash = 0;
	jmscott_strcat(path, sizeof path, "/tmp");
	TRACE2("tmp/ path", path);
	if ((err = jmscott_mkdir_p(path)))
		return err;

	return (char *)0;
}

/*
 *  Parse service query args and possibly create dirs 
 *	$BLOBIO_ROOT/{data,run,spool,tmp} exist.
 *  Note:
 *	Should perms be checked for dirs under $BLOBIO_ROOT?
 *
 *	setting BR=~ makes BR=<end_point>.  why would that not be the default?
 */
static char *
fs_open()
{
	char *end_point = fs_service.end_point;

	if (algo[0] && !find_digest(algo))
		return "unknown digest algorithm in query arg \"algo\"";

	//  tilde shorthand for the file system root in end point
	if (strcmp("~", BR) == 0) {
		BR[0] = 0;
		jmscott_strcat(BR, PATH_MAX, end_point);
		TRACE2("BR ~ reset to fs path", BR);
	}

	//  make dirs under $BLOBIO_ROOT
	char *err = fs_mkdirs();
	if (err)
		return err;

	*blob_path = 0;
	if (algo[0])
		jmscott_strcat4(blob_path, sizeof blob_path,
				end_point,
				"/data/",
				"fs_",
				algo
		);
	TRACE2("data fs path", blob_path);
			
	return (char *)0;
}

static char *
fs_open_output()
{
	return (char *)0;
}

static char *
fs_close()
{
	return (char *)0;
}

static char *
fs_copy(char *in_path, char *out_path)
{
	int in = -1, out = -1;
	int nr;
	int e;
	unsigned char buf[MAX_ATOMIC_MSG];

	//  open input path or point to standard input

	if (in_path) {
		in = jmscott_open(in_path, O_RDONLY, 0);
		if (in < 0)
			return strerror(errno);
	} else
		in = input_fd;

	//  open output path or point to standard out

	if (out_path) {
		out = jmscott_open(out_path, O_WRONLY|O_CREAT,S_IRUSR|S_IRGRP);
		if (out < 0)
			return strerror(errno);
	} else
		out = output_fd;

	while ((nr = jmscott_read(in, buf, sizeof buf)) > 0) {
		if (jmscott_write(out, buf, nr) < 0)
			break;
		blob_size += nr;
	}
	e = errno;
	if (in != -1)
		jmscott_close(in);
	if (out != -1)
		jmscott_close(out);
	errno = e;
	if (e != 0)
		return strerror(e);
	return (char *)0;
}

/*
 *  Set global variables related to blog request records.
 *
 *  Note:
 *	Really, really, really need to refactor this code about the service
 *	level.  Too much replication between services "fs" and "bio4".
 */
static char *
fs_set_brr(char *hist, char *fs_path) {

	struct stat st;

	blob_size = 0;
	TRACE2("fs_path", fs_path);
	if (fs_path && fs_path[0]) {
		if (stat(fs_path, &st)) {
			if (errno == ENOENT)
				return "stat(fs_path): file does not exist";
			return strerror(errno);
		}
		blob_size = st.st_size;
	}

	snprintf(transport, sizeof transport, "fs~%d:", getpid());

	strcat(transport, fs_service.end_point);
	TRACE2("transport", transport);

	strcpy(chat_history, hist);
	TRACE2("chat history", hist);

	return (char *)0;
}

/*
 *  Get a blob.
 *
 *  If output path exists then link the output path to the blob path.
 *  Copy the input to the output if the blobs are on different devices.
 */
static char *
fs_get(int *ok_no)
{
	char *err;
	int len;

	*ok_no = 1;

	//  build path to source blob file

	len = strlen(blob_path);
	err = fs_service.digest->fs_path(
					blob_path + len,
					sizeof blob_path - len
	);
	if (err)
		return err;

	//  link output path to source blob file
	
	if (output_path && output_path != null_device) {
		if (jmscott_link(blob_path, output_path) == 0) {
			*ok_no = 0;
			return fs_set_brr("ok", blob_path);
		}

		//  blob file does not exist

		if (errno == ENOENT)
			return fs_set_brr("no", (char *)0);

		//  linking not allowed, either cross link or permissions
		if (errno != EXDEV && errno != EPERM)
			return strerror(errno);
	}
	if ((err = fs_copy(blob_path, output_path))) {
		if (errno == ENOENT)
			return fs_set_brr("no", (char *)0);
		return err;
	}
	*ok_no = 0;
	return fs_set_brr("ok", blob_path);
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
fs_eat(int *ok_no)
{
	char *err;
	int len;

	jmscott_strcat(blob_path, sizeof blob_path, "/");
	len = strlen(blob_path);
	err = fs_service.digest->fs_path(
					blob_path + len,
					sizeof blob_path - len
	);
	if (err)
		return err;

	//  insure we can read the file.

	if (jmscott_access(blob_path, R_OK)) {
		if (errno == ENOENT) {
			*ok_no = 1;
			return fs_set_brr("no", (char *)0);
		}
		return strerror(errno);
	}
	*ok_no = 0;
	return fs_set_brr("ok", (char *)0);
}

static char *
fs_put(int *ok_no)
{
	char *err = (char *)0;
	char *np;
	int in_fd, tmp_fd;
	int nr;
	unsigned char buf[MAX_ATOMIC_MSG];

	//  Name of temporary file
	//  size: 1 + 8 + 1 + 21 + 1 + 21 + 1

	char tmp_name[57];	//  /<verb>-<epoch>.<pid>\0

	*ok_no = 1;

	//  append /<blob-file-name> to the path to the blob

	jmscott_strcat(blob_path, sizeof blob_path, "/");
	np = blob_path + strlen(blob_path);
	err = fs_service.digest->fs_name(np, PATH_MAX - (np - blob_path));
	if (err)
		return err;

	//  if the blob file already exists then we are done

	if (jmscott_access(blob_path, F_OK) == 0) {
		*ok_no = 0;

		//  Note: what about draining the input not bound to a file?
		return fs_set_brr("ok,ok", blob_path);
	}
	if (errno != ENOENT)
		return strerror(errno);

	//  try to hard link the target blob to the existing source blob

	if (input_path) {
		if (jmscott_link(input_path, blob_path) == 0		||
		    errno == EEXIST) {
			*ok_no = 0;
			return fs_set_brr("ok,ok", blob_path);
		}

		if (errno != EXDEV)
			return strerror(errno);
	}

	//  can not link input file to blob in data/fs_<algo>/XXX/XXX/<digest>

	//  build path to temporary file which will accumulate the blob.

	char now[21];
	snprintf(now, sizeof now, "%d", (int)time((time_t *)0));

	snprintf(tmp_name, sizeof tmp_name,
			"%s-%s-%d.blob",
			verb,
			now,
			getpid()
	);
	char tmp_path[PATH_MAX];
	tmp_path[0] = 0;
	jmscott_strcat3(tmp_path, sizeof tmp_path,
			BR,
			"/tmp/",
			tmp_name
	);

	//  open input file.  may be standard input

	if (input_path) {
		in_fd = jmscott_open(input_path, O_RDONLY, 0);
		if (in_fd < 0)
			return strerror(errno);
	} else
		in_fd = input_fd;

	tmp_fd = jmscott_open(
			tmp_path,
			O_WRONLY | O_CREAT, 
			S_IRUSR | S_IRGRP
	);
	if (tmp_fd < 0)
		return strerror(errno);

	//  copy the blob to temporary path.

	while ((nr = jmscott_read(in_fd, buf, sizeof buf)) > 0)
		if (jmscott_write(tmp_fd, buf, nr) < 0) {
			err = strerror(errno);
			break;
		}
	if (nr < 0)
		err = strerror(errno);

	//  copy successfull so link temp and final blob.

	if (err == (char *)0) {

		if (jmscott_link(tmp_path, blob_path) && errno != EEXIST) {
			err = strerror(errno);
		} else
			*ok_no = 0;
	}

	if (jmscott_close(tmp_fd) && err == (char *)0)
		err = strerror(errno);
	if (jmscott_unlink(tmp_path) && err == (char *)0)
		err = strerror(errno);
	return fs_set_brr("ok,ok", blob_path);
}

static char *
fs_take(int *ok_no)
{
	char *err = (char *)0;

	err = fs_get(ok_no);
	if (err)
		return err;
	if (*ok_no)
		return fs_set_brr("no", (char *)0);
	fs_set_brr("ok,ok,ok", blob_path);
	if (jmscott_unlink(blob_path) != 0 && errno != ENOENT)
		return strerror(errno);
	return (char *)0;
}

static char *
fs_give(int *ok_no)
{
	char *err = fs_put(ok_no);
	if (err)
		return err;
	return fs_set_brr("ok,ok,ok", blob_path);
}

/*
 *  exec "blobio put" for a given input path.
 *
 *  Note:
 *	why derive the udig, since we are given the input path?
 */
static char *
fs_exec_put(char *udig, char *input_path)
{
	if (tracing) {
		TRACE2("udig", udig);
		TRACE2("input path", input_path);
	}
	pid_t put_pid;
AGAIN:
	put_pid = fork();
	if (put_pid < 0) {
		if (errno == EAGAIN)
			goto AGAIN;
		return strerror(errno);
	}

	//  wait for the "blobio put" child to exit
	//
	//  Note:
	//	Refactor code for both fs_exec_{put,get}!
	if (put_pid > 0) {
		int status;
		
		if (waitpid(put_pid, &status, 0) < 0) {
			int e = errno;

			TRACE2("waitpid() failed", strerror(e));
			return strerror(e);
		}

		if (WIFEXITED(status)) {
			TRACE_ULL("waitpid() exited: status", status);
			if (WEXITSTATUS(status) != 0)
				return "blobio put wrap failed";
			return (char *)0;
		}
		if (WIFSIGNALED(status))
			return "\"blobio put\" exit due to signal";
		if (WCOREDUMP(status))
			return "\"blobio put\" core dumped";
		if (WSTOPSIG(status))
			return "\"blobio put\" STOPPED";
		return "unexpected status from waitpid()";
	}

	char srv[PATH_MAX * 2];
	srv[0] = 0;
	jmscott_strcat2(srv, sizeof srv,
			"fs:",
			fs_service.end_point
	);
	if (fs_service.query[0])
		jmscott_strcat2(srv, sizeof srv, "?", fs_service.query);

	//  in the child, so exec the "blobio put"
	if (tracing) {
		TRACE2("child: udig", udig);
		TRACE2("child: service", srv);
		TRACE2("child: input path", input_path);
	}
	execlp("blobio",
		"blobio", "put",
		"--udig", udig,
		"--service", srv,
		"--input-path", input_path,
		(char *)0
	);
	return strerror(errno);
}

/*
 *  Steps: "wrap"
 *	B=$BLOBIO_ROOT
 *
 *	ALGO=$(extract and verify service query arg "[&?]alog=(sha|btc20)")
 *
 *	#  "freeze" file spool/fs.brr so no other "wrap" can access.
 *
 *	FROZEN_BRR=$B/spool/fs-<epoch>-<pid>.brr
 *	xlock and freeze file $B/spool/fs.brr by renaming to $FROZEN_BRR
 *
 *	#  digest and move frozen blob request log (*.brr) to spool/wrap/
 *
 *	BRR_UDIG=algo:$(digest $FROZEN_BRR)
 *	move $FROZEN_BRR spool/wrap/$BRR_UDIG.brr
 *
 *	#  write set of udigs for the *.brr in wrap/ into $B/tmp/wrap.uset
 *
 *	WRAP_USET=$B/tmp/wrap-<now>-<pid>.uset
 *	find $B/spool/wrap					\
 *		-maxdepth 1					\
 *		-type f						\
 *		-name '[a-z]*:*.brr'				|
 *			xargs basename -s .brr >$WRAP_USET
 *	WRAP_UDIG=algo:$(digest $WRAP_USET)
 *	blobio give $WRAP_SET
 *
 *	echo $WRAP_UDIG
 */
static char *
fs_wrap(int *ok_no)
{
	char brr_path[PATH_MAX];
	char *err = (char *)0;

	//  fs: requires algo in query arg "[&?]algo=(sha|btc20)"

	if (!algo[0])
		return "qarg \"algo\" is missing";
	struct digest *d = find_digest(algo);
	if (!d)
		return "impossible: unknown digest";	//  already verified
	if ((err = d->init()))
		return err;

	brr_path[0] = 0;
	jmscott_strcat2(brr_path, sizeof brr_path, BR, "/spool/fs.brr");
	TRACE2("brr path", brr_path);

	char now[21];
	snprintf(now, sizeof now, "%d", (int)time((time_t *)0));

	/*
	 *  build a "unique" path for freezing brr file,
	 *  named $BLOBIO_ROOT/spool/fs-<unix-epoch>-<pid>.brr
	 */
	char pid[21];
	jmscott_ulltoa((unsigned long long)getpid(), pid);

	char frozen_brr_path[PATH_MAX];
	frozen_brr_path[0] = 0;
	jmscott_strcat6(frozen_brr_path, sizeof brr_path,
		BR,
		"/spool/fs-",
		now,
		"-",
		pid,
		".brr"
	);
	TRACE2("frozen brr path", frozen_brr_path);

	/*
	 *  Open brr file with exclusive lock, to block the other shared lock
	 *  writers in brr_write().  hold lock briefly to rename fs.brr to
	 *  fs-<unix epoch>-<pid>.brr.  if no brr file exists then an empty brr
	 *  is created.
	 *
	 *  Note:
	 *	exclusive lock is not posix for open() syscall.
	 *	macos DOES support xlock on open(), linux does not.
	 *
	 *	a race still exists
	 */
	int brr_fd = jmscott_open(
		brr_path,
		O_RDONLY|O_CREAT,
		S_IRUSR|S_IWUSR|S_IRGRP
	);
	if (brr_fd < 0)
		return strerror(errno);			//  lock freed on exit
	if (jmscott_flock(brr_fd, LOCK_EX))
		die2("flock(brr:LOCK_EX) failed", strerror(errno));
	int status = rename(brr_path, frozen_brr_path);
	err = (char *)0;
	if (status)
		err = strerror(status);
	
	/*
	 *  close(), regardless of error, to release exclusive lock.
	 *
	 *  Note:
	 *	what to do with the existing, frozen brr file?
	 */
	if (jmscott_flock(brr_fd, LOCK_UN))
		die2("flock(brr:LOCK_UN) failed", strerror(errno));
	if (jmscott_close(brr_fd) && err == (char *)0)
		return strerror(errno);
	if (err)
		return err;		//  earlier error has higher priority

	//  Note:
	//	hack to fool digest code.
	//	really, really, really, really need to refactor code
	//	into libblobio.a.  this is terrible coding practioce.
	strcpy(algorithm, algo);

	int frozen_fd = jmscott_open(frozen_brr_path, O_RDONLY, 0);
	if (frozen_fd < 0)
		return strerror(errno);

	ascii_digest[0] = 0;
	if ((err = d->eat_input(frozen_fd)))
		return err;
	//  sanity test.  really, really need to refactor code
	if (!ascii_digest[0])
		return "impossible: null algo after frozen digest";
	TRACE2("ascii digest", ascii_digest);

	if (jmscott_close(frozen_fd) && err == (char *)0)
		return strerror(errno);

	char put_udig[8 + 1 + 128 + 1 + 1];	//  <algo>:<digest>\n\0
	put_udig[0] = 0;
	char *put_udig_null = jmscott_strcat3(put_udig, sizeof put_udig,
			algorithm,
			":",
			ascii_digest
	);
	TRACE2("put udig", put_udig);

	//  move frozen brr file into spool/wrap/<wrap udig>.brr
	char wrap_brr_path[PATH_MAX];

	wrap_brr_path[0] = 0;
	jmscott_strcat4(wrap_brr_path, sizeof wrap_brr_path,
		BR,
		"/spool/wrap/",
		put_udig,
		".brr"
	);
	TRACE2("wrap brr path", wrap_brr_path);
	if (rename(frozen_brr_path, wrap_brr_path))
		return strerror(errno);

	//  exec a "blobio put" of the frozen wrapped file.

	if ((err = fs_exec_put(put_udig, wrap_brr_path)))
		return err;

	char wrap_uset_path[PATH_MAX];
	wrap_uset_path[0] = 0;
	jmscott_strcat6(wrap_uset_path, sizeof wrap_uset_path,
			BR,
			"/tmp/wrap-",
			now,
			"-",
			pid,
			".uset"
	);
	TRACE2("wrap uset path", wrap_uset_path);
	int wu_fd = jmscott_open(
			wrap_uset_path,
			O_CREAT | O_EXCL | O_WRONLY,
			S_IRUSR
	);
	if (wu_fd < 0)
		return strerror(errno);

	char wrap_dir_path[PATH_MAX];
	wrap_dir_path[0] = 0;
	jmscott_strcat2(wrap_dir_path, sizeof wrap_dir_path,
			BR,
			"/spool/wrap"
	);
	TRACE2("wrap dir path", wrap_dir_path);
	int dir_fd = open(wrap_dir_path, O_RDONLY, 0);
	if (dir_fd < 0)
		return "open(wrap/) failed";
	DIR *dirp = fdopendir(dir_fd);
	if (dirp == (DIR *)0)
		return "fdopendir(wrap) failed";

	err = NULL;
	struct dirent *ep;
	while (err == NULL && (ep = readdir(dirp)) != NULL) {
		struct stat st;

		TRACE2("wrap dir entry", ep->d_name);

		if (fstatat(dir_fd, ep->d_name, &st, 0))
			return strerror(errno);
		if (!S_ISREG(st.st_mode))
			continue;
		char *period = rindex(ep->d_name, '.');
		if (period == NULL)
			continue;
		int ulen = period - ep->d_name;
		if (ulen < 3 || ulen > 8 + 1 + 128)
			continue;
		char udig[8 + 1 + 128 + 1];
		memcpy(udig, ep->d_name, ulen);
		udig[ulen] = 0;
		TRACE2("wrap/ udig candidate", udig);

		if (jmscott_frisk_udig(udig))
			continue;
		udig[ulen] = '\n';
		if (jmscott_write(wu_fd, udig, ulen + 1) < 0)
			return strerror(errno);
	}
	if (closedir(dirp) < 0)
		return strerror(errno);
	if (jmscott_close(wu_fd) < 0)
		return strerror(errno);

	TRACE2("put udig", put_udig);
	*put_udig_null++ = '\n';
	*put_udig_null = 0;
	if (jmscott_write(output_fd, put_udig, put_udig_null - put_udig) < 0)
		return strerror(errno);

	*ok_no = 0;
	return fs_set_brr("ok", blob_path);
}

/*
 *  exec "blobio get" for a given udig.
 */
static char *
fs_exec_get(char *udig, char *output_path)
{
	if (tracing) {
		TRACE2("udig", udig);
		TRACE2("input path", input_path);
	}
	pid_t get_pid;
AGAIN:
	get_pid = fork();
	if (get_pid < 0) {
		if (errno == EAGAIN)
			goto AGAIN;
		return strerror(errno);
	}

	//  wait for the "blobio get" child to exit
	if (get_pid > 0) {
		int status;
		
		if (waitpid(get_pid, &status, 0) < 0)
			return strerror(errno);

		if (WIFEXITED(status))
			return (char *)0;
		if (WIFSIGNALED(status))
			return "\"blobio get\" exit due to signal";
		if (WCOREDUMP(status))
			return "\"blobio get\" core dumped";
		if (WSTOPSIG(status))
			return "\"blobio get\" STOPPED";
		return "unexpected status from waitpid";
	}

	char srv[PATH_MAX * 2];
	srv[0] = 0;
	jmscott_strcat2(srv, sizeof srv,
			"fs:",
			fs_service.end_point
	);
	if (fs_service.query[0])
		jmscott_strcat2(srv, sizeof srv, "?", fs_service.query);

	//  in the child, so exec the "blobio get"
	execlp("blobio",
		"blobio", "get",
		"--udig", udig,
		"--service", srv,
		"--output-path", output_path,
		(char *)0
	);

	//  not reached
	return strerror(errno);
}

/*
 *  Read a wrap set of udigs to remove wrapped blob request
 *  logs files in spool/wrap/<udig>.brr.
 *
 *  Steps: roll <wrap-udig>
 *	USET=tmp/roll-<udig>-<epoch>-<pid>.uset
 *	blobio get <wrap-udig> >$USET
 *	while read UDIG;  do
 *		rm -f spool/wrap/$USET.brr
 *	done <$USET
 *	rm $USET
 */
static char *
fs_roll(int *ok_no)
{
	(void)ok_no;
	struct stat st;

	char udig[8 + 1 + 128 + 1];
	udig[0] = 0;
	jmscott_strcat3(udig, sizeof udig, algo, ":", ascii_digest);
		
	char roll_uset[PATH_MAX];
	snprintf(roll_uset, sizeof roll_uset,
		"tmp/roll-%s-%ld-%d.uset",
		udig,
		time((time_t *)0),
		getpid()
	);
	TRACE2("tmp roll set", roll_uset);
	char *err = fs_exec_get(udig, roll_uset);
	if (err)
		return err;
	int roll_fd = jmscott_open(roll_uset, O_RDONLY, 0);
	if (roll_fd < 0) {
		if (errno == ENOENT)
			return "can not fetch roll set";
		return strerror(errno);
	}
	/*
	 *  unlink the opened wrap set now, 
	 *  to insure disappears upon process exit.
	 */
	if (unlink(roll_uset) < 0)
		return strerror(errno);

	/*
	 *  Slurp the wrap set into memory.
	 */
	if (jmscott_fstat(roll_fd, &st) != 0)
		return strerror(errno);
	TRACE_ULL("roll set size", st.st_size);
	char *roll_udigs = malloc(st.st_size + 1);
	if (roll_udigs == NULL)
		return "malloc(roll udigs) failed";
	int status = jmscott_read_exact(roll_fd, roll_udigs, st.st_size);
	if (status != 0 && status != -3) {
		TRACE_LL("read_exact(roll udigs) failed: exit status", status);
		if (status == -1)
			return strerror(errno);
		if (status == -2)
			return "roll udigs: unexpected end of file";
		return "PANIC: read_exact(roll udigs): unexpected return value";
	}
	roll_udigs[st.st_size] = 0;
	if (jmscott_close(roll_fd))
		return strerror(errno);

	TRACE("scanning udigs in roll uset ...");
	char *rup = roll_udigs;
	while (*rup) {
		char *nl = index(rup, '\n');
		if (nl == NULL)
			return "expected new line in udig of roll uset";
		*nl = 0;
		TRACE2("wrap udig to roll", rup);
		err = jmscott_frisk_udig(rup);
		if (err)
			return err;
		rup = nl + 1;
	}

	return "\"roll\" not implemented (yet) service";
}

struct service fs_service =
{
	.name			=	"fs",
	.end_point_syntax	=	fs_end_point_syntax,
	.open			=	fs_open,
	.close			=	fs_close,
	.get			=	fs_get,
	.open_output		=	fs_open_output,
	.eat			=	fs_eat,
	.put			=	fs_put,
	.take			=	fs_take,
	.give			=	fs_give,
	.roll			=	fs_roll,
	.wrap			=	fs_wrap,
	.end_point		=	{0},
	.query			=	{0}
};
