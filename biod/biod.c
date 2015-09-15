/*
 *  Synopsis:
 *	A reference server that speaks the blob.io protocol
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 *  Note:
 *	Signal handling needs to be pushed to main listen loop or cleaned
 *	up with sigaction().
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

#include "biod.h"

#ifdef __APPLE__
#include "macosx.h"
#endif

#define BIOD_PORT		1797

#define ACCEPT_TIMEOUT		3
#define MAX_VERB_SIZE		5

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
	biod(verb, algorithm, digest, b, b_end - b);			\
	state = STATE_HALT;						\
}
	
#define HEARTBEAT		15	/* 15 seconds */
#define REQUEST_READ_TIMEOUT	20	/* 20 seconds */
#define REQUEST_WRITE_TIMEOUT	20	/* 20 seconds */
#define LEAVE_PAUSE		3	/* 3 seconds */

extern pid_t	brr_logger_pid;
extern pid_t	logged_pid;
extern pid_t	arborist_pid;

/*
 *  Return exit status of request child process.
 *  State bits are ORed in as request proceeds.
 */
time_t last_log_heartbeat		= 0;

void		**module_boot_data = 0;
char		*BLOBIO_ROOT = 0;
pid_t		request_pid = 0;
pid_t		master_pid = 0;
pid_t		logger_pid = 0;
int		leaving = 0;
unsigned char	request_exit_status = 0;
time_t		start_time;	

static char	pid_path[] = "run/biod.pid";

static struct request	req =
{
	.client_fd	=	-1,
	.verb		=	0,
	.algorithm	=	0,
	.digest		=	0,
	.scan_buf	=	0,
	.scan_size	=	0,
	.open_data	=	0,
	.blob_size	=	0,
	.read_timeout	=	REQUEST_READ_TIMEOUT,
	.write_timeout	=	REQUEST_WRITE_TIMEOUT
};

static int		listen_fd = -1;

/*
 *  Track inbound connections and request exit states.
 */
static u8	connect_count = 0;	//  connections answered
static u8	success_count =	0;	//  requests exit ok
static u8	timeout_count =	0;	//  request timedout
static u8	error_count =	0;	//  request had client/network error
static u8	fault_count =	0;	//  request faulted
static u8	signal_count =	0;	//  request terminated with signal

/*
 *  Track request verbs
 */
static u8	get_count =	0;
static u8	put_count =	0;
static u8	give_count =	0;
static u8	take_count =	0;
static u8	eat_count =	0;
static u8	wrap_count =	0;
static u8	roll_count =	0;

/*
 *  Track request handshakes with the client.
 */
static u8	chat_ok_count = 0;
static u8	chat_no_count = 0;
static u8	chat_no2_count =0;
static u8	chat_no3_count =0;
static u8	eat_no_count =	0;
static u8	take_no_count =	0;

static u2	rrd_sample_duration = 0;
static char	rrd_log[] = "log/biod-rrd.log";

/*
 *  Exit quickly and quietly, shutting down the logger.
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
	signal(SIGALRM, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	leaving = 1;		/* hush reaper complaints about exiting */
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

	/*
	 *  In the master, so shutdown all children, the logger last.
	 */
	if (my_pid == master_pid) {
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
		if (io_unlink(pid_path) && errno != ENOENT) {
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
	}
	exit(exit_status);
}

/*
 *  Synoposis:
 *	Write a panicy message to log and then exit.
 */
static void
die(char *msg)
{
	if (logged_pid == 0) {			/* still booting */
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

	/*
	 *  In a child process.
	 */
	error(msg);

	/*
	 *  Dieing implies either client error, timeout or fault.
	 *  Therefore, an uncategorized error must "promoted" to a full fault;
	 *  otherwise, leave(), not die() would have been invoked.
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
			error2(req.netflow, "write(no) failed");
	die2(req.netflow, msg);
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
 *	    <ok\n[blob][close]		# server sends blob, bye
 *          <no\n[close]		# server rejects blob, bye
 */
static int
get(struct request *rp, struct digest_module *mp)
{
	int status;

	request_exit_status = (request_exit_status & 0x1C) |
					(REQUEST_EXIT_STATUS_GET << 2);
	/*
	 *  Note:
	 *	Why not set verb in caller?
	 */
	rp->verb = "get";
	status = (*mp->get)(rp);
	if (status) {
		if (status < 0)
			error3("get", rp->algorithm, "callback get() failed");
		/*
		 *  If we haven't responded to the client,
		 *  then respond with no.
		 */
		if (!rp->chat_history[0] && write_no(rp))
			error3("get", rp->algorithm, "write_no() failed");
	}
	return status;
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
	int status;

	rp->verb = "eat";
	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_EAT << 2);
	status = (*mp->eat)(rp);
	if (status) {
		if (status < 0)
			error3("eat", rp->algorithm, "callback eat() failed");
		/*
		 *  If we haven't responded to the client,
		 *  then respond with no.
		 */
		if (write_no(rp))
			error3("eat", rp->algorithm, "write_no() failed");
	} else
		if (write_ok(rp))
			error3("eat", rp->algorithm, "write_ok() failed");
	return status;
}

/*
 *  Synopsis:
 *	Execute the put request by firing the callbacks of the digest module.
 *  Protocol Flow:
 *	>put algorithm:digest\n[blob]	# request for blob
 *	    <ok\n[close]		# server accepts blob, bye
 *          <no\n[close]		# server rejects blob, bye
 */
static int
put(struct request *rp, struct digest_module *mp)
{
	int status;

	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_PUT << 2);
	rp->verb = "put";
	status = (*mp->put)(rp);
	if (status) {
		error3("put", rp->algorithm, "callback put() failed");
		/*
		 *  If we haven't responded to the client,
		 *  then respond with no.
		 */
		if (!rp->chat_history[0] && write_no(rp))
			error3("put", rp->algorithm, "write_no() failed");
	} else if (write_ok(rp))
		error3("put", rp->algorithm, "write_ok() failed");
	return status;
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
 */
static int
take(struct request *rp, struct digest_module *mp)
{
	int status;
	char *reply;

	rp->verb = "take_blob";
	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_TAKE << 2);
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
		struct stat st;
		int status;

		snprintf(wrap_path, sizeof wrap_path, "spool/wrap/%s:%s.brr",
				rp->algorithm, rp->digest);
		status = io_stat(wrap_path, &st);
		if (status == 0) {
			char wrap_udig[MAX_UDIG_SIZE + 1];

			snprintf(wrap_udig, sizeof wrap_udig, "%s:%s",
						rp->algorithm, rp->digest);
			warn3("take", rp->netflow, "blob in unrolled wrap set");
			warn2("take failed", wrap_udig);
			write_no(rp);
			return 1;
		}
		if (errno != ENOENT)
			die3("take: stat() failed", strerror(errno), wrap_path);
	}
	status = (*mp->take_blob)(rp);
	rp->verb = "take";

	if (status) {
		if (status < 0)
			error3("take", rp->algorithm,
						"callback take_blob() failed");
		if (!rp->chat_history[0] && write_no(rp))
			error3("take", rp->algorithm, "write_no(blob) failed");
		return status;
	}
	/*
	 *  Read reply from client.
	 */
	reply = read_reply(rp);
	if (reply == NULL) {
		error3("take", rp->algorithm, "read_reply() failed");
		return -1;
	}
	/*
	 *  Client replied 'no', for some reason, so just shutdown.
	 */
	if (*reply == 'n') {
		warn3("take", rp->algorithm, "client replied no to my ok?");
		/*
		 *  Not sure this is the correct return.
		 */
		return 1;
	}
	/*
	 *  Ok, the client sucessfull took the blob,
	 *  so call the take_ok callback.
	 */
	rp->verb = "take_reply";
	status = (*mp->take_reply)(rp, reply);
	rp->verb = "take";
	if (status) {
		error3("take", rp->algorithm, "callback take_reply() failed");
		if (write_no(rp)) {
			error3("take", rp->algorithm, "write_no(reply) failed");
			return -1;
		}
	}
	else if (write_ok(rp)) {
		error3("take", rp->algorithm, "write_ok() failed");
		return -1;
	}
	return status;
}

/*
 *  Synopsis:
 *	Fire module callbacks for "give" verb in blobio protocol.
 *  Protocol Flow:
 *	>give algorithm:digest\n[blob]	# client sends the blob
 *	    <ok\n			# server accepts the blob
 *	        >ok[close]		# client forgets blob
 *		>no			# client still remembers blob
 *	    <no\n[close]		# server rejects blob request, bye
 */
static int
give(struct request *rp, struct digest_module *mp)
{
	int status;
	char *reply;

	rp->verb = "give";
	request_exit_status = (request_exit_status & 0x1C) |
						(REQUEST_EXIT_STATUS_GIVE << 2);
	status = (*mp->give_blob)(rp);
	/*
	 *  Server can't take blob, so write "no" to client and shutdown.
	 */
	if (status) {
		error3("give", rp->algorithm, "callback give_blob() failed");
		if (write_no(rp))
			error3("give", rp->algorithm, "write_no() failed");
		return status;
	}
	/*
	 *  Server accepted blob, so acknowledge client.
	 */
	if (write_ok(rp)) {
		error3("give", rp->algorithm, "write_ok() failed");
		return -1;
	}
	reply = read_reply(rp);
	if (reply == NULL) {
		error3("give", rp->algorithm, "read_reply() failed");
		return -1;
	}
	/*
	 *  Call give_reply() to handle the reply.
	 *  Any error returned by give_reply() is a panicy situation
	 *  that needs immediate attention.
	 */
	status = (*mp->give_reply)(rp, reply);
	if (status)
		panic3("give", rp->algorithm, "callback give_reply() failed");
	return status;
}

static void
biod(char *verb, char *algorithm, char *digest,
       unsigned char *scan_buf, int scan_size)
{
	int status;
	struct digest_module *mp;
	int (*verb_callback)(struct request *, struct digest_module *);

	verb_callback = 0;		//  quiets clang compiler

	req.verb = verb;
	if (strcmp("wrap", verb) == 0) {
		if (algorithm[0])
			die3_NO(verb, algorithm, "unexpected algorithm");
		strcpy(algorithm, "sha");
	} else if (!algorithm[0] || !digest[0])
		die2_NO(verb, "missing udig");

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
	if (strcmp("get", verb) == 0) {
		ps_title_set("biod-get", (char *)0, (char *)0);
		verb_callback = get;
	} else if (strcmp("put", verb) == 0) {
		ps_title_set("biod-put", (char *)0, (char *)0);
		verb_callback = put;
	} else if (strcmp("take", verb) == 0) {
		ps_title_set("biod-take", (char *)0, (char *)0);
		verb_callback = take;
	} else if (strcmp("give", verb) == 0) {
		ps_title_set("biod-give", (char *)0, (char *)0);
		verb_callback = give;
	} else if (strcmp("eat", verb) == 0) {
		ps_title_set("biod-eat", (char *)0, (char *)0);
		verb_callback = eat;
	} else if (strcmp("wrap", verb) == 0) {
		ps_title_set("biod-wrap", (char *)0, (char *)0);
		verb_callback = wrap;
	} else if (strcmp("roll", verb) == 0) {
		ps_title_set("biod-roll", (char *)0, (char *)0);
		verb_callback = roll;
	} else {
		die3_NO(algorithm, "unknown verb", verb);
		/*NOTREACHED*/
	}

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
 *  Synopsis:
 *	Execute a client blobio request in the child process.
 */
static void
request()
{
	int nread, state;
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

	ps_title_set("biod-request", (char *)0, (char *)0);
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
	 *   [get|put|give|take|eat|roll] [:alpha]{1,15}:[[:isgraph:]]{1,128}]\n
	 *   wrap\n
	 */
	state = STATE_SCAN_VERB;
	while (state != STATE_HALT &&
	      (nread = read_buf(req.client_fd, buf, sizeof buf,
	      	                req.read_timeout)) > 0) {

		unsigned char *b = buf;
		unsigned char *b_end = buf + nread;

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
	if (nread < 0)
		die_NO("read_buf(request) failed");
	if (state == STATE_SCAN_VERB && v_next == verb)
		die_NO("empty read_buf()");

	/*
	 *  Write out the blob request record if the verb halted "normally".
	 */
	if (state == STATE_HALT) {
		char *ch = req.chat_history;
		int len = strlen(req.chat_history);

		switch (len) {
		case 0:
			panic("unexpected empty length of chat history");
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
 *	  ------01	client error	- unknown verb, bad udig syntax
 *	  ------10	hard time out	- usually network timeout, maybe disk
 *	  ------11	fault		- error affecting server stability
 *					  network errors are faults
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
		 *
		 *  Note:
		 *  	How are timeouts handled?
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
		 *  Process exited normally.  Accumlate stats dreived
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
				if (((s8 & 0x60) >> 5) == 
					     REQUEST_EXIT_STATUS_CHAT_NO)
					take_no_count++;
				break;
			case REQUEST_EXIT_STATUS_EAT:
				eat_count++;
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
		if (errno == EINTR || errno == EAGAIN)
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

	snprintf(buf, sizeof buf, "heartbeat: %u sec", HEARTBEAT);
	info(buf);

	/*
	 *  Only burp out message when request count changes.
	 *  A simple tickle of the listen socket will change the request count.
	 */
	if (prev_connect_count == connect_count)
		return;

	snprintf(buf, sizeof buf,
		"connect=%llu, suc=%llu, tmout=%llu, err=%llu, fault=%llu",
		connect_count, success_count, timeout_count,error_count,
			fault_count);
	info(buf);

	snprintf(buf, sizeof buf,
		"get=%llu, put=%llu, give=%llu, take=%llu, eat=%llu",
			get_count, put_count, give_count, take_count,eat_count);
	info(buf);

	snprintf(buf, sizeof buf, "wrap=%llu, roll=%llu",wrap_count,roll_count);
	info(buf);

	snprintf(buf, sizeof buf,
	      "chat: ok=%llu, no[123]=%llu, eat|take no=%llu|%llu",
			chat_ok_count,
			//  u8 could wrap!!!!
			chat_no_count + chat_no2_count + chat_no3_count,
			eat_no_count, take_no_count
	);
	info(buf);

	connect_diff = connect_count - prev_connect_count;
	connect_rate = (float)connect_diff / (float)HEARTBEAT;
	snprintf(buf, sizeof buf, "sample: %.0f connect/sec, connect=%llu",
				connect_rate, connect_diff);
	info(buf);

	prev_connect_count = connect_count;
}

/*
 *  Write out a round robin database sample to log/biod-rrd-sample.log
 *  A cron job is expected to merge the samples into log/bio-stat.rrd
 *  and roll the file log/biod-rrd-sample.log.
 *
 *  Each line sample is suitable as an argument to cron driven rrdupdate.
 *  The sample looks like:
 *
 *	time epoch:
 *		success_count:
 *		timeout_count:
 *		error_count:
 *		fault_count:
 *		signal_count:
 *		get_count:
 *		put_count:
 *		give_count:
 *		take_count:
 *		eat_count:
 *		wrap_count:
 *		roll_count:
 *		chat_ok_count:
 *		chat_no_count:
 *		chat_no2_count:
 *		chat_no3_count:
 *		eat_no_count:
 *		take_no_count
 */
static void
put_rrd_sample()
{
	int fd;
	char buf[512];		/* <= PIPE_MAX */

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
		(u8)time((time_t *)0),
 		success_count,
		timeout_count,
		error_count,
		fault_count,
		signal_count,
		get_count,
		put_count,
		give_count,
		take_count,
		eat_count,
		wrap_count,
		roll_count,
		chat_ok_count,
		chat_no_count,
		chat_no2_count,
		chat_no3_count,
		eat_no_count,
		take_no_count
	);
	if (io_write(fd, buf, strlen(buf)) < 0)
		panic3(rrd_log, "write(rrd log) failed", strerror(errno));
	if (io_close(fd))
		panic3(rrd_log, "close(rrd log) failed", strerror(errno));
}

static void
catch_CHLD(int sig)
{
	time_t now;

	UNUSED_ARG(sig);

	reap_request();
	time(&now);
	if (now - last_log_heartbeat >= HEARTBEAT) {
		heartbeat();
		put_rrd_sample();	//  --rrd-sample-duration == HEARTBEAT
	}
}

#ifdef SIGINFO
/*
 *  Note:
 *  	Add boot message in log file if SIGINFO is defined.
 */
static void
catch_SIGINFO(int sig)
{
	UNUSED_ARG(sig);

	info("caught signal: SIGINFO");
	heartbeat();
}
#endif

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
			addr2text(ntohl(req.bind_address.sin_addr.s_addr)),
			(int)ntohs(req.bind_address.sin_port));
	info(buf);
try_bind:
	if (bind(listen_fd, (const void *)&req.bind_address,
						sizeof req.bind_address) < 0) {
		int e = errno;
		if (e == EINTR || e == EAGAIN)
			goto try_bind;

		snprintf(buf, sizeof buf, "bind(%s:%d) failed",
				addr2text(ntohl(
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

		if (e == EINTR || e == EAGAIN)
			goto try_listen;
		die2("listen() failed", strerror(errno));
	}
}

static void
set_pid_file(char *path)
{
	static char n[] = "set_pid_file";
	char buf[MSG_SIZE];

	switch (is_file(path)) {
	case -1:
		die3(n, "is_file() failed", path);
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
		error3(n, "pid file already exists", pid_path);
		snprintf(buf, sizeof buf,
				"is another biod running with pid=%u", pid);
		die2(n, buf);
	}
	default:
		die3(n, "is_file() returned unexpected value", buf);
	}
	snprintf(buf, sizeof buf, "%d\n", getpid());
	if (burp_text_file(buf, pid_path))
		die3(n, "burp_text_file() failed", pid_path);
	/*
	 *  Chmod run/biod.pid u=r,go=
	 */
	if (io_chmod(pid_path, S_IRUSR))
		panic4(n, "chmod(S_IRUSR) failed", strerror(errno), pid_path);
}

static void 
daemonize(int argc, char **argv)
{
	pid_t pid;
	char buf[MSG_SIZE];
	static char n[] = "daemonize";

	/*
	 *  Root always runs as user blobio.
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
	ps_title_init(argc, argv);

	/*
	 *  Put ourselves in the background.
	 */
	pid = fork();
	if (pid < 0)
		die3(n, "daemonize: fork() failed", strerror(errno));
	if (pid > 0)
		exit(0);	/* original caller */

	master_pid = logged_pid = getpid();

	ps_title_set("biod-listen", (char *)0, (char *)0);

	log_open();
	info("hello, world");

	snprintf(buf, sizeof buf, "logger process id: %u", logger_pid);
	info(buf);

	/*
	 *  Set up posix session.
	 */
	pid = setsid();
	if (pid < 0)
		die3(n, "setsid() failed", strerror(errno));
	snprintf(buf, sizeof buf, "posix session id=%d", pid);
	info(buf);
	set_pid_file(pid_path);

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
 *  Return 1 in the parent (biod) and 0 in the child (the request).
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
	ps_title_set("biod-request", (char *)0, (char *)0);
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
	 *  In the parent - the master biod process - just close down
	 *  the connections and return to accept more requests.
	 */
	if (fork_request()) {
		close(rp->client_fd);
		rp->client_fd = -1;
		return;
	}

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
	snprintf(rp->netflow, sizeof rp->netflow - 1,
		"tcp4~%s:%u;%s:%u",
		addr2text(ntohl(rp->remote_address.sin_addr.s_addr)),
		(unsigned int)ntohs(rp->remote_address.sin_port),
		addr2text(ntohl(rp->bind_address.sin_addr.s_addr)),
		(unsigned int)ntohs(rp->bind_address.sin_port)
	);
		
	request();

	panic2(n, "unexpected return from request()");
}

int
main(int argc, char **argv)
{
	char buf[MSG_SIZE];
	sigset_t mask;
	unsigned short port;
	int i;

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
				rrd_sample_duration = HEARTBEAT;
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
				die3(o, "unexpected seconds", hb);
		} else if (strncmp("--", argv[i], 2) == 0)
			die2("unknown option", argv[i]);
		else
			die2("unknown argument", argv[i]);
	}

	time(&start_time);

	if (port == 0)
		port = BIOD_PORT;

	BLOBIO_ROOT = getenv("BLOBIO_ROOT");
	if (BLOBIO_ROOT == (char *)0)
		die("environment variable BLOBIO_ROOT not defined");
	/*
	 *  Goto $BLOBIO_ROOT directory.
	 */
	if (chdir(BLOBIO_ROOT) < 0) {
		snprintf(buf, sizeof buf, "chdir(%s) failed: %s", BLOBIO_ROOT,
							strerror(errno));
		die(buf);
	}

	daemonize(argc, argv);

	/*
	 *  Calculate current working directory.
	 */
	if (getcwd(buf, sizeof buf) != buf)
		die2("getcwd() failed", strerror(errno));
	info2("current directory", buf);

	if (module_boot())
		die("modules_boot() failed");

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
#ifdef SIGINFO
	if (signal(SIGINFO, catch_SIGINFO) == SIG_ERR)
		panic2("signal(INFO) failed", strerror(errno));
#endif
		
#ifdef SIGPWR
	if (signal(SIGPWR, catch_terminate) == SIG_ERR)
		panic2("signal(PWR) failed", strerror(errno));
#endif
	if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
		panic2("signal(HUP, SIG_IGN) failed", strerror(errno));

	/*
	 *  Open the socket to listen for requests.
	 */
	open_listen(port);
	info("accepting incoming requests ...");
	snprintf(buf, sizeof buf, "socket accept timeout: %u seconds",
						ACCEPT_TIMEOUT);
	info(buf);

	snprintf(buf, sizeof buf, "heartbeat: %u sec", HEARTBEAT);
	info(buf);

	if (rrd_sample_duration > 0) {
		snprintf(buf, sizeof buf, "rrd sample duration: %u sec",
							rrd_sample_duration);
		info(buf);
		info2("rrd log", rrd_log);
	} else
		info("rrd sampling disabled");

	req.verb = 0;
	req.client_fd = -1;
	req.algorithm = 0;
	req.digest = 0;
	req.scan_buf = 0;
	req.scan_size = 0;
	req.open_data = 0;
	req.blob_size = 0;
	req.read_timeout = REQUEST_READ_TIMEOUT;
	req.write_timeout = REQUEST_WRITE_TIMEOUT;
	memset(req.chat_history, 0, sizeof req.chat_history);
	req.remote_len = sizeof req.remote_address;

	/*
	 *  Note:
	 *	The log heartbeat is not lively enough under a heavy load.
	 */
accept_request:
	switch (io_accept(
			listen_fd,
			(struct sockaddr *)&req.remote_address,
			&req.remote_len,
			&req.client_fd,
			ACCEPT_TIMEOUT)) {
	case -1:
		die("io_accept() failed");
		/*NOTREACHED*/
	case 0:
		connect_count++;
		fork_accept(&req);
		break;
	case 1:
		break;
	default:
		panic("io_accept() returned impossible value");
	}
	catch_CHLD(SIGCHLD);

	goto accept_request;
}
