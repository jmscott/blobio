/*
 *  Synopsis:
 *	Implement "wrap" and "roll" verbs of blob.io protocol.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 *  Note:
 *	Ought to automatically create the spool/wrap directory.
 */
#include <sys/stat.h>
#include <sys/types.h>

#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include "biod.h"

extern pid_t		logged_pid;
extern unsigned char	request_exit_status;

pid_t brr_logger_pid = 0;

static int	log_fd = -1;
static char 	log_path[] = "spool/biod.brr";
static char	*RFC3339Nano =
"%04d-%02d-%02dT%02d:%02d:%02d.%09ld+00:00\t%s\t%s\t%s%s%s\t%s\t%llu\t%ld.%09ld\n";

/*
 *  Maximum size of a brr record.  
 *
 *  Need enough space for 128 char max digest and 128 char netflow field
 *  (e.g., ip4 tunneled through netflowed ip6, source/dest).
 *
 *  Breakdown of the field sizes are:
 *
 *	Byte Count	Description
 *	--------        --------------
 *	36	+	start time: YYYY-MM-DDTHH:MM:SS.NS9[-+]hh:mm
 *	1	+	<tab>
 *	8+1+128 +	proto8:[[:graph:]{1,128}
 *	1	+	<tab>
 *	8	+	verb: get|put|give|take|eat|wrap|roll ...
 *	1	+	<tab>
 *	8+1+128 +	udig: algorithm:digest
 *	8	+	chat history: ok(,ok){,2}|no|ok,no|ok,ok,no
 *	1	+	<tab>
 *	20	+	signed 64 bit byte >= 0 and <= 9223372036854775807
 *	1	+	<tab>
 *	20		wall duration: sec32.ns9, sec32 >= 0 && ns9 <= 999999999
 */
#define BRR_SIZE		371		//  terminating null NOT counted

/*
 *  Answer a wrap request from a child process.
 *  Move spool/biod.brr to spool/biod-<now>-seq.brr and inform the child
 *  on the fifo at reply_fifo_path.
 *
 *  Note:
 *  	The brr record describing the wrap is NOT guaranted to be in the
 *  	next "wrap", which is problematic.  To fix, the brr record ought
 *  	to be written here or the "wrap" ought to be serialed in the
 *  	forward facing process.
 */
static void
answer_wrap(char *reply_fifo_path)
{
	static u2 seq = 0;

	static char 	log_wrap_format[] = "spool/biod-%010u-%05u.brr";
	char path[MSG_SIZE];
	int fd;
	time_t now;
	static char nm[] = "answer_wrap";

	/*
	 *  Close the active incarnation of spool/biod.brr
	 */
	fd = log_fd;
	log_fd = -1;
	if (io_close(fd))
		panic3(nm, "close(brr) failed", strerror(errno));
	/*
	 *  Move brr log file from spool/biod.brr to spool/biod-<now>-<seq>.brr
	 */
	now = time((time_t *)0);
	if (!now)
		panic3(nm, "time() failed", strerror(errno));
	seq++;
	snprintf(path, sizeof path, log_wrap_format, time, seq);
	if (io_rename(log_path, path)) {
		int e = errno;
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "rename(%s, %s) failed", log_path,
								path);
		panic3(nm, buf, strerror(e));
	}
	if (io_chmod(path, S_IRUSR))
		panic4(nm, path, "chmod(frozen brr) failed", strerror(errno));
	/*
	 *  Reopen the spool/biod.brr log file.  No other process can write
	 *  to biod.brr till this answer_wrap() exits.
	 */
	if ((log_fd = io_open_append(log_path, 0)) < 0)
		panic4(nm, "open(brr) failed", log_path, strerror(errno));

	/*
	 *  Inform the waiting wrap request child by writing the renamed
	 *  brr log file path onto the reply fifo sent to us by the child.
	 */
	fd = io_open(reply_fifo_path, O_WRONLY, 0);
	if (fd < 0)
		panic4(nm, reply_fifo_path, "open(reply fifo) failed",
						strerror(errno));
	if (io_msg_write(fd, path, 32) < 0)
		panic3(nm, "write(reply fifo) failed", strerror(errno));
	if (io_close(fd))
		panic3(nm, "close(reply fifo) failed", strerror(errno));
}

/*
 *  Respond to requests from blob request child process and log results
 *  into spool/biod.brr.
 */
static void
brr_logger(int request_fd)
{
	ssize_t status, nwritten, nread;
	static char nm[] = "brr_logger";
	struct io_message request;

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
		info("shutting down brr logger");
		leave(0);
	}
	nread = request.len;
	/*
	 *  Typically the blob request child sends only a full brr record, which
	 *  is promptly written to spool/biod.brr. 
	 *
	 *  However, a wrap request, can syncronously freeze further writes
	 *  to the spool/biod.brr log file by sending a special record that
	 *  looks like
	 *
	 *  	[null]run/wrap-<pid>.fifo[null]
	 *
	 *  This request means to close spool/biod.brr and rename to
	 *  log/biod-<now>-<seq>.brr, thus freezing the log from updates,
	 *  and the write the path log/biod-<now>-<seq>.brr to the
	 *  run/wrap-<pid>.fifo pipe, so the child process can
	 *  complete the wrap request.
	 *
	 *  Here is the interprocess communication flow
	 *
	 * 	In wrap request process:
	 * 		REPLY_FIFO=run/wrap-<pid>.fifo
	 * 		mkfifo $REPLY_FIFO
	 *
	 * 		#  brr_log_pipe is the write side of the pipe to
	 * 		#  brr file logger
	 *
	 * 		write '<null>$REPLY_FIFO<null>' >>brr_log_pipe[write]
	 *
	 * 		# wait for reply from biod process
	 *		BRR_LOG=$(read BRR_LOG <$REPLY_FIFO)
	 *
	 * 	In brr-logger process (here):
	 *
	 * 		#  read the request from the wrap child request.
	 * 		#  the child sent the path to the fifo for which to
	 * 		#  reply with the path to the wrappable brr file.
	 * 		#  the child wrap finishes the wrap request
	 *
	 * 		REPLY_FIFO=$(read <brr_log_pipe[read])
	 *
	 * 		BRR_LOG=spool/biod-<now>-<seq>.brr
	 *		mv spool/biod.brr $BRR_LOG
	 *		write $BRR_LOG >>$REPLY_FIFO
	 *
	 *	In wrap request process:
	 * 		REPLY_FIFO=run/wrap-pid.fifo
	 *
	 * 		# wait for reply from biod-listen process
	 * 		BRR_LOG=$(read <$REPLY_FIFO)
	 *
	 *		rm $REPLY_FIFO
	 *		UDIG=$(eat $BRR_LOG)
	 *		blobio put $BRR_LOG
	 *		mv $BBR spool/wrap/$UDIG
	 *
	 *		#
	 *		#  blobify the udigs of all the brr log files in
	 *		#  spool/wrap/ directory.  for example, say spool/wrap
	 *		#  contains 2 files,
	 *		#
	 *		#	spool/wrap/sha:b69d2...
	 *		#	spool/wrap/sha:f128d...
	 *		#  
	 *		#  then we build a set, say X, with 2 lines
	 *		#	sha:b69d2...\n
	 *		#	sha:f128d...\n
	 *		#
	 *		#  and then blobify X and send the udig of X to the
	 *		#  client that request the wrap.
	 *		#
	 */
	if (nread == 25) {
		answer_wrap((char *)request.payload);
		goto request;
	}

	/*
	 *  Client request wrote a simple, atomic blob request record, so just
	 *  log that record to file spool/biod.brr.
	 */
	nwritten = io_write(log_fd, request.payload, nread);

	/*
	 *  Failed writes to physical brr log file are very problematic.
	 *  Just close down the physical file, redirect to stderr and abort.
	 */
	if (nwritten < 0)
		panic4(nm, "write(brr) failed", log_path, strerror(errno));
	if (nwritten == 0)
		panic2(nm, "write(brr) empty");
	if (nwritten != nread) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf,
				"write(reply fifo) too %s: %ld %c %ld",
				nwritten > nread ? "big" : "short",
				nwritten,
				nwritten > nread ? '>' : '<',
				nread
		);
		panic2(nm, buf);
	}
	goto request;
}

static void
fork_brr_logger()
{
	int status;
	pid_t pid;
	int brr_pipe[2];
	static char nm[] = "fork_brr_logger";

	status = io_pipe(brr_pipe);
	if (status < 0)
		panic3(nm, "pipe() failed", strerror(errno));
	pid = fork();
	if (pid < 0)
		panic3(nm, "fork() failed", strerror(errno));

	/*
	 *  In the parent biod process, so shutdown the read side of the
	 *  logger pipe and map the write side of the pipe to the log_fd
	 *  descriptor.
	 */
	if (pid > 0) {
		brr_logger_pid = pid;
		io_close(log_fd);
		log_fd = brr_pipe[1];
		if (io_close(brr_pipe[0]))
			panic3(nm, "close(pipe read) failed", strerror(errno));
		return;
	}
	brr_logger_pid = logged_pid = getpid();
	ps_title_set("biod-brr-logger", (char *)0, (char *)0);
	info2(nm, "brr logger process started");

	/*
	 *  Shutdown the write() side of the pipeline.
	 */
	if (io_close(brr_pipe[1]))
		panic3(nm, "close(pipe write) failed", strerror(errno));

	/*
	 *  TERM is ignored so biod can use killpg() to quickly terminate
	 *  request children.  biod-brr-logger shuts down when the read
	 *  side of the pipe returns 0 (EOF).
	 */
	if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
		panic3(nm, "signal(TERM) failed", strerror(errno));

	brr_logger(brr_pipe[0]);
	panic2(nm, "unexpected return from brr_logger()");
}

void
brr_open()
{
	static char nm[] = "brr_open";

	if (log_fd > -1)
		panic2(nm, "brr logger already open");

	/*
	 *  Open the Blob Request Record file for append.
	 */
	if ((log_fd = io_open_append(log_path, 0)) < 0)
		panic4(nm, "open() failed", log_path, strerror(errno));
	fork_brr_logger();
}

void
brr_close()
{
	int fd;
	static char nm[] = "brr_close";

	if (log_fd == -1)
		return;
	fd = log_fd;
	log_fd = -1;
	if (io_close(fd))
		panic3(nm, "close() failed", strerror(errno));

	if (getpid() == brr_logger_pid) {
		info3(nm, "closing brr log file", log_path);
		log_close();
	}
}

/*
 *  Write the blob request record to the brr logger.
 */
void
brr_write(struct request *rp)
{
	char brr[BRR_SIZE + 1];
	long int sec, nsec;
	size_t len;
	struct tm *t;
	static char n[] = "brr_write";

	if (log_fd < 0)
		panic2(n, "brr log file is not open");

	/*
	 *  Build the ascii version of the start time.
	 */
	t = gmtime(&rp->start_time.tv_sec);
	if (!t)
		panic3(n, "gmtime() failed", strerror(errno));
	/*
	 *  Calculate the elapsed time in seconds and
	 *  nano seconds.  The trick of adding 1000000000
	 *  is the same as "borrowing" a one in grade school
	 *  subtraction.
	 */
	if (rp->end_time.tv_nsec - rp->start_time.tv_nsec < 0) {
		sec = rp->end_time.tv_sec - rp->start_time.tv_sec - 1;
		nsec = 1000000000 + rp->end_time.tv_nsec -
							rp->start_time.tv_nsec;
	} else {
		sec = rp->end_time.tv_sec - rp->start_time.tv_sec;
		nsec = rp->end_time.tv_nsec - rp->start_time.tv_nsec;
	}

	/*
	 *  Verify that seconds and nanoseconds are positive values.
	 *  This test is added until I (jmscott) can determine why
	 *  I peridocally see negative times on Linux hosted by VirtualBox
	 *  under Mac OSX.
	 */
	if (sec < 0) {
		char ebuf[128];

		snprintf(ebuf, sizeof ebuf, "negative seconds: %ld", sec);
		error2(n, ebuf);
		warn2(n, "resetting seconds to 0");
		sec = 0;
	}
	if (nsec < 0) {
		char ebuf[128];

		snprintf(ebuf, sizeof ebuf, "negative nano seconds: %ld", nsec);
		error2(n, ebuf);
		warn2(n, "resetting nano seconds to 0");
		nsec = 0;
	}
	/*
	 *  Format the record buffer.
	 */
	snprintf(brr, BRR_SIZE + 1, RFC3339Nano,
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		rp->start_time.tv_nsec,
		rp->netflow,
		rp->verb,
		rp->algorithm && rp->algorithm[0] ? rp->algorithm : "",
		rp->algorithm && rp->algorithm[0] &&
			rp->digest && rp->digest[0] ? ":" : "",
		rp->digest && rp->digest[0] ? rp->digest : "",
		rp->chat_history,
		rp->blob_size,
		sec, nsec
	);

	len = strlen(brr);
	/*
	 *  Write the entire blob request record in a single write().
	 */
	if (io_msg_write(log_fd, brr, len) < 0)
		panic3(n, "write(log) failed", strerror(errno));
}

/*
 *  States while parsing a udig set during a roll.
 */
#define NEW_UDIG	0
#define IN_ALGORITHM	1
#define IN_DIGEST	2
#define PUT_UDIG	3

/*
 *  Parse the blob into a hash set of udigs.
 *  Duplicates are NOT allowed.
 */
static int
blob2udig_set(char *blob, off_t size, void **udig_set, void **algo_set)
{
	char *b, *b_end;
	char *udig_start, *digest_start;
	void *uset, *aset;
	int state;			/* parse state */
	char *sn;			/* parse state name */
	char ebuf[MSG_SIZE];
	int line_count = 1;
	int status;
	static char nm[] = "blob2udig_set";
	static char no_graph_algo[] =
		"non graphic character in algorithm: 0x%x: line #%d";
	static char no_graph_dig[] =
		"non graphic character in digest: 0x%x: line #%d";

	if (size == 0)
		panic2(nm, "impossible empty blob");
	if (blob[size - 1] != '\n') {
		error2(nm, "last character of blob is not newline");
		return -1;
	}
	/*
	 *  Allocate the udig set.
	 */
	blob_set_alloc(&uset);
	/*
	 *  Allocate the algorithm set that is derived from the
	 */
	blob_set_alloc(&aset);
	b = blob;

	/*
	 *  Parse the blob for udigs that match.
	 *
	 *	(algorithm{1,8}:digest{1,255}\n)*
	 */
	state = NEW_UDIG;
	sn = "NEW_UDIG";
	b_end = blob + size;
	while (b < b_end) {
		char c = *b++;

		switch (state) {
		case NEW_UDIG:
			if (c == '\n') {
				snprintf(ebuf, sizeof ebuf,
					"empty udig: line #%d", line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			if (c == ':') {
				snprintf(ebuf, sizeof ebuf,
				      "unexpected colon: line #%d", line_count);
				error3(nm, sn,  ebuf);
				goto croak;
			}
			if (isgraph(c)) {
				udig_start = b - 1;
				state = IN_ALGORITHM;
				sn = "IN_ALGORITHM";
			} else {
				snprintf(ebuf, sizeof ebuf,
				       "non graphic character: 0x%x: line #%d",
				       	c, line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			break;
		case IN_ALGORITHM:
			if (b - udig_start > MAX_ALGORITHM_SIZE) {
				snprintf(ebuf, sizeof ebuf,
						"algorithm too long: line #%d",
						line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			if (c == ':') {
				state = IN_DIGEST;
				digest_start = b + 1;
				sn = "IN_DIGEST";
			} else if (!isgraph(c)) {
				snprintf(ebuf, sizeof ebuf, no_graph_algo,
								c, line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			break;
		case IN_DIGEST:
			if (digest_start - udig_start > MAX_DIGEST_SIZE) {
				snprintf(ebuf, sizeof ebuf,
						"digest too long: line #%d",
						line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			if (c == '\n') {	/* new line terminating udig */
				line_count++;
				*--b = 0;
				state = PUT_UDIG;
				sn = "PUT_UDIG";
			} else if (!isgraph(c)) {
				snprintf(ebuf, sizeof ebuf, no_graph_dig,
								c, line_count);
				error3(nm, sn, ebuf);
				goto croak;
			}
			break;
		case PUT_UDIG:
			/*
			 *  Verify the udig is unique in the set.
			 */
			status = blob_set_put(uset, (unsigned char *)udig_start,
							b - udig_start - 1);
			if (status == 1) {
				*b = 0;
				error3(nm, "duplicate udig in set", udig_start);
				goto croak;
			}

			/*
			 *  Record the algorithm.
			 */
			(void)blob_set_put(aset, (unsigned char *)udig_start,
						(digest_start - udig_start -2));
			state = NEW_UDIG;
			sn = "NEW_UDIG";
			break;
		default:
			panic3(nm, sn, "impossible state");
		}
	}
	if (state != NEW_UDIG)
		panic3(nm, "unexpected state at end of parse",
			state == IN_ALGORITHM ? "IN_ALGORITHM" :
			(state == IN_DIGEST ? "IN_DIGEST" :
			 (state == PUT_UDIG ? "PUT_UDIG" : "UNKNOWN")
			)
		);

	*udig_set = uset;
	*algo_set = aset;
	return 0;
croak:
	status = -1;
	if (uset)
		blob_set_free(uset);
	if (aset)
		blob_set_free(aset);
	return -1;
}

/*
 *  Extract the udig from the file name.
 */
static int
extract_wrap_udig(char *name, char *udig)
{
	int len;
	char *colon;
	char algorithm[MAX_ALGORITHM_SIZE];
	int offset;
	struct digest_module *mp;

	len = strlen(name);
	if (len < 4 || len >= (MAX_UDIG_SIZE + 4) ||
	    strcmp(&name[len - 4], ".brr"))
		return -1;
	colon = strchr(name, ':');
	if (colon == NULL)
		return -1;
	offset = colon - name;
	if (offset > MAX_ALGORITHM_SIZE)
		return -1;
	memcpy(algorithm, name, offset);
	algorithm[offset] = 0;

	mp = module_get(algorithm);
	if (mp == NULL)
		return -1;
	memmove(udig, name, len - 4);
	udig[len - 4] = 0;
	return mp->is_digest(udig + (colon - name) + 1) > 0  ? 0 : -1;
}

/*
 *  Synopsis:
 *	Forget all traffic logs (not blobs) that match a udig in a set.
 *  Protocol Flow:
 *	>roll udig\n			# request to roll set of traffic logs
 *	    <ok\n[close]		# ok, traffic files forgotten
 *	    <no\n[close]		# no, some logs may still exist
 *  Returns:
 *	0	success
 *	-1	error
 */
int
roll(struct request *rp, struct digest_module *mp)
{
	char roll_path[MAX_FILE_PATH_LEN + 1];
	int roll_fd;
	struct stat st;
	char *blob, *b, *b_start, *b_end;
	void *udig_set = 0, *algo_set = 0;
	int roll_file_count;
	DIR *dirp;
	struct dirent *dp;
	int err;
	static char nm[] = "roll";

	request_exit_status = (request_exit_status & 0x1C) |
					(REQUEST_EXIT_STATUS_ROLL << 2);
	roll_path[0] = 0;
	/*
	 *  Open a temporary file to hold the roll list blob sent by the client.
	 */
	snprintf(roll_path, sizeof roll_path, "tmp/roll-%d-%u.log",
					/*
					 *  Note:
					 *	Casting time() to int is
					 *	incorrect!!
					 */
					(int)time((time_t *)0),
					getpid()
	);
	/*
	 *  Open the temporary file to stash the roll blob sent by the client.
	 *  The contents of the blob must be stored on this server.
	 */
	roll_fd = io_open(roll_path, O_CREAT | O_EXCL | O_RDWR,S_IRUSR|S_IWUSR);
	err = errno;

	/*
	 *  Unlink temp file while still open,so file blocks are automatically
	 *  freed when process exists.
	 *
	 *  Note:
	 *	Very unix specfic file system behavior.
	 */
	io_unlink(roll_path);
	if (roll_fd < 0)
		panic4(nm, roll_path, "open(roll set) failed", strerror(err));
	/*
	 *  Write the blob to the temporary file.
	 */
	if ((*mp->write)(rp, roll_fd) == 1) {
		if (write_no(rp)) {
			error3(nm, rp->algorithm, "write_no(client) failed");
			return -1;
		}
		return -1;
	}

	/*
	 *  Seek to begining of roll list.
	 */
	if (io_lseek(roll_fd, (off_t)0, SEEK_SET))
		panic3(nm, "lseek(roll set failed", strerror(errno));
	/*
	 *  How much ram do we need to store the blob in memory?
	 *
	 *  Note: since we've already stored the blob on the file system, then
	 *        why not just reference that blob directly via mmap()?
	 */
	if (io_fstat(roll_fd, &st))
		panic4(nm, roll_path, "fstat() failed", strerror(errno));
	if (st.st_size == 0) {
		warn4(nm, "empty blob", rp->algorithm, rp->digest);
		if (write_ok(rp)) {
			error3(nm, rp->algorithm, "write_ok(client) failed");
			return -1;
		}
		return 0;
	}

	/*
	 *  Allocate enough memory for udig list.
	 *  Need make sure the blob is not too big.
	 */
	blob = (char *)malloc(st.st_size);
	if (blob == NULL) {
		char buf[MSG_SIZE];

		int e = errno;

		snprintf(buf, sizeof buf, "roll set malloc(%lu) failed",
						(unsigned long)st.st_size); 
		panic3(nm, buf, strerror(e));
	}

	/*
	 *  Slurp up the blob into memory.
	 */
	b = b_start = blob;
	b_end = blob + st.st_size;
	while (b < b_end) {
		int nread = io_read(roll_fd, (unsigned char*)b, b_end - b);
		if (nread < 0)
			panic3(nm, "read(roll set) failed", strerror(errno));
		if (nread == 0)
			panic2(nm, "read(roll set) unexpected end of file");
		b += nread;
	}
	if (io_close(roll_fd))
		panic4(nm, roll_path, "close(roll set) failed",strerror(errno));

	/*
	 *  Parse the blob into a udig hash set.
	 */
	err = blob2udig_set(blob, st.st_size, &udig_set, &algo_set);
	free(blob);
	if (err < 0) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "%s: blob not udig set: %s:%s",
						nm, rp->algorithm, rp->digest);
		warn(buf);
		return -1;
	}

	/*
	 *  Scan the spool/wrap directory for brr logs to remove.
	 */
	dirp = io_opendir("spool/wrap");
	if (dirp == NULL)
		panic3(nm, "opendir(spool/wrap) failed", strerror(errno));
	errno = 0;
	roll_file_count = 0;

	/*
	 *  Loop through spool/wrap/ adding udigs to the temporary wrap set.
	 */
	while ((dp = io_readdir(dirp))) {
	 	char *n = dp->d_name;
		char udig[MAX_UDIG_SIZE + 1];

		if (strcmp(".", n) == 0 || strcmp("..", n) == 0)
			continue;
		if (dp->d_type != DT_REG) {
			warn3(nm, "non regular file in spool/wrap", n);
			continue;
		}
		if (extract_wrap_udig(n, udig)) {
			warn3(nm, "file in spool wrap is not log file", n);
			continue;
		}
		if (blob_set_exists(udig_set, (u1 *)udig, strlen(udig))) {
			char path[MAX_FILE_PATH_LEN];

			snprintf(path, sizeof path, "spool/wrap/%s", n);
			/*
			 *  What about verifying the existence of the blob?
			 */
			if (io_unlink(path)) {
				if (errno == ENOENT)
					warn3(nm, "brr file disappeared", n);
				else
					panic4(nm, n, "unlink(roll brr) failed",
							strerror(errno));
			}
			roll_file_count++;
		}
	}
	if (errno && errno != ENOENT)
		panic3(nm, "readdir(spool/wrap) failed", strerror(errno));
	if (io_closedir(dirp))
		panic3(nm, "close(spool/wrap) failed", strerror(errno));

	if (roll_file_count > 0) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf,
			"removed %u brr log file%s in roll set",
			roll_file_count,
			roll_file_count == 1 ? "" : "s"
		);
		info2(nm, buf);
	} else
		info2(nm, "no frozen brr logs in spool/wrap/");
	if (write_ok(rp)) {
		error2(nm, "reply: write_ok() failed");
		return -1;
	}
	return 0;
}

/*
 *  Write a udig to a wrap set.
 */
static void
put_wrap_set(int fd, char *path, char *udig)
{
	int nwrite, nwritten;
	static char nl[] = "\n";
	static char nm[] = "put_wrap_set";
	
	/*
	 *  Write the udig and new-line to the set file.
	 */
	nwrite = strlen(udig);
	nwritten = io_write(fd, udig, nwrite);
	if (nwritten < 0)
		panic4(nm, path, "write(udig) failed", strerror(errno));
	if (nwritten != nwrite) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "expected %d bytes, got %d",
						nwrite, nwritten);
		panic4(nm, path, "unexpected write(udig) length", buf);
	}
	nwritten = io_write(fd, nl, 1);
	if (nwritten < 0)
		panic3(nm, "write(new-line) failed", strerror(errno));
	if (nwritten != 1)
		panic2(nm, "unexpected write(new-line) length");
}

/*
 *  Synopsis:
 *	Freeze spool/biod.brr and wrap <udig>.brr files in spool/wrap/ into blob
 *  Description:
 *	Wrap occurs in two steps. First, move a frozen version of spool/biod.brr
 *	file into the file spool/wrap/<udig>.brr, where <udig> is the digest of
 *	the frozen blobified brr file.
 *
 *		mv spool/biod.brr ->
 *		     spool/wrap/sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f.brr
 *
 *	where, say, sha:8495766f40ca8ac6fcea76b9a11c82d87d2d0f9f is the digest
 *	of the frozen, read only brr log file.
 *
 *	Second, scan the spool/wrap directory to build a set of all the frozen
 *	blobs in spool/wrap and then take the digest of THAT set of udigs.
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
 * 	to the client.
 *  Return:
 *	0	sucess
 *	1	error
 *  Note:
 *	Wraps do NOT appear to be sequential.  The protocol insures
 *	each wrapped brr blob starts with a "wrap" record and no other
 *	wrap exists after the first line.  Can this be guaranteed?
 *
 *	What happens when wrap occurr too quickly?  In particular, how
 *	sensitive to time is the tail algorithm in flowd?  Could
 *	rapidly wrapped logs be missed by flowd?
 */
int
wrap(struct request *rp, struct digest_module *mp)
{
	char fifo_path[25];
	char frozen_path[32];
	struct io_message frozen;

	/* spool/wrap/<udig>.brr */
	char wrap_path[11 + MAX_UDIG_SIZE + 4 + 1];

	char wrap_set_path[MAX_FILE_PATH_LEN + 1];
	int reply_fifo, frozen_fd, wrap_set_fd;
	int status;
	int err;
	char frozen_udig[MAX_UDIG_SIZE + 1];
	char wrap_set_udig[MAX_UDIG_SIZE + 2];		/* new-line in reply */
	static char nm[] = "wrap";
	DIR *dirp;
	struct dirent *dp;
	off_t offset;
	unsigned int wrap_udig_count = 0;
	size_t len;

	request_exit_status = (request_exit_status & 0x1C) |
					(REQUEST_EXIT_STATUS_WRAP << 2);

	/*
	 *  Send a request message to the brr logger process to freeze the
	 *  current brr log file.  The message length of 25 chars indicates
	 *  to the brr logger to freeze the current brr log file.  The contents
	 *  of the message is the fifo path to reply on with the path to the
	 *  frozen brr file.
	 */
	snprintf(fifo_path, sizeof fifo_path, "run/wrap-%010u.fifo", getpid());
	if (io_mkfifo(fifo_path, S_IRUSR | S_IWUSR))
		panic4(nm, fifo_path, "mkfifo() failed", strerror(errno));

	/*
	 *  Send request to the brr logger process.
	 *  The length of exactly 25 chars triggers the brr wrapper process
	 *  to freeze the spool/biod.brr file
	 */
	if (io_msg_write(log_fd, fifo_path, 25) < 0)
		panic3(nm, "write(log) failed", strerror(errno));

	/*
	 *  Wait for reply containing the frozen path.
	 */
	reply_fifo = io_open(fifo_path, O_RDONLY, 0);
	if (reply_fifo < 0) {
		err = errno;
		io_unlink(fifo_path);
		panic4(nm, fifo_path, "open(reply fifo) failed", strerror(err));
	}

	io_msg_new(&frozen, reply_fifo);
	/*
	 *  Listen on the fifo for the brr wrapper process to send us the
	 *  path to the frozen brr log file.
	 */
	status = io_msg_read(&frozen);
	err = errno;
	io_unlink(fifo_path);
	if (status < 0)
		panic3(nm, "read(reply fifo) failed", strerror(err));
	if (status == 0)
		panic2(nm, "empty read(reply fifo)");
	strcpy(frozen_path, (char *)frozen.payload);
	info3(nm, "frozen brr file", frozen_path);

	/*
	 *  Digest the brr file.
	 *  Probably too chatty on the logging.
	 */
	info4(nm, "digesting brr log file", rp->algorithm, frozen_path);
	frozen_fd = io_open(frozen_path, O_RDONLY, 0);
	if (frozen_fd < 0)
		panic4(nm, frozen_path, "open(frozen brr) failed",
							strerror(errno));
	strcpy(frozen_udig, mp->name);
	strcat(frozen_udig, ":");
	/*
	 *  Call the module to generate a digest for the stream of the
	 *  wrapped brr log file.
	 */
	err = (*mp->digest)(rp, frozen_fd, frozen_udig + strlen(frozen_udig),1);
	if (io_close(frozen_fd))
		panic4(nm, frozen_path, "close(frozen brr) failed",
							strerror(errno));
	if (err) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "udig(%s:%s) failed",
						mp->name, frozen_path);
		panic2(nm, buf);
	}
	info3(nm, "udig of frozen brr log", frozen_udig);

	/*
	 *  Rename the frozen brr log file in spool/ to spool/wrap/<udig>.brr
	 */
	strcpy(wrap_path, "spool/wrap/");
	strcat(wrap_path, frozen_udig);
	strcat(wrap_path, ".brr");
	if (io_rename(frozen_path, wrap_path)) {
		char buf[MSG_SIZE];
		int err;

		err = errno;

		snprintf(buf, sizeof buf,
			"rename(%s, %s) failed", frozen_path, wrap_path);
		panic3(nm, buf, strerror(err));
	}

	/*
	 *  Build a temporary file of udigs in spool/wrap and digest the file.
	 *  Send the udig of the blob set to the client.
	 */
	snprintf(wrap_set_path, sizeof wrap_set_path,
				"tmp/wrap-%ld-%u.udig",
				time((time_t *)0), getpid()
	);
	wrap_set_fd = io_open(wrap_set_path, O_RDWR | O_CREAT,S_IRUSR|S_IWUSR);
	err = errno;

	io_unlink(wrap_set_path);
	if (wrap_set_fd < 0)
		panic4(nm, wrap_set_path,"open(wrap set) failed",strerror(err));

	/*
	 *  Write the freshly frozen udig set first.
	 */
	put_wrap_set(wrap_set_fd, wrap_set_path, frozen_udig);
	wrap_udig_count++;

	dirp = io_opendir("spool/wrap");
	if (dirp == NULL)
		panic3(nm, "opendir(spool/wrap) failed", strerror(errno));
	errno = 0;

	/*
	 *  Loop through spool/wrap/ adding udigs to the temporary wrap set.
	 */
	while ((dp = io_readdir(dirp))) {
	 	char *n = dp->d_name;
		char udig[MAX_UDIG_SIZE];

		if (strcmp(".", n) == 0 || strcmp("..", n) == 0)
			continue;
		if (dp->d_type != DT_REG) {
			warn3(nm, "non regular file in spool/wrap", n);
			continue;
		}
		/*
		 *  Attempt to extract the udig from the scanned file name.
		 *  ought to see Only file names matching
		 *
		 *	<udig>.brr
		 * 
		 *  otherwise, grumble and move on.
		 */
		if (extract_wrap_udig(n, udig)) {
			warn3(nm, "spool/wrap: file does not match <udig>.brr",
									n);
			continue;
		}
		if (strcmp(udig, frozen_udig) == 0)
			continue;
		put_wrap_set(wrap_set_fd, wrap_set_path, udig);
		wrap_udig_count++;
	}
	if (errno)
		panic3(nm, "readdir(spool/wrap) failed", strerror(errno));
	if (io_closedir(dirp))
		panic3(nm, "close(spool/wrap) failed", strerror(errno));

	if (wrap_udig_count > 0) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf,
			"%u udig%s in wrap set of frozen brr logs",
			wrap_udig_count,
			wrap_udig_count == 1 ? "" : "s"
		);
		info2(nm, buf);
	} else
		info2(nm, "no frozen brr logs in spool/wrap/");

	/*
	 *  Rewind to start of file for digest routine.
	 */
	offset = io_lseek(wrap_set_fd, (off_t)0, SEEK_SET);
	if (offset < 0)
		panic4(nm, wrap_set_path, "lseek(wrap set) failed",
							strerror(errno));
	if (offset != 0)
		panic3(nm, wrap_set_path, "lseek(wrap set) != 0");

	strcpy(wrap_set_udig, mp->name);
	strcat(wrap_set_udig, ":");

	/*
	 *  Digest the temporary wrapped set of udigs.
	 */
	err = (*mp->digest)(rp, wrap_set_fd,
				wrap_set_udig + strlen(wrap_set_udig), 1);
	if (io_close(wrap_set_fd))
		panic4(nm, wrap_set_path, "close(wrap set) failed",
							strerror(errno));
	if (err) {
		char buf[MSG_SIZE];

		snprintf(buf, sizeof buf, "wrap set(%s:%s) failed",
						mp->name, wrap_set_path);
		panic2(nm, buf);
	}
	info3(nm, "udig of wrapped set", wrap_set_udig);
	if (write_ok(rp)) {
		error2(nm, "reply: write_ok() failed");
		return -1;
	}
	rp->digest = strdup(strchr(wrap_set_udig, ':') + 1);
	strcat(wrap_set_udig, "\n");
	len = strlen(wrap_set_udig);
	if (write_blob(rp, (unsigned char *)wrap_set_udig, len)) {
		error2(nm, "reply: write_blob(wrap set udig) failed");
		return -1;
	}
	return 0;
}
