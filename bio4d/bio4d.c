/*
 *  Synopsis:
 *	A reference blobio server that speaks the blob.io protocol
 *  Usage:
 *	export BLOBIO_ROOT=/usr/local/blobio
 *	$BLOBIO_ROOT/sbin/bio4d
 *  Note:
 *	Is reap_request() needed to be called after each socket accept()?
 *
 *	The entire start/stop code in bio4d needs a full shakedown.
 *
 *	A broken get request still generates a partial and incorrect brr
 *	record.  For example, see get requests for (or any big blob)
 *
 *		bc160:22572c9bf76e5bd879d5ce800ba6889d50e62ff7
 *
 *	Document command line options in Usage:
 *
 *	Should a "wrap" never return an empty udig?
 *
 *	Bio4d probably panics on a network flap.
 *
 *	Signal handling needs to be pushed to main listen loop or cleaned
 *	up with sigaction().
 *
 *	Need to enumerate all the environment variables upon boot up.
 *
 *	Taking the empty blob ought to fail.
 */
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>

/*
 *  __USE_GNU defined strsignal() for linux.  Ugly.
 */
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "bio4d.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

#define BIO4D_PORT		1797

#define MAX_VERB_SIZE		5
#define DEFAULT_WRAP_DIGEST_ALGORITHM	"bc160"

/*
 *  States of lexical parser for incoming client requests.
 */
#define STATE_HALT		0
#define STATE_SCAN_VERB		1
#define STATE_SCAN_ALGORITHM	2
#define STATE_SCAN_NEW_LINE	3
#define STATE_SCAN_DIGEST	4
#define STATE_MAX		5

#define STATE_go							\
{									\
	*d_next++ = 0;							\
	bio4d(verb, algorithm, digest, b, b_end - b);			\
	state = STATE_HALT;						\
}
	
#define LOG_HEARTBEAT		10	/* update log at least 10 seconds */
#define PID_HEARTBEAT		60	/* update times of run/bio4d.pid */

/*
 *  Note:
 *	Should ACCEPT_TIMEOUT default to same as READ?WRITE?
 */
#define ACCEPT_TIMEOUT		3	/* 3 seconds */
#define NET_TIMEOUT		20	/* 20 seconds for net read/write */
#define LEAVE_PAUSE		2	/* pause seconds before exiting */

extern pid_t	brr_logger_pid;
extern pid_t	logged_pid;
extern pid_t	arborist_pid;

/*
 *  Return exit status of request child process.
 *  State bits are ORed in as request proceeds.
 */
time_t recent_log_heartbeat		= 0;
time_t recent_pid_heartbeat		= 0;
time_t rrd_sample_prev			= 0;

void		**module_boot_data = 0;
pid_t		request_pid = 0;
pid_t		master_pid = 0;
pid_t		logger_pid = 0;
int		leaving = 0;
unsigned char	request_exit_status = 0;
time_t		start_time;	
u2		rrd_sample_duration = 0;

char	pid_path[] = "run/bio4d.pid";

static char		*BLOBIO_ROOT = 0;

static char		wrap_digest_algorithm[MAX_ALGORITHM_SIZE+1];

static int	net_timeout = -1;

static struct request	req =
{
	.client_fd	=	-1,
	.verb		=	0,
	.step		=	0,
	.algorithm	=	0,
	.digest		=	0,
	.scan_buf	=	0,
	.scan_size	=	0,
	.open_data	=	0,
	.blob_size	=	0,
	.read_timeout	=	NET_TIMEOUT,
	.write_timeout	=	NET_TIMEOUT
};

static int		listen_fd = -1;

/*
 *  Inbound Connections:
 *	connect_count ==
 *		success_count +
 *		error_count +
 *		timeout_count +
 *		signal_count +
 *		fault_count
 */
static u8	connect_count = 0;	//  socket connections answered

/*
 *  Request summaries
 */
static u8	success_count =	0;	//  exit ok
static u8	error_count =	0;	//  error talking with client
static u8	timeout_count =	0;	//  timeout read()/write() with client
static u8	signal_count =	0;	//  terminated with signal
static u8	fault_count =	0;	//  faulted (panic in request)

/*
 *  Request Verbs
 */
static u8	get_count =	0;
static u8	put_count =	0;
static u8	give_count =	0;
static u8	take_count =	0;
static u8	eat_count =	0;
static u8	wrap_count =	0;
static u8	roll_count =	0;

/*
 *  Request chat summaries.
 */
static u8	chat_ok_count = 0;	//  ok OR ok,ok, OR ok,ok,ok
static u8	chat_no_count = 0;	//  no
static u8	chat_no2_count =0;	//  ok,no
static u8	chat_no3_count =0;	//  ok,ok,no
static u8	eat_no_count =	0;	//  no occured on eat
static u8	take_no_count =	0;	//  single no on take

static char	rrd_log[] = "log/bio4d-rrd.log";

static unsigned char	in_foreground = 0;

/*
 *  Exit cleanly, shutting down the logger.
 *
 *  Note:
 *  	Do the sockets need a shutdown() before a close().
 */
void
leave(int exit_status)
{
	pid_t my_pid;

	if (leaving)
		return;

	leaving = 1;		/* hush reaper complaints about exiting */

	signal(SIGALRM, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	my_pid = getpid();

	/*
	 *  Shutdown client socket associated with this request.
	 */
	if (req.client_fd > -1) {
		io_close(req.client_fd);
		req.client_fd = -1;
	}

	/*
	 *  Shutdown server listen socket.
	 *
	 *  Note:
	 *	Need to do shutdown()?
	 */
	if (listen_fd > -1) {
		io_close(listen_fd);
		listen_fd = -1;
	}
	if (my_pid != master_pid)
		exit(exit_status);

	/*
	 *  In the master, so shutdown all children, the logger last.
	 */
	char buf[MSG_SIZE];

	info("request to shutdown");

	info("shutting down the arborist");
	arbor_close();

	info("shutting down brr logger");
	brr_close();

	info("shutting down digest modules");
	module_leave();

	info("sending TERM signal to request children");
	killpg(getpgrp(), SIGTERM);
	
	info2("removing process id file", pid_path);
	if (io_unlink(pid_path)) {
		snprintf(buf, sizeof buf,
			"PANIC: unlink(%s) failed: %s",
				pid_path, strerror(errno));
		error(buf);
		error("this server may not restart");
	}

	snprintf(buf, sizeof buf, "sleeping %d seconds while kids exit",
					LEAVE_PAUSE);
	info(buf);

	/*
	 *  Sleep awhile as kids shutdown.
	 *
	 *  Note:
	 *  	Need to waitpid() for logger process to shutdown.
	 */
	sleep(LEAVE_PAUSE);
	info("good bye, cruel world");
	log_close();
	exit(exit_status);
}

/*
 *  Write a panicy message to log and then exit.
 */
static void
die(char *msg)
{
	/*
	 *  If no logger process then we are probably still booting.
	 */
	if (logged_pid == 0) {
		static char ERROR[] = "\nbio4d: ERROR: ";

		write(2, ERROR, sizeof ERROR - 1); 
		write(2, msg, strlen(msg));
		write(2, "\n", 1);
		exit(1);
	}
	/*
	 *  Dieing in the master listen process is a panicy error.
	 *  Dieing in the child is just an error.
	 */
	if (master_pid)
		panic(msg);

	error(msg);		//  in a child process

	/*
	 *  Reach this statement implies a fault.
	 */
	if (request_pid && (request_exit_status & 0x3) == 0)
		request_exit_status = (request_exit_status & 0xFC) | 0x1;
	leave(request_exit_status);
}

static void
die2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	die(log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
die3(char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	die(log_strcpy3(buf, sizeof buf, msg1, msg2, msg3));
}

/*
 *  Send a NO to client and then die.
 */
static void
die_NO(char *msg)
{
	static char NO[] = "no\n";

	if (req.client_fd > -1)
		if (write(req.client_fd, NO, sizeof NO - 1) < 0)
			error2(req.transport, "write(no) failed");
	die2(req.transport, msg);
}

static void
die2_NO(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	die_NO(log_strcpy2(buf, sizeof buf, msg1, msg2));
}

static void
die3_NO(char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	die_NO(log_strcpy3(buf, sizeof buf, msg1, msg2, msg3));
}

/*
 *  Synopsis:
 *	Fire digest module callbacks for "get" verb.
 *  Protocol Flow:
 *	get algorithm:digest\n		# request for blob
 *	    <ok\n[bytes][close]		# server sends blob, bye
 *          <no\n[close]		# server rejects blob, bye
 */
static int
get(struct request *rp, struct digest_module *mp)
{
	request_exit_status = (request_exit_status & 0x1C) |
					(REQUEST_EXIT_STATUS_GET << 2);

	rp->step = "request";
	if ((*mp->get_request)(rp))
		return write_no(rp);
	if (write_ok(rp))
		return -1;

	rp->step = "bytes";
	return (*mp->get_bytes)(rp);
}

/*
 *  Synopsis:
 *	Fire digest module callbacks for "eat" verb.
 *  Protocol Flow:
 *	eat algorithm:digest\n		# request for to eat local blob
 *	    <ok\n[close]		# ok, blob digest verified locally
 *          <no\n[close]		# no blob with signature
 */
static int
eat(struct request *rp, struct digest_module *mp)
{
	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_EAT << 2);
	if ((*mp->eat)(rp))
		return write_no(rp);
	return write_ok(rp);
}

/*
 *  Synopsis:
 *	Execute the put request by firing the callbacks of the digest module.
 *  Protocol Flow:
 *	>put udig\n	# request from client to put blob matching digest
 *	  <ok\n		#   server ready for bytes matching bytes
 *	    >[bytes]	#     blob bytes sent from client
 *	    <ok\n	#       accepted bytes for blob
 *	    <no\n	#       rejects bytes for blob
 *	<no\n		#   server rejects request to put blob
 */
static int
put(struct request *rp, struct digest_module *mp)
{
	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_PUT << 2);

	rp->step = "request";
	if ((*mp->put_request)(rp))
		return write_no(rp);
	if (write_ok(rp))
		return -1;

	rp->step = "bytes";
	if ((*mp->put_bytes)(rp))
		return write_no(rp);
	return write_ok(rp);
}

/*
 *  Synopsis:
 *	Fire module callbacks for "take" verb in blobio protocol.
 *  Protocol Flow:
 *	>take algorithm:digest\n	# request for blob
 *	    <ok\n[blob]			# server sends blob
 *	    <no\n[close]		# server rejects take, bye
 *		>ok\n			# client accepted blob, ok to forget
 *		>no[close]		# client rejects blob, bye
 *			<ok[close]	# we probably forget blob, bye
 *			<no[close]	# we will keep the blob, bye
 *  Note:
 *	Taking the empty blob ought to fail.
 */
static int
take(struct request *rp, struct digest_module *mp)
{
	char *reply;

	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_TAKE << 2);
	rp->step = "is_wrap";
	/*
	 *  Verify the blob is not an element of the current unrolled wrap
	 *  set in the file
	 *
	 *		spool/wrap/algorithm:digest.brr.
	 *
	 *  In other words, we can't take a blob that has not been rolled;
	 *  such a blob can only be taken after a roll.
	 *
	 *  Note:
	 *	Should the empty blob be treated differently than other blobs?
	 */
	{
		char wrap_path[MAX_FILE_PATH_LEN];
		int status;

		snprintf(wrap_path, sizeof wrap_path, "spool/wrap/%s:%s.brr",
						rp->algorithm, rp->digest);
		status = io_path_exists(wrap_path);
		if (status < 0)
			panic4(
				rp->verb,
				"stat(wrap) failed",
				strerror(errno),
				wrap_path
			);

		/*
		 *  A wrap log file with requested digest exists, so reply "no".
		 */
		if (status == 1) {
			char wrap_udig[MAX_UDIG_SIZE + 1];

			snprintf(wrap_udig, sizeof wrap_udig, "%s:%s",
						rp->algorithm, rp->digest);

			error3(
				"take",
				rp->transport,
				"blob in unrolled wrap set"
			);
			error3("take", "forbidden until a next roll", wrap_udig);
			return write_no(rp);
		}
	}

	rp->step = "request";
	if ((*mp->take_request)(rp))
		return write_no(rp);
	if (write_ok(rp))
		return -1;

	rp->step = "bytes";
	if ((*mp->take_bytes)(rp))
		return -1;

	/*
	 *  Did the client ack accepting the taken blob.
	 */
	rp->step = "read_reply";
	reply = read_reply(rp);
	if (reply == NULL)
		return -1;		//  error reading ok/no from client

	/*
	 *  Client replied 'no', for some reason, so just shutdown.
	 */
	if (*reply == 'n')
		return 0;

	/*
	 *  Ok, the client sucessfull took the blob,
	 *  so call the take_ok callback.
	 */
	rp->step = "reply";
	if ((*mp->take_reply)(rp, reply))
		return write_no(rp);
	return write_ok(rp);
}

/*
 *  Synopsis:
 *	Fire module callbacks for "give" verb in blobio protocol.
 *  Protocol Flow:
 * 	>give udig\n		# request to put blog matching a udig
 *	  <ok\n			#   server ready for blob bytes
 *	    >[bytes]		#     send digested bytes to server
 * 	      <ok\n		#   server accepts the bytes
 *	        >ok[close]	#     client probably forgets blob
 *		>no[close]	#     client might remember the blob
 *	  <no\n[close]		#   server rejects blob give request
 */
static int
give(struct request *rp, struct digest_module *mp)
{
	char *reply;

	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_GIVE << 2);

	rp->step = "request";
	if ((*mp->give_request)(rp))
		return write_no(rp);
	if (write_ok(rp))
		return -1;

	rp->step = "bytes";

	/*
	 *  Server can't take blob, so write "no" to client and shutdown.
	 */
	if ((*mp->give_bytes)(rp))
		return write_no(rp);
	if (write_ok(rp))
		return -1;

	/*
	 *  Read reply from client acking they have zapped blob.
	 */
	rp->step = "read_reply";
	reply = read_reply(rp);
	if (reply == NULL)
		return -1;

	rp->step = "reply";
	return (*mp->give_reply)(rp, reply);
}

static void
bio4d(char *verb, char *algorithm, char *digest,
       unsigned char *scan_buf, int scan_size)
{
	int status;
	struct digest_module *mp;
	int (*verb_callback)(struct request *, struct digest_module *) = 0;
	char ps_title[6 + 4 + 1];

	req.verb = verb;
	req.step = 0;
	if (strcmp("wrap", verb) == 0) {
		if (algorithm[0])
			die3_NO(verb, algorithm, "unexpected algorithm");
		strcpy(algorithm, wrap_digest_algorithm);
	} else if (!algorithm[0] || !digest[0])
		die2_NO(verb, "missing udig or unknown verb");

	req.algorithm = algorithm;
	req.blob_size = scan_size;
	req.digest = digest;

	/*
	 *  Find the digest algorithm in the installed list.
	 */
	mp = module_get(algorithm);
	if (!mp)
		die3_NO(verb, "unknown digest algorithm", algorithm);
	/*
	 *  Verify the digest is correct.
	 */
	if (*digest && !mp->is_digest(req.digest)) {
		char ebuf[MSG_SIZE];

		snprintf(ebuf, sizeof ebuf, "not a %s digest", algorithm);
		die3_NO(verb, ebuf, digest);
	}

	/*
	 *  Map the verb name onto the appropriate callback
	 *  BEFORE opening the module.
	 *
	 *  Note:
	 *  	Collapse into faster inline tests.
	 */
	if (strcmp("get", verb) == 0)
		verb_callback = get;
	else if (strcmp("put", verb) == 0)
		verb_callback = put;
	else if (strcmp("take", verb) == 0)
		verb_callback = take;
	else if (strcmp("give", verb) == 0)
		verb_callback = give;
	else if (strcmp("eat", verb) == 0)
		verb_callback = eat;
	else if (strcmp("wrap", verb) == 0)
		verb_callback = wrap;
	else if (strcmp("roll", verb) == 0)
		verb_callback = roll;
	else {
		die3_NO(algorithm, "unknown verb", verb);
		/*NOTREACHED*/
	}

	//  set the process title seen by the os

	strcpy(ps_title, "bio4d-");
	strcat(ps_title, verb);
	ps_title_set(ps_title, (char *)0, (char *)0);

	/*
	 *  Open the module.
	 */
	status = (*mp->open)(&req);
	if (status) {
		/*
		 *  Call close() despite the open() not succeeding.
		 */
		if ((*mp->close)(&req, status))
			error3(verb, algorithm, "digest->close() failed");
		/*
		 *  Bye, bye.
		 */
		die3_NO(verb, algorithm, "digest->open() failed");
	}

	req.scan_buf = scan_buf;
	req.scan_size = scan_size;

	/*
	 *  Fire the get/put/give/take/eat/wrap/roll callback.
	 */
	status = (*verb_callback)(&req, mp);

	/*
	 *  Close down the remote connection.
	 */
	if ((*mp->close)(&req, status) < 0)
		error3(mp->name, "close() failed", mp->name); 
}

/*
 *  Execute a client blobio request in the child process.
 */
static void
request()
{
	ssize_t nr;
	int state;
	char verb[MAX_VERB_SIZE + 1], *v_next, *v_end;
	char algorithm[MAX_ALGORITHM_SIZE + 1], *a_next, *a_end;
	char digest[MAX_DIGEST_SIZE + 1], *d_next, *d_end;
	char unsigned buf[MSG_SIZE];

	static char big_verb[] = "too many characters in verb";
	static char big_digest[] = "too many characters in digest";
	static char big_algorithm[] = "too many characters in algorithm";

	static char no_alpha_com[] = "non alpha character in verb";
	static char no_alnum_algo[] ="non alpha/numeric character in algorithm";
	static char no_num1_algo[] =
				"first character in algorithm can't be digit";
	static char no_print_digest[] = "unprintable character in digest";
	static char no_new_line[] = "new line expected after carriage return";

	ps_title_set("bio4d-request", (char *)0, (char *)0);
	v_next = verb;
	v_end = verb + MAX_VERB_SIZE;

	a_next = algorithm;
	a_end = algorithm + MAX_ALGORITHM_SIZE;

	d_next = digest;
	d_end = digest + MAX_DIGEST_SIZE;

	*v_next = *a_next = *d_next = 0;

	/*
	 *  Scan for request from the client.
	 *  We are looking for
	 *
	 *	UDIG_RE=[:alpha:][:alnum:]{0,7}:[[:isgraph:]]{32,128}]
	 *	[get|put|give|take|eat|wrap|roll] $UDIG_RE\n
	 *  
	 *  or
	 *
	 *	 wrap\n
	 */
	state = STATE_SCAN_VERB;
	while (state != STATE_HALT && (nr=req_read(&req, buf, sizeof buf)) > 0){

		unsigned char *b = buf;
		unsigned char *b_end = buf + nr;

		/*
		 *  We just read a chunk of bytes.
		 *  Parse the request.
		 */
		while (state != STATE_HALT && b < b_end) {
			int c = *b++;

			switch (state) {
			case STATE_SCAN_VERB:
				if (c == ' ') {
					*v_next = 0;
					state = STATE_SCAN_ALGORITHM;
				} else if (c == '\n') {
					*v_next = 0;
					STATE_go;
				} else if (c == '\r') {
					*v_next = 0;
					state = STATE_SCAN_NEW_LINE;
				} else if (isalpha(c)) {
					if (v_next == v_end)
						die_NO(big_verb);
					*v_next++ = c;
				} else {
					char ebuf[16];

					if (isprint(c))
						snprintf(ebuf, sizeof ebuf,
								"%c=0x%02x",
									c, c);
					else
						snprintf(ebuf, sizeof ebuf,
								"0x%02x", c);
					die2_NO(no_alpha_com, ebuf);
				}
				break;
			case STATE_SCAN_ALGORITHM:
				if (c == ':') {
					*a_next = 0;
					state = STATE_SCAN_DIGEST;
				} else if (c == '\n') {
					*a_next++ = 0;
					STATE_go;
				} else if (c == '\r') {
					*a_next++ = 0;
					state = STATE_SCAN_NEW_LINE;
				} else if (isalnum(c)) {
					if (a_next == a_end)
						die_NO(big_algorithm);
					/*
					 *  First character must be alpha.
					 */
					if (a_next == algorithm && !isalpha(c)){
						char ebuf[16];
						snprintf(ebuf, sizeof ebuf,
								"0x%02x", c);
						die2_NO(no_num1_algo, ebuf);
					}
					*a_next++ = c;
				} else {
					char ebuf[16];
					if (isprint(c))
						snprintf(ebuf, sizeof ebuf,
								"%c", c);
					else
						snprintf(ebuf, sizeof ebuf,
								"0x%02x", c);
					die2_NO(no_alnum_algo, ebuf);
				}
				break;
			/*
			 *  We saw a carriage return and are expecting
			 *  a new-line as the next character in the output.
			 */
			case STATE_SCAN_NEW_LINE:
				if (c != '\n')
					die(no_new_line);
				STATE_go;
				break;
			case STATE_SCAN_DIGEST:
				if (c == '\n') {
					STATE_go;
				} else if (c == '\r')
					state = STATE_SCAN_NEW_LINE;
				else if (isgraph(c)) {
					if (d_next == d_end)
						die_NO(big_digest);
					*d_next++ = c;
				} else {
					char ebuf[16];

					snprintf(ebuf, sizeof ebuf, "0x%02x",c);
					die2_NO(no_print_digest, ebuf);
				}
				break;
			default: {
				char ebuf[16];
				
				snprintf(ebuf, sizeof ebuf, "0x%x", state);
				panic2("request: impossible state", ebuf);
			}}
		}
	}
	if (state != STATE_HALT && nr < 0)
		die_NO("read_buf(request) failed");
	if (state == STATE_SCAN_VERB && v_next == verb)
		die_NO("empty read_buf(verb)");

	/*
	 *  Write out the blob request record if the verb halted "normally".
	 */
	if (state == STATE_HALT) {
		char *ch = req.chat_history;
		int len = strlen(req.chat_history);

		switch (len) {
		case 0:
			panic("unexpected empty length of chat history");
			break;
			;;
		case 2:
			/*
			 *  "no"
			 */
			if (ch[0] == 'n')
				request_exit_status=(request_exit_status&0x9F)| 
					(REQUEST_EXIT_STATUS_CHAT_NO << 5)
				;
			break;
		case 5:
			/*
			 *  "ok,no"
			 */
			if (ch[3] == 'n')
				request_exit_status=(request_exit_status&0x9F)| 
					(REQUEST_EXIT_STATUS_CHAT_NO2 << 5)
				;
			break;
		case 8:
			/*
			 *  "ok,ok,no"
			 */
			if (ch[5] == 'n')
				request_exit_status =(request_exit_status&0x9F)|
					(REQUEST_EXIT_STATUS_CHAT_NO3 << 5)
				;
			break;
		default:
			panic2("unexpected chat history", ch);
		}
		/*
		 *  Record the ending time.
		 */
		if (clock_gettime(CLOCK_REALTIME, &req.end_time) < 0)
			panic2("clock_gettime(end REALTIME) failed",
							strerror(errno));
		brr_write(&req);
	} else
		error("incomplete read from client");
	/*
	 *  Exit the child process that handled the single request.
	 */
	leave(request_exit_status);
}

/*
 *  Synopsis:
 *	Reap child requests and update various stats.
 *
 *  Child Exit Status Codes:
 *  	First seven bits of the exit status encode the final state of the
 *  	request process.  Statistics are accumulated.
 *
 *	Process Exit Class - Bits 1 and 2:
 *
 *	  ------00	normal request  - successful request
 *	  ------01	client error	- unknown verb, bad udig syntax,
 *					  read error taking to client
 *	  ------10	hard time out	- read()/write() timeout occured
 *	  ------11	fault		- error affecting server stability
 *
 *  	Verb - Bits 3, 4, and 5
 *
 *	  ---000--	unused
 *	  ---001--	get request 	
 *	  ---010--	put request 	
 *	  ---011--	give request 	
 *	  ---100--	take request 	
 *	  ---101--	eat request 	
 *	  ---110--	wrap request 	
 *	  ---111--	roll request 	
 *
 *	Client Chat - Bits 6 and 7
 *
 *	  -00-----	client chat ok 	- "ok", "ok,ok", "ok,ok,ok"
 *	  -01-----	client chat no
 *	  -10-----	client chat ok,no
 *	  -11-----	client chat ok,ok,no
 *  Note:
 *	Panicing too soon upon unexpected termination of arborist or loggers.
 */
static void
reap_request()
{
	pid_t corpse;
	int status;
	static char n[] = "reap_request";

	/*
	 *  Poll for zombie kids.
	 */
again:
	while ((corpse = waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
		if (corpse == logger_pid) {
			char buf[MSG_SIZE];

			logger_pid = 0;
			if (leaving)
				continue;
			snprintf(buf, sizeof buf,
				"signaled=%d, exited=%d, core=%c, status=%d",
				WIFSIGNALED(status),
				WIFEXITED(status),
#ifdef WCOREDUMP
				WCOREDUMP(status) ? '1' : '0',
#else
				'?',
#endif
				status
			);
			panic2("unexpected exit of logger process", buf);
		}
		if (corpse == brr_logger_pid) {
			char buf[MSG_SIZE];

			brr_logger_pid = 0;
			if (leaving)
				continue;
			snprintf(buf, sizeof buf,
				"signaled=%d, exited=%d, core=%c, status=%d",
				WIFSIGNALED(status),
				WIFEXITED(status),
#ifdef WCOREDUMP
				WCOREDUMP(status) ? '1' : '0',
#else
				'?',
#endif
				status
			);
			panic2("unexpected exit of brr logger process", buf);
		}
		if (corpse == arborist_pid) {
			char buf[MSG_SIZE];

			arborist_pid = 0;
			if (leaving)
				continue;
			snprintf(buf, sizeof buf,
				"signaled=%d, exited=%d, core=%c, status=%d",
				WIFSIGNALED(status),
				WIFEXITED(status),
#ifdef WCOREDUMP
				WCOREDUMP(status) ? '1' : '0',
#else
				'?',
#endif
				status
			);
			panic2("unexpected exit of arborist process", buf);
		}

		/*
		 *  Process exited abnormally with signal, so log status.
		 *  No brr record is ever generated for a signaled process.
		 */
		if (WIFSIGNALED(status)) {
			char buf[128];
			unsigned char sig;

			signal_count++;
			sig = WTERMSIG(status);

			snprintf(buf, sizeof buf,
					"process #%u exited with signal %s(%d)",
					corpse, sig_name(sig), sig
			);
			switch (sig) {
			case SIGINT: case SIGQUIT: case SIGTERM:
			case SIGPIPE:
				warn(buf);
				break;
			case SIGKILL:
				error(buf);
				break;
			default:
				panic(buf);
			}
#ifdef WCOREDUMP
			if (WCOREDUMP(status))
				warn("core dump generated");
#endif
		}

		/*
		 *  Process exited normally.  Accumlate stats derived
		 *  from bits in the exit status.
		 */
		if (WIFEXITED(status)) {
			unsigned char s8 = WEXITSTATUS(status);

			/*
			 *  Process exit status in first 2 lower bits.
			 */
			switch (s8 & 0x3) {
			case REQUEST_EXIT_STATUS_SUCCESS:
				success_count++;
				break;
			case REQUEST_EXIT_STATUS_ERROR:
				error_count++;
				break;
			case REQUEST_EXIT_STATUS_TIMEOUT:
				timeout_count++;
				break;
			case REQUEST_EXIT_STATUS_FAULT:
				fault_count++;
				break;
			}

			/*
			 *  Verb stored in bits 3,4 and 5
			 */
			switch ((s8 & 0x1C) >> 2) {
			case REQUEST_EXIT_STATUS_GET:
				get_count++;
				break;
			case REQUEST_EXIT_STATUS_PUT:
				put_count++;
				break;
			case REQUEST_EXIT_STATUS_GIVE:
				give_count++;
				break;
			case REQUEST_EXIT_STATUS_TAKE:
				take_count++;

				//  blob did not exists (single no)

				if (((s8 & 0x60) >> 5) == 
					     REQUEST_EXIT_STATUS_CHAT_NO)
					take_no_count++;
				break;
			case REQUEST_EXIT_STATUS_EAT:
				eat_count++;

				//  blob probably did not exist

				if (((s8 & 0x60) >> 5) == 
					     REQUEST_EXIT_STATUS_CHAT_NO)
					eat_no_count++;
				break;
			case REQUEST_EXIT_STATUS_WRAP:
				wrap_count++;
				break;
			case REQUEST_EXIT_STATUS_ROLL:
				roll_count++;
				break;
			default: {
				char buf[MSG_SIZE];

				/*
				 *  Cheap sanity test of exit status code.
				 *  An exit status with no errors must have
				 *  a verb.
				 */
				if (s8 & 0x3)
					break;
				snprintf(buf, sizeof buf,
					"pid #%u: no verb in exit status: 0x%x",
					 corpse, s8
				);
				panic(buf);
			}}
			/*
			 *  Chat history stored in bits 6 and 7.
			 */
			if ((s8 & 0x3) == 0) 		/* normal request */
				switch ((s8 & 0x60) >> 5) {
				case REQUEST_EXIT_STATUS_CHAT_OK:
					chat_ok_count++;
					break;
				case REQUEST_EXIT_STATUS_CHAT_NO:
					chat_no_count++;
					break;
				case REQUEST_EXIT_STATUS_CHAT_NO2:
					chat_no2_count++;
					break;
				case REQUEST_EXIT_STATUS_CHAT_NO3:
					chat_no3_count++;
					break;
				}
		}
	}
	if (corpse) {
		if (errno == EINTR)
			goto again;
		panic3(n, "waitpid(WNOHANG) failed", strerror(errno));
	}
}

static void
heartbeat()
{
	char buf[MSG_SIZE];
	u8 connect_diff;
	float connect_rate;
	static u8 prev_connect_count = 0;
	time_t now;

	time(&now);

	if (now - recent_pid_heartbeat >= PID_HEARTBEAT) {
		struct timeval times[2];

		times[0].tv_sec = times[1].tv_sec = now;
		times[0].tv_usec = times[1].tv_usec = 0;
		if (io_utimes(pid_path, times) < 0)
			panic2("io_utime(pid log) failed", strerror(errno));
		recent_pid_heartbeat = now;
	}

	if (now - recent_log_heartbeat < LOG_HEARTBEAT)
		return;

	snprintf(buf, sizeof buf, "heartbeat: %u sec", LOG_HEARTBEAT);
	info(buf);

	/*
	 *  Only burp out message when request count changes.
	 *  A simple tickle of the listen socket will change the request count.
	 */
	if (prev_connect_count == connect_count)
		return;

	snprintf(buf, sizeof buf,
		"suc=%llu, err=%llu, tmo=%llu, sig=%llu, flt=%llu",
			success_count,
			error_count,
			timeout_count,
			signal_count,
			fault_count
	);
	info(buf);

	snprintf(buf, sizeof buf,
		"get=%llu, put=%llu, give=%llu, take=%llu, eat=%llu",
			get_count,
			put_count,
			give_count,
			take_count,
			eat_count
	);
	info(buf);

	snprintf(buf, sizeof buf, "wrap=%llu, roll=%llu",wrap_count,roll_count);
	info(buf);

	snprintf(buf, sizeof buf,
	      "chat: ok=%llu, no[123]=%llu, eat|take no=%llu|%llu",
			chat_ok_count,
			chat_no_count + chat_no2_count + chat_no3_count,
			eat_no_count,
			take_no_count
	);
	info(buf);

	connect_diff = connect_count - prev_connect_count;
	connect_rate = (float)connect_diff / (float)LOG_HEARTBEAT;
	snprintf(buf, sizeof buf, "sample: %.0f connect/sec, connect=%llu",
				connect_rate, connect_diff);
	info(buf);

	prev_connect_count = connect_count;
}

/*
 *  Write out a round robin database sample to log/bio4d-rrd-sample.log
 *  A cron job is expected to merge the samples into log/bio-stat.rrd
 *  and roll the file log/bio4d-rrd-sample.log.
 *
 *  Each line sample is suitable as an argument to cron driven rrdupdate.
 *  The sample looks like:
 *
 *	time epoch:
 *		//  request summaries
 *
 *		success_count:		//  request satisfied
 *		error_count:		//  stable error in request
 *		timeout_count:		//  timeout in request
 *		signal_count:		//  request ended due to signal
 *		fault_count:		//  unstable error in request
 *
 *		//  verb summaries, regardless of ok/no chat history
 *
 *		get_count:
 *		put_count:
 *		give_count:		
 *		take_count:
 *		eat_count:
 *		wrap_count:
 *		roll_count:
 *
 *		//  chat history summaries
 *
 *		chat_ok_count:
 *		chat_no_count:
 *		chat_no2_count:
 *		chat_no3_count:
 *		eat_no_count:
 *		take_no_count
 */
static void
rrd_sample()
{
	int fd;
	char buf[512];		/* <= PIPE_MAX */
	time_t now;

	if (rrd_sample_duration == 0)
		return;
	/*
	 *  Request summaries
	 */
	static u8	success_count_prev =	0;
	static u8	error_count_prev =	0;
	static u8	timeout_count_prev =	0;
	static u8	signal_count_prev =	0;
	static u8	fault_count_prev =	0;

	/*
	 *  Request Verbs
	 */
	static u8	get_count_prev =	0;
	static u8	put_count_prev =	0;
	static u8	give_count_prev =	0;
	static u8	take_count_prev =	0;
	static u8	eat_count_prev =	0;
	static u8	wrap_count_prev =	0;
	static u8	roll_count_prev =	0;

	/*
	 *  Request chat summaries.
	 */
	static u8	chat_ok_count_prev =	0;
	static u8	chat_no_count_prev =	0;
	static u8	chat_no2_count_prev =	0;
	static u8	chat_no3_count_prev =	0;
	static u8	eat_no_count_prev =	0;
	static u8	take_no_count_prev =	0;

	time(&now);
	if (now - rrd_sample_prev < rrd_sample_duration)
		return;

	static char format[] =
		"%llu:"					/* time epoch */

		"%llu:%llu:%llu:%llu:%llu:"		/* process exit class */
		"%llu:%llu:%llu:%llu:%llu:%llu:%llu:"	/* verb count */
		"%llu:%llu:%llu:%llu:%llu:%llu"		/* chat history */
		"\n"
	;

	fd = io_open_append(rrd_log, 0);
	if (fd < 0)
		panic3(rrd_log, "open(rrd log) failed", strerror(errno));
	snprintf(buf, sizeof buf, format,
		now,

 		success_count - success_count_prev,
		error_count - error_count_prev,
		timeout_count - timeout_count_prev,
		signal_count - signal_count_prev,
		fault_count - fault_count_prev,

		get_count - get_count_prev,
		put_count - put_count_prev,
		give_count - give_count_prev,
		take_count - take_count_prev,
		eat_count - eat_count_prev,
		wrap_count - wrap_count_prev,
		roll_count - roll_count_prev,

		chat_ok_count - chat_ok_count_prev,
		chat_no_count - chat_no_count_prev,
		chat_no2_count - chat_no2_count_prev,
		chat_no3_count - chat_no3_count_prev,
		eat_no_count - eat_no_count_prev,
		take_no_count - take_no_count_prev
	);
	if (io_write(fd, buf, strlen(buf)) < 0)
		panic3(rrd_log, "write(rrd log) failed", strerror(errno));
	if (io_close(fd))
		panic3(rrd_log, "close(rrd log) failed", strerror(errno));

	//  reset the samples
	rrd_sample_prev = now;
	success_count_prev = success_count;
	error_count_prev = error_count;
	timeout_count_prev = timeout_count;
	signal_count_prev = signal_count;
	fault_count_prev = fault_count;

	get_count_prev = get_count;
	put_count_prev = put_count;
	give_count_prev = give_count;
	take_count_prev = take_count;
	eat_count_prev = eat_count;
	wrap_count_prev = wrap_count;
	roll_count_prev = roll_count;

	chat_ok_count_prev = chat_ok_count;
	chat_no_count_prev = chat_no_count;
	chat_no2_count_prev = chat_no2_count;
	chat_no3_count_prev = chat_no3_count;
	eat_no_count_prev = eat_no_count;
	take_no_count_prev = take_no_count;
}

//  Note: not sure if CHLD signal is queued during handler!

static void
catch_CHLD(int sig)
{
	(void)sig;

	reap_request();
	heartbeat();
	rrd_sample();
}

static void
catch_terminate(int sig)
{
	switch (sig) {
	case SIGBUS:  case SIGSEGV:  case SIGFPE:  case SIGSYS:
	case SIGXCPU:  case SIGXFSZ:

#ifdef SIGSTKFLT
	case SIGSTKFLT:
#endif

#ifdef SIGEMT
	case SIGEMT:
#endif
		panic2("caught signal", strsignal(sig));
	}
	warn2("caught signal", sig_name(sig));
	leave(1);
}

static void
open_listen( unsigned short port)
{
	int bool_opt;
	char buf[MSG_SIZE];

	/*
	 *  Allocate a stream packet socket.
	 */
	listen_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_fd < 0)
		die2("socket(PF_INET) failed", strerror(errno));
	/*
	 *  Allow the listen socket to bind to an address:port for which 
	 *  "ACTIVE" connections still exist. Typically these connections
	 *  are in a TIME_WAIT state.  Supposedly older systems
	 *  implement this option, incorrectly, allowing mutiple process
	 *  to bind simulatneously, although I am not aware of any examples.
	 */
	bool_opt = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
				       &bool_opt, sizeof bool_opt) < 0)
		die2("setsockopt(listen, REUSEADDR) failed", strerror(errno));
	/*
	 *  Bind socket to internet address *:port.
	 */
	memset((void *)&req.bind_address, 0, sizeof req.bind_address);
	req.bind_address.sin_family = AF_INET;
	req.bind_address.sin_port = htons(port);
	req.bind_address.sin_addr.s_addr = INADDR_ANY;
	snprintf(buf, sizeof buf, "binding tcp socket to inet %s:%d",
			net_32addr2text(ntohl(
				req.bind_address.sin_addr.s_addr)),
			(int)ntohs(req.bind_address.sin_port));
	info(buf);
try_bind:
	if (bind(listen_fd, (const void *)&req.bind_address,
						sizeof req.bind_address) < 0) {
		int e = errno;
		if (e == EINTR)
			goto try_bind;

		snprintf(buf, sizeof buf, "bind(%s:%d) failed",
				net_32addr2text(ntohl(
					req.bind_address.sin_addr.s_addr)),
				(int)ntohs(req.bind_address.sin_port));
		die2(buf, strerror(e));
	}

	/*
	 *  Add to listen queue.
	 */
try_listen:
	if (listen(listen_fd, SOMAXCONN) < 0) {
		int e = errno;

		if (e == EINTR)
			goto try_listen;
		die2("listen() failed", strerror(errno));
	}
}

static void
set_pid_log(char *path)
{
	static char n[] = "set_pid_log";
	char buf[MSG_SIZE];

	switch (io_path_exists(path)) {
	case -1:
		panic4(n, "io_path_exists() failed", strerror(errno), path);
		return;
	case 0:
		break;
	case 1: {
		pid_t pid;

		warn3(n, "file exists", pid_path);
		if (slurp_text_file(pid_path, buf, sizeof buf))
			die3(n, "slurp_text_file() failed", pid_path);
		if (sscanf(buf, "%d", &pid) != 1)
			die3(n, "can't read process id in file", pid_path);
		error3(n, "pid log already exists", pid_path);
		snprintf(
			buf,
			sizeof buf,
			"is another bio4d running with pid=%u",
			pid
		);
		die2(n, buf);
		break;
	}
	default:
		panic3(n, "io_path_exists() returned unexpected value", path);
	}
	snprintf(buf, sizeof buf, "%lld\n%lld\n",
		(long long)getpid(),
		(long long)start_time
	);
	if (burp_text_file(buf, pid_path))
		die3(n, "burp_text_file() failed", pid_path);
	buf[strlen(buf) - 1] = 0;		//  zap new line
	/*
	 *  Chmod run/bio4d.pid u=rw,go=
	 */
	if (io_chmod(pid_path, S_IRUSR | S_IWUSR))
		panic4(n, "chmod(pid) failed", strerror(errno), pid_path);
}

static void 
daemonize()
{
	pid_t pid;
	char buf[MSG_SIZE];
	static char n[] = "daemonize";

	info("daemonizing");

	/*
	 *  Server always runs as user blobio.
	 *
	 *  Note:
	 *	This code is probably not needed with modern init managers.
	 */
	if (getuid() == 0) {
		struct passwd *pwd = getpwnam("blobio");

		if (pwd == NULL) {
			panic("root user must setuid() to user blobio");
			die2(n, "no password entry for user blobio");
		}
		/*
		 *  Set real and effective group id.
		 */
		if (setgid(pwd->pw_uid) < 0) {
			int e = errno;

			snprintf(buf, sizeof buf, "setgid(blobio:%d) failed",
								pwd->pw_uid);
			die3(n, buf, strerror(e));
		}
		/*
		 *  Set real and effective user id.
		 */
		if (setuid(pwd->pw_uid) < 0) {
			int e = errno;

			snprintf(buf, sizeof buf, "setuid(blobio:%d) failed",
								pwd->pw_uid);
			die3(n, buf, strerror(e));
		}
	}

	/*
	 *  Put ourselves in the background.
	 */
	pid = fork();
	if (pid < 0)
		die3(n, "fork() failed", strerror(errno));
	if (pid > 0)
		exit(0);	/* caller in foreground */

	/*
	 *  Set up posix session.
	 */
	pid = setsid();
	if (pid < 0)
		die3(n, "setsid() failed", strerror(errno));

	/*
	 *  Shutdown stdin/stdout.
	 */
	if (io_close(0) < 0)
		die3(n, "close(0) failed", strerror(errno));
	if (io_close(1) < 0)
		die3(n, "close(1) failed", strerror(errno));
}

/*
 *  Fork a blob request by a remote client.
 *  Return 1 in the parent (bio4d) and 0 in the child (the request).
 */
static int
fork_request()
{
	pid_t pid;
	static char n[] = "fork_request";

	pid = fork();
	if (pid < -1)
		panic3(n, "fork() failed", strerror(errno));
	if (pid)
		return 1;
	/*
	 *  In child, so reset signals, free resources, set
	 */
	master_pid = 0;
	request_pid = logged_pid = getpid();

	/*
	 *  Inform outside world.
	 */
	ps_title_set("bio4d-request", (char *)0, (char *)0);
	return 0;
}

/*
 *  Fork a process to handle accepted socket.
 */
static void
fork_accept(struct request *rp)
{
	static char n[] = "fork_accept";
	int status;

	/*
	 *  Fork a child to handle the request.
	 *  In the parent - the master bio4d process - just close down
	 *  the connection and return to accept more requests.
	 */
	if (fork_request()) {
		if (io_close(rp->client_fd))
			panic2("close(client fd) failed", strerror(errno));
		rp->client_fd = -1;
		return;
	}

	/* in the child request */

	/*
	 *  Record start time.
	 */
	if (clock_gettime(CLOCK_REALTIME, &rp->start_time) < 0)
		panic3(n, "clock_gettime(start REALTIME) failed",
						strerror(errno));
	/*
	 *  Shutdown listen fd.
	 *  Need to disable signals here?
	 */
	status = io_close(listen_fd);
	listen_fd = -1;
	if (status < 0)
		die3(n, "close(listen) failed", strerror(errno));

	/*
	 *  Build a description of a network connection for blob request
	 *  record and error messages.
	 */
	strcpy(rp->transport_tiny,
		net_32addr2text(ntohl(rp->remote_address.sin_addr.s_addr)));
	snprintf(rp->transport, sizeof rp->transport - 1,
		"tcp4~%s:%u;%s:%u",
		net_32addr2text(ntohl(rp->remote_address.sin_addr.s_addr)),
		(unsigned int)ntohs(rp->remote_address.sin_port),
		net_32addr2text(ntohl(rp->bind_address.sin_addr.s_addr)),
		(unsigned int)ntohs(rp->bind_address.sin_port)
	);

	request();

	panic2(n, "unexpected return from request()");
}

int
main(int argc, char **argv, char **env)
{
	char buf[MSG_SIZE];
	sigset_t mask;
	unsigned short port;
	int i;

	time(&start_time);

	strcpy(wrap_digest_algorithm, DEFAULT_WRAP_DIGEST_ALGORITHM);
	/*
	 *  Parse verb line arguments.
	 */
	for (i = 1;  i < argc;  i++) {
		/*
		 *  --ps-title-XXXXXXXXXXX is an ugly hack.  see ps_title.c
		 */
		if (strncmp("--ps-title-", argv[i], 11) == 0)
			continue;
		if (strcmp("--port", argv[i]) == 0) {
			int p;

			if (++i >= argc)
				die("option --port: missing port number");
			p = atoi(argv[i]);
			if (p <= 0)
				die2("port <= 0", argv[i]);
			if (p > 65536)
				die2("port > 65536", argv[i]);
			port = p;
		} else if (strcmp("--rrd-sample-duration", argv[i]) == 0) {
			static char o[] = "option --rrd-sample-duration";
			char *hb;

			if (++i >= argc)
				die2(o, "missing duration");
			hb = argv[i];
			if (strcmp("", hb) == 0)
				die2(o, "empty seconds");
			if (strcmp("heartbeat", argv[i]) == 0)
				rrd_sample_duration = LOG_HEARTBEAT;
			else if (isdigit(hb[0])) {
				unsigned j, sec;

				if (strlen(hb) > 4)
					die3(o,"seconds must be < 5 digits",hb);
				for (j = 1;  hb[j] && j < 4;  j++) {
					if (isdigit(hb[j]))
						continue;
					die3(o, "non digit in seconds", hb);
				}
				if (sscanf(hb, "%u", &sec) != 1)
					die3(o, "sscanf(seconds) failed", hb);
				rrd_sample_duration = (u2)sec;
			} else
				die3(o, "unexpected seconds or heartbeat", hb);
		} else if (strcmp("--wrap-algorithm", argv[i]) == 0) {
			static char o[] = "option --wrap-algorithm";

			if (++i >= argc)
				die2(o, "missing algorithm");

			if (*argv[i] == 0)
				die2(o, "empty algorithm");

			if (wrap_digest_algorithm[0])
				die2(o, "given more than once");

			//  verify the digest module exists
			if (!module_get(argv[i]))
				die3(o, "unknown digest algorithm", argv[i]);
			strcpy(wrap_digest_algorithm, argv[i]);
		} else if (strcmp("--in-foreground", argv[i]) == 0) {
			static char o[] = "option --in-foreground";

			if (in_foreground)
				die2(o, "given more than once");
			in_foreground = 1;
		} else if (strcmp("--root", argv[i]) == 0) {
			static char o[] = "option --root";

			if (++i >= argc)
				die2(o, "missing root directory path");
			if (BLOBIO_ROOT)
				die2(o, "given more than once");
			if (argv[i] == 0)
				die2(o, "empty directory path");
			BLOBIO_ROOT = strdup(argv[i]);
		} else if (strcmp("--net-timeout", argv[i]) == 0) {
			static char o[] = "option --net-timeout";
			char *tmo;
			unsigned j, sec;

			if (++i >= argc)
				die2(o, "missing timeout seconds");
			tmo = argv[i];
			if (net_timeout >= 0)
				die2(o, "given more than once");
			if (argv[i] == 0)
				die2(o, "empty seconds");

			if (!isdigit(tmo[0]))
				die3(o, "unexpected seconds or timeout", tmo);

			if (strlen(tmo) > 3)
				die3(o,"seconds must be < 4 digits", tmo);
			for (j = 1;  tmo[j] && j < 4;  j++) {
				if (isdigit(tmo[j]))
					continue;
				die3(o, "non digit in timeout seconds", tmo);
			}
			if (sscanf(tmo, "%u", &sec) != 1)
				die3(o, "sscanf(timeout seconds) failed", tmo);
			if (sec > 255)
				die3(o, "timeout > 255 seconds", tmo);
			if (sec == 0)
				die2(o, "timeout is 0");
			net_timeout = (u1)sec;
		} else if (strncmp("--", argv[i], 2) == 0)
			die2("unknown option", argv[i]);
		else
			die2("unknown argument", argv[i]);
	}

	if (rrd_sample_duration > 0 && rrd_sample_duration < LOG_HEARTBEAT) {
		char ebuf[512];

		sprintf(ebuf, "rrd sample duration < heartbeat: %d < %d sec",
				rrd_sample_duration, LOG_HEARTBEAT);
		die(ebuf);
	}

	if (rrd_sample_duration > 0 && rrd_sample_duration < ACCEPT_TIMEOUT) {
		char ebuf[512];
		static char r2small[] =
		    "rrd sample duration < socket accept duration: %d < %d sec";

		sprintf(ebuf, r2small, rrd_sample_duration, ACCEPT_TIMEOUT);
		die(ebuf);
	}

	if (net_timeout == -1)
		net_timeout = NET_TIMEOUT;

	if (port == 0)
		port = BIO4D_PORT;

	if (!BLOBIO_ROOT)
		die("option --root <directory-path> is required");
	/*
	 *  Goto $BLOBIO_ROOT directory.
	 */
	if (chdir(BLOBIO_ROOT) < 0) {
		snprintf(buf, sizeof buf, "chdir(%s) failed: %s", BLOBIO_ROOT,
							strerror(errno));
		die(buf);
	}

	ps_title_init(argc, argv);

	if (!in_foreground)
		daemonize();
	set_pid_log(pid_path);

	master_pid = logged_pid = getpid();

	log_open();

	info("hello, world");

	snprintf(buf, sizeof buf, "logger process id: %u", logger_pid);
	info(buf);

	info2("wrap digest algorithm", wrap_digest_algorithm);

	/*
	 *  Calculate current working directory.
	 */
	if (getcwd(buf, sizeof buf) != buf)
		die2("getcwd() failed", strerror(errno));
	info2("current directory", buf);

	/*
	 *  Dump process environment.  Want a minimal environment in production,
	 *  so this dump forces keeping the enironment simple.
	 */
	info("dumping process environment variables ...");
	if (*env[0])
		for (i = 0;  env[i];  i++)
			info(env[i]);
	else
		info("no environment variables (ok)");

	if (module_boot())
		die("modules_boot() failed");

	tmp_open();

	brr_open();
	snprintf(buf, sizeof buf, "brr logger process id: %u", brr_logger_pid);
	info(buf);

	arbor_open();
	snprintf(buf, sizeof buf, "arborist process id: %u", arborist_pid);
	info(buf);

	/*
	 *  Catch various signals.
	 *
	 *  Note:
	 *	Probably ought to mask only subset being caught??
	 */
	sigfillset(&mask);
	if (sigprocmask(SIG_UNBLOCK, &mask, (sigset_t *)0) != 0)
		panic2("sigprocmask(SETMASK) failed", strerror(errno));

	if (signal(SIGCHLD, catch_CHLD) == SIG_ERR)
		panic2("signal(CHLD) failed", strerror(errno));
	if (signal(SIGINT, catch_terminate) == SIG_ERR)
		panic2("signal(INT) failed", strerror(errno));
	if (signal(SIGQUIT, catch_terminate) == SIG_ERR)
		panic2("signal(QUIT) failed", strerror(errno));
	if (signal(SIGTERM, catch_terminate) == SIG_ERR)
		panic2("signal(TERM) failed", strerror(errno));
	if (signal(SIGABRT, catch_terminate) == SIG_ERR)
		panic2("signal(ABRT) failed", strerror(errno));
	if (signal(SIGSEGV, catch_terminate) == SIG_ERR)
		panic2("signal(SEGV) failed", strerror(errno));
	if (signal(SIGBUS, catch_terminate) == SIG_ERR)
		panic2("signal(BUS) failed", strerror(errno));
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		panic2("signal(PIPE ignore) failed", strerror(errno));

#ifdef SIGPWR
	if (signal(SIGPWR, catch_terminate) == SIG_ERR)
		panic2("signal(PWR) failed", strerror(errno));
#endif
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
		panic2("signal(HUP, SIG_IGN) failed", strerror(errno));

	ps_title_set("bio4d-listen", (char *)0, (char *)0);
	/*
	 *  Open the socket to listen for requests.
	 */
	open_listen(port);
	snprintf(buf, sizeof buf, "socket accept timeout: %u seconds",
						ACCEPT_TIMEOUT);
	info(buf);

	snprintf(buf, sizeof buf, "log heartbeat: %u sec", LOG_HEARTBEAT);
	snprintf(buf, sizeof buf, "pid heartbeat: %u sec", PID_HEARTBEAT);
	info(buf);

	if (rrd_sample_duration > 0) {
		snprintf(buf, sizeof buf, "rrd sample duration: %u sec",
							rrd_sample_duration);
		info(buf);
		info2("rrd log", rrd_log);
	} else
		warn("rrd sampling disabled");

	if (net_timeout > 0) {
		snprintf(buf, sizeof buf, "net timeout: %ds sec", net_timeout);

		info(buf);
	} else
		warn("net timeout: disabled");

	memset(req.chat_history, 0, sizeof req.chat_history);
	req.remote_len = sizeof req.remote_address;
	req.read_timeout = req.write_timeout = net_timeout;

	info("accepting incoming requests ...");
accept_request:
	switch (net_accept(
			listen_fd,
			(struct sockaddr *)&req.remote_address,
			&req.remote_len,
			&req.client_fd,
			/*
			 *  Note:
			 *	Should ACCCEPT_TIMEOUT match read/write timeout?
			 */
			ACCEPT_TIMEOUT)) {
	case -1:
		die2("accept(server listen socket) failed", strerror(errno));
		/*NOTREACHED*/
		break;
	case 0:
		connect_count++;
		fork_accept(&req);
		break;
	case 1:			//  timeout
		break;
	default:
		panic("net_accept() returned impossible value");
		/*NOTREACHED*/
	}
	heartbeat();
	rrd_sample();
	reap_request();
	goto accept_request;
}
