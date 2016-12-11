/*
 *  Synopsis:
 *  	Process to manage file system rename and take requests.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
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
rename_blob(char *reply_path, char *old_path, char *new_path)
{
	char *slash, *p;
	int reply_fd;
	unsigned char reply = 0;
	static char nm[] = "rename_blob";

	reply_fd = io_open(reply_path, O_WRONLY, 0);
	if (reply_fd < 0) {
		error3(nm, "open(reply fifo) failed", strerror(errno));
		reply = 1;
		goto bye;
	}

	/*
	 *  Make the full directory path to the new blob.
	 */
	p = new_path;
	while ((slash = index(p, '/'))) {
		*slash = 0;
		reply = make_path(new_path);
		*slash = '/';
		if (reply)
			goto bye;
		p = slash + 1;
	}

	/*
	 *  Move the blob to the new directory.
	 */
	if (io_rename(old_path, new_path)) {
		int err = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "rename(%s, %s) failed: %s",
					old_path, new_path, strerror(err));
		error2(nm, buf);
		reply = 1;
	}
	
	/*
	 *  Set the blob read only for user/group.
	 */
	if (io_chmod(new_path, S_IRUSR | S_IRGRP)) {
		int err = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "readonly chmod(%s) failed: %s",
					new_path, strerror(err));
		error2(nm, buf);
		reply = 1;
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
		if (io_msg_write(reply_fd, &reply, 1) < 0)
			panic3(nm, "write(reply fifo) failed", strerror(errno));
		if (io_close(reply_fd))
			panic3(nm, "close(reply fifo) failed", strerror(errno));
	}

	if (reply)
		panic2(nm, "aborist process replied with failure");
}

static int
trim_blob_dir(char *dir_path)
{
	char *slash;
	static char nm[] = "trim_blob_dir";
	int count = 0;

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
	goto zap;
}

static void
arborist(int request_fd)
{
	ssize_t status;
	struct io_message request;

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
		info("read from client request pipe of zero bytes");
		info("shutting down arborist");
		leave(0);
	}
	/*
	 *  The client requests two commands: rename or take.
	 *  The 'rename' command moves a file into a directory, possibly
	 *  creating the full directory path.  The 'take' command
	 *  removes all empty directories in a path branch, effectively
	 *  garbage collecting the empty directory entries.
	 *
	 *  The request packets look like:
	 *
	 *  	R[reply fifo][null][tmp blob path ...][null][new path ...][null]
	 *  	T[dir path to blob ...][null]
	 *
	 *  where 'R' means rename and 'T' means take.
	 *
	 *  The for 'R' rename, the arborist replies to the request over the
	 *  sent fifo with a single null byte for success, panic otherwise.
	 *
	 *  For the 'T' take, no reply occurs.
	 */
	if ((char)request.payload[0] == 'R') {
		char *reply_path, *tmp_path, *new_path;
		
		reply_path = (char *)request.payload + 1;
		tmp_path = reply_path + strlen(reply_path) + 1;
		new_path = tmp_path + strlen(tmp_path) + 1;

		rename_blob(reply_path, tmp_path, new_path);
	} else {
		char buf[MSG_SIZE];
		int count;

		count = trim_blob_dir((char *)request.payload + 1);
		snprintf(buf, sizeof buf, "trimmed %d director%s",
					count, count == 1 ? "y" : "ies");
		info(buf);
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

	if (arbor_fd < -1)
		return;
	fd = arbor_fd;
	arbor_fd = -1;
	if (io_close(fd))
		panic3(nm, "close(write pipe) failed", strerror(errno));
}

void
arbor_rename(char *tmp_path, char *new_path)
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
	 *  	R[reply fifo][null][tmp blob path ...][null][new path ...][null]
	 */
	tlen = strlen(tmp_path) + 1;
	nlen = strlen(new_path) + 1;
	len = 1 + rlen + tlen + nlen;
	if (len > MSG_SIZE) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "message length %d > 255",len);
		panic2(nm, buf);
	}

	msg[0] = 'R';
	memcpy(msg + 1, reply_path, rlen);
	memcpy(msg + 1 + rlen, tmp_path, tlen);
	memcpy(msg + 1 + rlen + tlen, new_path, nlen);

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
