/*
 *  Synopsis:
 *  	Process to manage file system rename and take requests.
 *
 *	The arborist stops searching at a symbolic link in the path.
 *	Ought to follow the symbolic links.
 *  Note:
 *	Need to document why move() and rename() are not combined into
 *	single call.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include "biod.h"

extern pid_t	logged_pid;

pid_t		arborist_pid = 0;

static int	arbor_fd = -1;

static int
make_path(char *path)
{
	//  group can search but not scan directory 

	if (io_mkdir(path, S_IRWXU | S_IXGRP)) {
		char buf[MSG_SIZE];
		int err = errno;

		if (err == EEXIST)
			return 0;
		snprintf(buf, sizeof buf, "make_path: mkdir(%s) failed: %s",
							path, strerror(err));
		error(buf);
		return 1;
	}
	return 0;
}

static void
rename_blob(char *reply_path, char *src_path, char *tgt_path)
{
	char *slash, *p;
	int reply_fd;
	unsigned char reply_payload = 0;
	static char nm[] = "rename_blob";

	reply_fd = io_open(reply_path, O_WRONLY, 0);
	if (reply_fd < 0) {
		error3(nm, "open(reply fifo) failed", strerror(errno));
		reply_payload = 1;
		goto bye;
	}

	/*
	 *  Make the full directory path to the target blob.
	 */
	p = tgt_path;
	while ((slash = index(p, '/'))) {
		*slash = 0;
		reply_payload = make_path(tgt_path);
		*slash = '/';
		if (reply_payload)
			goto bye;
		p = slash + 1;
	}

	/*
	 *  Move the blob to the target directory.
	 */
	if (io_rename(src_path, tgt_path)) {
		int err = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "rename(%s, %s) failed: %s",
					src_path, tgt_path, strerror(err));
		error2(nm, buf);
		reply_payload = 1;
		goto bye;
	}
	
	/*
	 *  Set the blob read only for user/group.
	 */
	if (io_chmod(tgt_path, S_IRUSR | S_IRGRP)) {
		int err = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "readonly chmod(%s) failed: %s",
					tgt_path, strerror(err));
		error2(nm, buf);
		reply_payload = 1;
	}

bye:
	/*
	 *  Always reply to requesting process so they can properly reply with
	 *  an error to the client.  Note, we have a race condition because the
	 *  panic() implies that the master biod process may simply shutdown the
	 *  request process before the request has a chance to reply to the
	 *  client.  That's ok, since the client still never gets ok, implying
	 *  the blob was not put properly.
	 */
	if (reply_fd >= 0) {
		if (io_msg_write(reply_fd, &reply_payload, 1) < 0)
			panic3(nm, "write(reply fifo) failed", strerror(errno));
		if (io_close(reply_fd))
			panic3(nm, "close(reply fifo) failed", strerror(errno));
	}

	//  no-zero reply indicates an error
	if (reply_payload)
		panic2(nm, "aborist process replied with failure");
}

/*
 *  Do unix "mv" command.
 */
int
_move(char *src_path, char *tgt_path)
{
	static char n[] = "_move";
	struct stat src_st, tgt_st;
	int src_fd = -1, tgt_fd = -1;
	char buf[MSG_SIZE], *slash;
	int nr;
	int ret = 0;

	if (io_lstat(src_path, &src_st)) {
		error4(n, "lstat(source path) failed",strerror(errno),src_path);
		goto croak;
	}

	//  file must be regular

	if (!S_ISREG(src_st.st_mode)) {
		error3(n, "source not a regular file", src_path);
		goto croak;
	}

	//  verify the target directory live on same device as source

	strncpy(buf, tgt_path, sizeof buf - 1);
	slash = rindex(buf, '/');
	if (slash == NULL) {
		error3(n, "no slash in target path", tgt_path);
		goto croak;
	}
	*slash = 0;

	if (io_lstat(buf, &tgt_st)) {
		error4(n, "lstat(target dir) failed", strerror(errno), buf);
		goto croak;
	}
	if (!S_ISDIR(tgt_st.st_mode)) {
		error3(n, "component in target not a directory", tgt_path);
		goto croak;
	}
	
	/*
	 *  Files on same device, so just rename.
	 */
	if (src_st.st_dev == tgt_st.st_dev) {
		int err;

		if (io_rename(src_path, tgt_path) == 0) {
			src_path[0] = 0;
			goto bye;
		}

		err = errno;
		error3(n, "source path", src_path);
		error3(n, "target path", tgt_path);
		error3(n, "rename(source, target) failed", strerror(err));

		goto croak;
	}

	/*
	 *  Files on different device, so copy source to target.
	 *
	 *  Note:
	 *	Use splice() under linux!
	 */
	src_fd = io_open(src_path, O_RDONLY, 0);
	if (src_fd < 0) {
		error4(n, "open(source) failed", strerror(errno), src_path);
		goto croak;
	}

	tgt_fd = io_open_append(tgt_path, 0);
	if (tgt_fd < 0) {
		error4(n, "open(target) failed", strerror(errno), tgt_path);
		goto croak;
	}

	/*
	 *  Copy the source file to the target file.
	 */

	while ((nr = io_read(src_fd, buf, sizeof buf)) > 0)
		if (io_write_buf(tgt_fd, buf, nr) < 0) {
			error4(n, "write_buf(target) failed", strerror(errno),
								tgt_path);
			goto croak;
		}
	if (nr == 0)
		goto bye;
	error4(n, "read(source) failed", strerror(errno), src_path);

croak:
	ret = -1;
bye:
	if (src_fd > -1 && io_close(src_fd)) {
		error4(n, "close(source) failed", strerror(errno), src_path);
		ret = -1;
	}
	if (tgt_fd > -1 && io_close(tgt_fd)) {
		error4(n, "close(target) failed", strerror(errno), tgt_path);
		ret = -1;
	}
	if (ret == 0 && src_path[0] && io_unlink(src_path)) {
		error4(n, "unlink(source) failed", strerror(errno), src_path);
		ret = -1;
	}

	return ret;
}

static void
move_blob(char *reply_path, char *src_path, char *tgt_path)
{
	char *slash, *p;
	int reply_fd;
	unsigned char reply_payload = 0;
	static char nm[] = "move_blob";

	reply_fd = io_open(reply_path, O_WRONLY, 0);
	if (reply_fd < 0) {
		error3(nm, "open(reply fifo) failed", strerror(errno));
		reply_payload = 1;
		goto bye;
	}

	/*
	 *  Make the full directory path to the target blob.
	 */
	p = tgt_path;
	while ((slash = index(p, '/'))) {
		*slash = 0;
		reply_payload = make_path(tgt_path);
		*slash = '/';
		if (reply_payload)
			goto bye;
		p = slash + 1;
	}

	//  move the blob file, perhaps doing a copy across file systems.

	if (_move(src_path, tgt_path)) {
		reply_payload = 1;
		goto bye;
	}

	/*
	 *  Set the blob read only for user/group.
	 */
	if (io_chmod(tgt_path, S_IRUSR | S_IRGRP)) {
		int err = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "readonly chmod(%s) failed: %s",
					tgt_path, strerror(err));
		error2(nm, buf);
		reply_payload = 1;
	}

bye:
	/*
	 *  Always reply to requesting process so they can properly reply with
	 *  an error to the client.  Note, we have a race condition because the
	 *  panic() implies that the master biod process may simply shutdown the
	 *  request process before the request has a chance to reply to the
	 *  client.  That's ok, since the client still never gets ok, implying
	 *  the blob was not put properly.
	 */
	if (reply_fd >= 0) {
		if (io_msg_write(reply_fd, &reply_payload, 1) < 0)
			panic3(nm, "write(reply fifo) failed", strerror(errno));
		if (io_close(reply_fd))
			panic3(nm, "close(reply fifo) failed", strerror(errno));
	}

	if (reply_payload)	//  non zero reply payload indicates error
		panic2(nm, "aborist process replied with failure");
}

static int
trim_blob_dir(char *dir_path)
{
	char *slash;
	static char nm[] = "trim_blob_dir";
	int count = 0;
	struct stat st;

	/*
	 *   Start at the deepest directory.
	 */
zap:
	if (io_rmdir(dir_path)) {
		if (errno == ENOTEMPTY)
			return count;
		if (errno != ENOENT) {
			char buf[MSG_SIZE];

			snprintf(buf, sizeof buf, "rmdir(%s) failed: %s",
						dir_path, strerror(errno));
			panic2(nm, buf);
		}
	} else
		count++;
	slash = rindex(dir_path, '/');		//  path must be relative
	if (slash == (char *)0)
		return count;
	*slash = 0;

	//  is the target directory a true directory.
	if (io_lstat(dir_path, &st) != 0)
		panic4(nm, "stat() failed", strerror(errno), dir_path);

	/*
	 *  Stop searching at symbolic link
	 *
	 *  Note:
	 *	This is broken.  Ought to follow the symbolic link.
	 */
	if (S_ISLNK(st.st_mode))
		return count;
	goto zap;
}

static void
arborist(int request_fd)
{
	ssize_t status;
	struct io_message request;
	char c;

	static char nm[] = "arborist";

	/*
	 *  Loop reading from child processes wishing to log blob request
	 *  records. We depend upon atomic write/reads on pipes.
	 */
	io_msg_new(&request, request_fd);

request:
	status = io_msg_read(&request);
	if (status < 0)
		panic3(nm, "io_msg_read(request) failed", strerror(errno));
	if (status == 0) {
		char ebuf[MSG_SIZE];

		info("read from client request pipe of zero bytes");
		snprintf(ebuf, sizeof ebuf,"parent process id: #%d",getppid());
		info(ebuf);
		info("shutting down arborist");
		leave(0);
	}
	/*
	 *  The client requests three commands: rename, move or take.
	 *
	 *  The 'rename' command renames a file into a directory, possibly
	 *  creating the full directory path.  The source file MUST exist
	 *  on the same volume as the target file.
	 *
	 *  The 'move' command renames or copies a file into a directory,
	 *  possibly creating the full directory path.  The source file MUST
	 *  exist on the same volume as the target file.
	 *
	 *  The 'trim' command removes the blob and all empty directories in a
	 *  path branch to the blob, effectively garbage collecting the empty
	 *  directory entries.
	 *
	 *  The request packets look like:
	 *
	 *  	R[reply fifo][null][src blob path ...][null][tgt path ...][null]
	 *  	M[reply fifo][null][src blob path ...][null][tgt path ...][null]
	 *  	T[dir path to blob ...][null]
	 *
	 *  where 'R' means rename, 'M' means move and 'T' means trim.
	 *
	 *  The for 'R' and 'M', the arborist replies to the request over the
	 *  sent fifo with a single null byte for success, panic otherwise.
	 *
	 *  For the 'T' trim, no reply occurs.
	 */
	c = (char)request.payload[0];
	if (c == 'T') {
		char buf[MSG_SIZE];
		int count;

		count = trim_blob_dir((char *)request.payload + 1);
		snprintf(buf, sizeof buf, "trimmed %d director%s",
					count, count == 1 ? "y" : "ies");
		info(buf);
	} else {
		char *reply_path, *tmp_path, *tgt_path;
		
		reply_path = (char *)request.payload + 1;
		tmp_path = reply_path + strlen(reply_path) + 1;
		tgt_path = tmp_path + strlen(tmp_path) + 1;

		if (c == 'R')
			rename_blob(reply_path, tmp_path, tgt_path);
		else
			move_blob(reply_path, tmp_path, tgt_path);
	}
	goto request;
}

static void
fork_arborist()
{
	int status;
	pid_t pid;
	int arbor_pipe[2];
	static char nm[] = "fork_arborist";

	status = io_pipe(arbor_pipe);
	if (status < 0)
		panic3(nm, "pipe() failed", strerror(errno));
	pid = fork();
	if (pid < 0)
		panic3(nm, "fork() failed", strerror(errno));

	/*
	 *  In the parent biod process, so shutdown the read side of the
	 *  arborist pipe and map the write side of the pipe to the arbor
	 *  descriptor.
	 */
	if (pid > 0) {
		arborist_pid = pid;
		if (io_close(arbor_pipe[0]))
			panic3(nm, "close(pipe read) failed", strerror(errno));
		arbor_fd = arbor_pipe[1];
		return;
	}

	/*
	 *  In the arborist process
	 */
	logged_pid = arborist_pid = getpid();
	ps_title_set("biod-arborist", (char *)0, (char *)0);
	info2(nm, "arborist process started");

	/*
	 *  Shutdown the write() side of the pipeline.
	 *  Replies go
	 */
	if (io_close(arbor_pipe[1]))
		panic3(nm, "close(pipe write) failed", strerror(errno));

	/*
	 *  TERM is ignored so biod can use killpg() to quickly terminate
	 *  request children.  arbor shuts down when the read side of the pipe
	 *  returns 0 (EOF).
	 */
	if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
		panic3(nm, "signal(TERM) failed", strerror(errno));

	arborist(arbor_pipe[0]);
	panic2(nm, "unexpected return from arborist()");
}

void
arbor_open()
{
	static char nm[] = "arbor_open";

	if (arborist_pid > 0)
		panic2(nm, "arborist process already open");
	fork_arborist();
}

void
arbor_close()
{
	int fd;
	static char nm[] = "arbor_close";

	if (arbor_fd < 0)
		return;
	fd = arbor_fd;
	arbor_fd = -1;
	if (io_close(fd))
		panic3(nm, "close(write pipe) failed", strerror(errno));
}

/*
 *  Client invocation to rename a file blob.
 *
 *  Note:
 *	WTF: the reply from the client is ignored!
 *
 *	Fifo is not always removed!
 */
void
arbor_rename(char *tmp_path, char *tgt_path)
{
	char msg[MSG_SIZE];
	unsigned len, rlen, tlen, nlen;
	int reply_fd;
	char reply_path[32];
	int status, err;
	struct io_message reply;
	static char nm[] = "arbor_rename";

	snprintf(reply_path, sizeof reply_path,"run/arborist-%u.fifo",getpid());
	rlen = strlen(reply_path) + 1;

	/*
	 *  The request packet looks like:
	 *
	 *  	R[reply fifo][null][tmp blob path ...][null][tgt path ...][null]
	 */
	tlen = strlen(tmp_path) + 1;
	nlen = strlen(tgt_path) + 1;
	len = 1 + rlen + tlen + nlen;
	if (len > MSG_SIZE) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "message length %d > 255",len);
		panic2(nm, buf);
	}

	msg[0] = 'R';
	memcpy(msg + 1, reply_path, rlen);
	memcpy(msg + 1 + rlen, tmp_path, tlen);
	memcpy(msg + 1 + rlen + tlen, tgt_path, nlen);

	/*
	 *  Create the reply fifo.
	 */
	snprintf(reply_path, sizeof reply_path,"run/arborist-%u.fifo",getpid());
	if (io_mkfifo(reply_path, S_IRUSR | S_IWUSR) < 0)
		panic4(nm, reply_path, "mkfifo(reply) failed", strerror(errno));

	/*
	 *  Send the rename request to the arborist process and wait for
	 *  response.
	 */
	if (io_msg_write(arbor_fd, (unsigned char *)msg, len)) {
		err = errno;

		io_unlink(reply_path);
		panic3(nm, "msg_write(request) failed", strerror(err));
	}

	reply_fd = io_open(reply_path, O_RDONLY, 0);
	if (reply_fd < 0) {
		err = errno;

		io_unlink(reply_path);
		panic4(nm, reply_path, "open(reply fifo) failed", strerror(err));
	}

	io_msg_new(&reply, reply_fd);
	status = io_msg_read(&reply);
	err = errno;
	io_close(reply_fd);
	io_unlink(reply_path);

	if (status < 0)
		panic3(nm, "msg_read(reply) failed", strerror(err));
	if (status == 0)
		panic2(nm, "unexpected msg_read(reply) of 0 from reply fifo");
}

/*
 *  Client invocation to move a file blob.
 *
 *  Note:
 *	Fifo is not always removed!
 */
void
arbor_move(char *tmp_path, char *tgt_path)
{
	char msg[MSG_SIZE];
	unsigned len, rlen, tlen, nlen;
	int reply_fd;
	char reply_path[32];
	int status, err;
	struct io_message reply;
	static char nm[] = "arbor_move";

	snprintf(reply_path, sizeof reply_path,"run/arborist-%u.fifo",getpid());
	rlen = strlen(reply_path) + 1;

	/*
	 *  The request packet looks like:
	 *
	 *  	M[reply fifo][null][tmp blob path ...][null][tgt path ...][null]
	 */
	tlen = strlen(tmp_path) + 1;
	nlen = strlen(tgt_path) + 1;
	len = 1 + rlen + tlen + nlen;
	if (len > MSG_SIZE) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "message length %d > 255",len);
		panic2(nm, buf);
	}

	msg[0] = 'M';
	memcpy(msg + 1, reply_path, rlen);
	memcpy(msg + 1 + rlen, tmp_path, tlen);
	memcpy(msg + 1 + rlen + tlen, tgt_path, nlen);

	/*
	 *  Create the reply fifo.
	 */
	snprintf(reply_path, sizeof reply_path,"run/arborist-%u.fifo",getpid());
	if (io_mkfifo(reply_path, S_IRUSR | S_IWUSR) < 0)
		panic4(nm, reply_path, "mkfifo(reply) failed", strerror(errno));

	/*
	 *  Send the rename request to the arborist process and wait for
	 *  response.
	 */
	if (io_msg_write(arbor_fd, (unsigned char *)msg, len)) {
		err = errno;

		io_unlink(reply_path);
		panic3(nm, "msg_write() failed", strerror(err));
	}

	reply_fd = io_open(reply_path, O_RDONLY, 0);
	if (reply_fd < 0) {
		err = errno;

		io_unlink(reply_path);
		panic4(nm, reply_path, "open(reply fifo) failed",strerror(err));
	}

	io_msg_new(&reply, reply_fd);
	status = io_msg_read(&reply);
	err = errno;
	io_close(reply_fd);
	io_unlink(reply_path);

	if (status < 0)
		panic3(nm, "msg_read(reply) failed", strerror(err));
	if (status == 0)
		panic2(nm, "unexpected msg_read() of 0 from reply fifo");
}

void
arbor_trim(char *dir_path)
{
	char msg[MSG_SIZE];

	snprintf(msg, sizeof msg, "T%s", dir_path);

	if (io_msg_write(arbor_fd, msg, strlen(msg) + 1))
		panic2("arbor_trim: msg_write(arborist) failed",
							strerror(errno));
}
