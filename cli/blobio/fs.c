/*
 *  Synopsis:
 *	A driver for a fast, trusting posix file system blobio service.
 *  Note:
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
 *
 *	A typo in the BLOBIO_SERVICE yields a "blob not found error", which
 *	is confusing.  Instead, the existence of the data directory ought to
 *	tested for a more clear error message.  An earlier version of fs did
 *	this test.
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

static char	fs_path[PATH_MAX] = {0};
static char	tmp_path[PATH_MAX] = {0};

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
 *  Verify that the root and data directories of the blob file system
 *  are at least searchable.
 *
 *  Posix declares process id (pid_t) to be 32 bit signed int.
 */
static char *
fs_open()
{
	struct stat st;
	char *end_point = fs_service.end_point;

	if (algo[0] && !find_digest(algo))
		return "unknown digest algorithm in query arg \"algo\"";
	
	//  tilde shorthand for the file system root in end point
	if (strcmp("~", BR) == 0) {
		BR[0] = 0;
		jmscott_strcat(BR, PATH_MAX, end_point);
		TRACE2("BR ~ reset to fs path", BR);
	}
	//  Note: should this test be in blobio.c:xref_argv()?
	if (*verb == 'w') {
		if (!BR[0])
			return "query arg \"BR\" not defined";
		return (char *)0;
	}

	TRACE2("end point", end_point);
	if (jmscott_access(end_point, X_OK)) {
		if (errno == ENOENT)
			return "blob root directory does not exist";
		if (errno == EPERM)
			return "no permission to search blob root directory";
		return strerror(errno);
	}
			
	//  verify the end point is, in fact, a directory

	if (stat(end_point, &st))
		return strerror(errno);
	if (!S_ISDIR(st.st_mode))
		return "root blob directory is not a directory";
	//  build the path to the $BLOBIO_ROOT/data/fs_<algo>/ directory

	*fs_path = 0;
	jmscott_strcat4(
		fs_path,
		sizeof fs_path,
		end_point,
		"/data/",
		"fs_",
		algorithm
	);
	TRACE2("path", fs_path);

	//  verify permissons on data/ directory

	if (jmscott_access(fs_path, X_OK)) {
		if (errno == ENOENT)
			return "directory does not exist: data/fs_<algo>";
		if (errno == EPERM)
			return "no permission for directory: data/fs_<algo>/";
		return strerror(errno);
	}
			
	//  verify the data directory is, in fact, a directory

	if (stat(fs_path, &st))
		return strerror(errno);
	if (!S_ISDIR(st.st_mode))
		return "not a directory: data/fs_<algo>";

	//  if writing a blob then build path to temporary directory.

	if (IS_WRITE_VERB()) {
		*tmp_path = 0;
		jmscott_strcat2(tmp_path, sizeof tmp_path, fs_path, "/tmp");
	}

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
 *	Really, really need to push this code about the service level.
 *	Too much replication between services "fs" and "bio4".
 */
static char *
fs_set_brr(char *hist, char *fs_path) {

	struct stat st;

	blob_size = 0;
	TRACE2("fs_path", fs_path);
	if (fs_path && fs_path[0]) {
		if (stat(fs_path, &st)) {
			if (errno == ENOENT)
				return "stat(fs_path): file gone";
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

	len = strlen(fs_path);
	err = fs_service.digest->fs_path(
				fs_path + len,
				PATH_MAX - len
	);
	if (err)
		return err;

	//  link output path to source blob file
	
	if (output_path && output_path != null_device) {
		if (jmscott_link(fs_path, output_path) == 0) {
			*ok_no = 0;
			return fs_set_brr("ok", fs_path);
		}

		//  blob file does not exist

		if (errno == ENOENT)
			return fs_set_brr("no", (char *)0);

		//  linking not allowed, either cross link or permissions
		if (errno != EXDEV && errno != EPERM)
			return strerror(errno);
	}
	if ((err = fs_copy(fs_path, output_path))) {
		if (errno == ENOENT)
			return fs_set_brr("no", (char *)0);
		return err;
	}
	*ok_no = 0;
	return fs_set_brr("ok", fs_path);
}

/*
 *  Eat a blob file by verifying existence and readability.
 */
static char *
fs_eat(int *ok_no)
{
	char *err;
	int len;

	jmscott_strcat(fs_path, sizeof fs_path, "/");
	len = strlen(fs_path);
	err = fs_service.digest->fs_path(fs_path + len, PATH_MAX - len);
	if (err)
		return err;

	//  insure we can read the file.

	if (jmscott_access(fs_path, R_OK)) {
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

	TRACE("entered");

	//  Name of temporary file
	//  size: 1 + 8 + 1 + 21 + 1 + 21 + 1

	char tmp_name[57];	//  /<verb>-<epoch>.<pid>\0

	*ok_no = 1;

	//  make the full directory path to the blob

	TRACE2("path", fs_path);
	err = fs_service.digest->fs_mkdir(fs_path, PATH_MAX - strlen(fs_path));
	if (err)
		return err;
	TRACE2("path", fs_path);

	//  append /<blob-file-name> to the path to the blob

	jmscott_strcat(fs_path, sizeof fs_path, "/");
	np = fs_path + strlen(fs_path);
	err = fs_service.digest->fs_name(np, PATH_MAX - (np - fs_path));
	if (err)
		return err;

	//  if the blob file already exists then we are done

	if (jmscott_access(fs_path, F_OK) == 0) {
		*ok_no = 0;

		//  Note: what about draining the input not bound to a file?
		return fs_set_brr("ok,ok", fs_path);
	}
	if (errno != ENOENT)
		return strerror(errno);

	//  try to hard link the target blob to the existing source blob

	if (input_path) {
		if (jmscott_link(input_path, fs_path) == 0 || errno == EEXIST) {
			*ok_no = 0;
			return fs_set_brr("ok,ok", fs_path);
		}

		if (errno != EXDEV)
			return strerror(errno);
	}

	//  build path to temporary file which will accumulate the blob.

	snprintf(tmp_name, sizeof tmp_name, "/%s.%ul", verb, getpid());
	jmscott_strcat(tmp_path, sizeof tmp_path, tmp_name);

	//  open input file.  may be standard input

	if (input_path) {
		in_fd = jmscott_open(input_path, O_RDONLY, 0);
		if (in_fd < 0)
			return strerror(errno);
	} else
		in_fd = input_fd;

	//  open tempory file to for incoming blob

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

		if (jmscott_link(tmp_path, fs_path) && errno != EEXIST) {
			err = strerror(errno);
		} else
			*ok_no = 0;
	}

	if (jmscott_close(tmp_fd) && err == (char *)0)
		err = strerror(errno);
	if (jmscott_unlink(tmp_path) && err == (char *)0)
		err = strerror(errno);
	return fs_set_brr("ok,ok", fs_path);
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
	fs_set_brr("ok,ok,ok", fs_path);
	if (jmscott_unlink(fs_path) != 0 && errno != ENOENT)
		return strerror(errno);
	return (char *)0;
}

static char *
fs_give(int *ok_no)
{
	char *err = fs_put(ok_no);
	if (err)
		return err;
	return fs_set_brr("ok,ok,ok", fs_path);
}

static char *
make_wrap_dir(char *dir_path, int dir_path_size)
{
	dir_path[0] = 0;
	jmscott_strcat2(dir_path, dir_path_size, BR, "/spool/wrap");
	TRACE2("dir path", dir_path);
	if (jmscott_mkdir_p(dir_path))
		return strerror(errno);
	return (char *)0;
}

static char *
make_roll_dir(char *dir_path, int dir_path_size)
{
	dir_path[0] = 0;
	jmscott_strcat2(dir_path, dir_path_size, BR, "spool/roll");
	if (jmscott_mkdir_p(dir_path))
		return strerror(errno);
	return (char *)0;
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
	if (put_pid > 0) {
		int status;
		
		if (waitpid(put_pid, &status, 0) < 0) {
			int e = errno;

			TRACE2("waitpid() failed", strerror(e));
			return strerror(e);
		}

		if (WIFEXITED(status)) {
			TRACE("waitpid() ok");
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

static char *
fs_wrap(int *ok_no)
{
	char brr_path[PATH_MAX];
	char spool_path[PATH_MAX];

	if (!algo[0])
		return "qarg \"algo\" is missing";

	spool_path[0] = 0;
	jmscott_strcat2(spool_path, sizeof spool_path, BR, "/spool");
	TRACE2("spool path", spool_path);

	//  path to brr file to wrap
	brr_path[0] = 0;
	jmscott_strcat2(
		brr_path,
		sizeof brr_path,
		spool_path,
		"/fs.brr"
	);

	TRACE2("brr path", brr_path);

	char now[21];
	snprintf(now, sizeof now, "%d", (int)time((time_t *)0));

	/*
	 *  build "unique" path to frozen brr file.
	 */
	char pid[21];
	jmscott_ulltoa((unsigned long long)getpid(), pid);

	char frozen_brr_path[PATH_MAX];
	frozen_brr_path[0] = 0;
	jmscott_strcat6(frozen_brr_path, sizeof brr_path,
		spool_path,
		"/fs-",
		now,
		"-",
		pid,
		".brr"
	);
	TRACE2("frozen brr path", frozen_brr_path);

	/*
	 *  Open brr file with exclusive lock, to block the other shared lock
	 *  writers in brr_write().  hold lock briefly to rename fs.brr to
	 *  fs-<unix epoch>.brr.  if no brr file exists then an empty brr
	 *  is created and renamed to fs-<epoch> empty.
	 *
	 *  Note:
	 *	exclusive lock is not posix for open() syscall.
	 *	i (jmscott) still not conviced that a race does not exist.
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
	char *err = (char *)0;
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

	/*
	 *  make the wrap/ dir, where the digested, frozen brr file be moved to.
	 */

	//  Note:
	//	hack to fool digest code.
	//	really, really, really, really need to refactor code!
	strcpy(algorithm, algo);

	int frozen_fd = jmscott_open(frozen_brr_path, O_RDONLY, 0);
	if (frozen_fd < 0)
		return strerror(errno);

	struct digest *d = find_digest(algo);
	if (!d)
		return "impossible: unknown digest";
	if ((err = d->init()))
		return err;

	ascii_digest[0] = 0;
	if ((err = d->eat_input(frozen_fd)))
		return err;
	//  sanity test.  really, really need to refactor code
	if (!ascii_digest[0])
		return "impossible: null algo after frozen digest";
	TRACE2("ascii digest", ascii_digest);

	if (jmscott_close(frozen_fd) && err == (char *)0)
		return strerror(errno);

	char put_udig[8 + 1 + 128 + 1];
	put_udig[0] = 0;
	jmscott_strcat3(put_udig, sizeof put_udig,
			algorithm,
			":",
			ascii_digest
	);
	TRACE2("put udig", put_udig);

	//  make the wrap dir
	char wrap_dir_path[PATH_MAX];
	err = make_wrap_dir(wrap_dir_path, sizeof wrap_dir_path);
	if (err)
		return err;
	TRACE2("wrap dir path", wrap_dir_path);

	//  move frozen brr file into wrap/<wrap udig>.brr
	char wrap_brr_path[PATH_MAX];

	wrap_brr_path[0] = 0;
	jmscott_strcat6(
		wrap_brr_path,
		sizeof wrap_brr_path,
		wrap_dir_path,
		"/",
		algo,
		":",
		ascii_digest,
		".brr"
	);
	TRACE2("wrap brr path", wrap_brr_path);
	if (rename(frozen_brr_path, wrap_brr_path))
		return strerror(errno);

	//  exec a "blobio put" of the frozen wrapped file.

	if ((err = fs_exec_put(put_udig, wrap_brr_path)))
		return err;

	if (jmscott_write(output_fd, (unsigned char*)put_udig,strlen(put_udig)))
		return strerror(errno);

	*ok_no = 0;
	return fs_set_brr("ok", fs_path);
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

#ifdef NO_COMPILE
static char *
fs_slurp_wrap(char *path, char *ent[PATH_MAX], int ent_size)
{
	(void)ent;

	TRACE2("path", path);
	if (tracing) {
		char buf[21];

		snprintf(buf, sizeof buf, "%d", ent_size);
	}

	DIR *dirp = opendir(path);
	if (dirp == (DIR *)0)
		return "opendir(wrap) failed";

	struct dirent *ep;
	errno = 0;
	int ent_count = 0;
	char *err = 0;
	while (err == NULL && (ep = readdir(dirp)) != NULL) {
		if (ent_count > ent_size) {
			err = "too many entries in wrap dir";
			break;
		}
		if (jmscott_frisk_udig(ep->d_name))
			continue;
		ent[ent_count][0] = 0;
		jmscott_strcat(ent[ent_count], PATH_MAX, ep->d_name);
		ent_count++;
	}
	if (closedir(dirp) && err == NULL)
		err = strerror(errno);
	if (err)
		return err;
	if (tracing) {
		char buf[21];

		snprintf(buf, sizeof buf, "%d", ent_count);
		TRACE2("dir entry count", buf);
	}
	return (char *)0;
}
#endif

/*
 *  Remove wrap sets from spool/wrap/ as contained in udig set
 */
static char *
fs_roll(int *ok_no)
{
	(void)ok_no;
	char wrap_dir_path[PATH_MAX];
	char roll_dir_path[PATH_MAX];

	char *err = make_wrap_dir(wrap_dir_path, sizeof wrap_dir_path);
	if (err)
		return err;

	int wrap_dir_fd = jmscott_open(wrap_dir_path, O_RDONLY, 0);
	if (wrap_dir_fd < 0)
		return strerror(errno);

	err = make_roll_dir(roll_dir_path, sizeof roll_dir_path);
	if (err)
		return err;

	if (jmscott_mkdir_p("tmp"))
		return strerror(errno);

	char udig[8 + 128 + 1 + 1];
	udig[0] = 0;
	jmscott_strcat3(udig, sizeof udig, algo, ":", ascii_digest);
		
	char wrap_set[PATH_MAX];
	snprintf(wrap_set, sizeof wrap_set,
		"tmp/wrap-%s.%ld_%d",
		udig,
		time((time_t *)0),
		getpid()
	);
	err = fs_exec_get(udig, wrap_set);
	if (err)
		return err;
	int wrap_fd = jmscott_open(wrap_set, O_RDONLY, 0);
	if (wrap_fd < 0) {
		if (errno == ENOENT)
			return "can not fetch wrap set";
		return strerror(errno);
	}

	/*
	 *  unlink wrap set now, to insure disappears upon process exit.
	 */
	if (unlink(wrap_set) < 0)
		return strerror(errno);

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
