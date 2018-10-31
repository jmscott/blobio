/*
 *  Synopsis:
 *	Manage log file for server PANIC|ERROR|WARN|INFO or info messages
 *  Description:
 *	Typical process logging with 4 tagged levels, PANIC|ERROR|WARN,
 *	and one untagged INFO level to a reliable, rolled file.  The levels 
 *	are interpreted as follows:
 *
 *		PANIC:	system error, server is probably going to die
 *		ERROR:	client ERROR
 *		 WARN:	unusual event that does not need immediate attention
 *
 *	The log file is rolled to $BLOBIO_ROOT/log/biod-Dow.log roughly at
 *	midnight in localtime zone.
 *  Note:
 *	Think about replacing the logger process with the postgresql model for
 *	logging.  The postgresql model watches atomic writes to stderr from
 *	each child process.
 *
 *	Really, really need syslog support!
 *
 *	Yosemite shows only biod-logger still connected to a tty?
 *
 *		502 12157 12155   0 10:45AM ttys000    0:00.01 biod-logger
 *
 *	Why?  All other processes are unbound from a terminal.
 *
 *	Should the log_* functions be renamed as buf_*?
 */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "biod.h"

#define LOG_SELECT_TIMEOUT	5		/* seconds */

extern pid_t		master_pid;
extern pid_t		logger_pid;
extern pid_t		request_pid;
extern unsigned char	request_exit_status;
extern time_t		last_log_heartbeat;
extern u2		rrd_sample_duration;
extern time_t		start_time;	
extern char		pid_path[];

pid_t			logged_pid;

static int	log_fd = 2;
static int	log_path_fd = -1;
static char	log_path[MAX_FILE_PATH_LEN] = "log/biod-%s.log";
static char	log_path_format[] = "log/biod-%s.log";
static char	log_dow = -1;
static char	*dow_name[] =
{
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

static int	is_logger = 0;

/*
 *  Note:
 *	After rolling the log file, log elapsed duration since server start up.
 *
 * 		2014/01/29 13:00:48 #27155: elaspsed since start up: 2d4h32m26s
 */
static void
open_log_path(int truncate)
{
	static char n[] = "open_log_path";
	struct tm *now;
	time_t now_t;

	/*
	 *  Shutdown previously opened log file.
	 */
	if (log_path_fd > -1 && log_path_fd != 2) {
		int fd = log_path_fd;
		log_path_fd = -1;

		/*
		 *  Note:
		 *  	Need a log rolling message!
		 */
		if (io_close(fd))
			panic4(n, log_path,"close(log) failed",strerror(errno));
	}

	/*
	 *  Format the time stamp.
	 */
	time(&now_t);
	now = localtime(&now_t);
	if (!now)
		panic3(n, "localtime() returned null", strerror(errno));
	snprintf(log_path, sizeof log_path, log_path_format,
						dow_name[now->tm_wday]);
	/*
	 *  Open the log file.
	 *  If we fail, just write to standard error and croak.
	 */
	if ((log_path_fd = io_open_append(log_path, truncate)) < 0)
		panic4(n, "open(log append) failed", log_path, strerror(errno));
	log_dow = now->tm_wday;
}

char *
server_elapsed()
{
	static char buf[MSG_SIZE];
	char tbuf[64];
	time_t now;
	double elapsed;
	unsigned long e;

	buf[0] = 0;

	time(&now);
	elapsed = difftime(now, start_time); 

	/*
	 *  Days
	 */
	e = elapsed / 86400;
	if (e > 0) {
		snprintf(tbuf, sizeof tbuf, "%lud", e);
		strcat(buf, tbuf);
	}

	/*
	 *  Hours
	 */
	e = ((unsigned long)elapsed % 86400) / 3600;
	if (e > 0) {
		snprintf(tbuf, sizeof tbuf, "%luhr", e);
		strcat(buf, tbuf);
	}

	/*
	 *  Minutes
	 */
	e = ((unsigned long)elapsed % 3600) / 60;
	if (e > 0) {
		snprintf(tbuf, sizeof tbuf, "%lumin", e);
		strcat(buf, tbuf);
	}

	/*
	 *  Seconds
	 */
	e = (unsigned long)elapsed % 60;
	if (e > 0) {
		snprintf(tbuf, sizeof tbuf, "%lusec", e);
		strcat(buf, tbuf);
	}
	return buf;
}


static void
check_log_age()
{
	struct tm *now;
	time_t now_t;

	time(&now_t);
	now = localtime(&now_t);

	if (now->tm_wday != log_dow) {
		char rsd[MSG_SIZE], ppid[MSG_SIZE];

		snprintf(ppid, sizeof ppid,
				"master/parent process id: #%u", getppid());

		snprintf(rsd, sizeof rsd,
				"rrd sample duration: %u", rrd_sample_duration);
		info(rsd);
		info2("elapsed running time", server_elapsed());
		info(ppid); 
		info2("closing log file", log_path);

		open_log_path(1);
		log_fd = log_path_fd;

		info2("opened new log file", log_path);
		info2("elapsed running time", server_elapsed());
		info(rsd);
		info(ppid);
	}
}

/*
 *  The logger process.
 */
static void
logger(int process_fd)
{
	int status;
	unsigned int log_check_count = 0;
	int nwrite;
	fd_set log_fd_set; 
	struct timeval timeout;
	char buf[MSG_SIZE];
	static char n[] = "logger";
	struct io_message process;

	io_msg_new(&process, process_fd);

	/*
	 *
	 *  Loop reading from processes to log file.
	 *  We depend upon atomic write/reads on pipes.
	 */
listen:
	/*
	 *  Check log age every 100 requests or on every timeout.
	 */
	if ((log_check_count++ % 100) == 0)
		check_log_age();

	FD_ZERO(&log_fd_set);
	FD_SET(process_fd, &log_fd_set);
	timeout.tv_sec = LOG_SELECT_TIMEOUT;
	timeout.tv_usec = 0;

	status = io_select(
			process_fd + 1,
			&log_fd_set,
			(fd_set *)0,
			(fd_set *)0,
			&timeout
	);
	if (status == -1)
		panic3(n, "select(process) failed", strerror(errno));

	/*
	 *  Natural timeout of select() ... roll the logs, captain.
	 */
	if (status == 0) {
		check_log_age();
		if (getppid() != master_pid)
			leave(1);
		goto listen;
	}

	/*
	 *  Read log message from some process.
	 */
	status = io_msg_read(&process);
	if (status == -1)
		panic3(n, "io_msg_read(process) failed", strerror(errno));
	/*
	 *  A zero byte io_msg_read() indicates all the writers have closed,
	 *  so just shutdown cleanly.
	 */
	if (status == 0) {
		pid_t parent_pid;

		/*
		 *  Grumble if the parent process is no longer our orginal
		 *  master.  Technically not an error since a sig9 could have
		 *  killed our orginal parent.
		 */
		parent_pid = getppid();
		if (parent_pid != master_pid) {
			snprintf(buf, sizeof buf,
			     "logger: master process id changed: "	\
			     "expected #%u, got #%u",
				     master_pid, parent_pid
			);
			error(buf);
			leave(1);
		}
		leave(0);
	}
	/*
	 *  Write log message to physical file.
	 */
	nwrite = io_write(log_path_fd, process.payload, process.len);
	if (nwrite == process.len)
		goto listen;
	if (nwrite == 0)
		panic2(n, "write(file) empty");
	status = errno;

	io_close(log_path_fd);
	panic4(n, "write(file) failed", log_path, strerror(status));
}

/*
 *  Fork a biod-logger process that reads a stream of log messages from various
 *  processes, writes the messages to the physical log file and rotates the
 *  logs.  The physical file has already been opened.
 */
static void
fork_logger()
{
	int log_pipe[2];
	pid_t pid;
	int process_fd;
	static char n[] = "fork_logger";

	log_fd = log_path_fd;		/* log to physical file */

	if (io_pipe(log_pipe))
		panic3(n, "pipe(request|logger) failed", strerror(errno));

	pid = fork();
	if (pid < 0)
		panic3(n, "fork() failed", strerror(errno));

	/*
	 *  In the parent process, the master biod server, so shutdown
	 *  the physical log file and the read end of the pipe, then
	 *  map the biod process level log file descriptor to the write end
	 *  of the logger pipe.
	 */
	if (pid > 0) {			/* parent, the master biod */
		logger_pid = pid;
		io_close(log_path_fd);
		io_close(log_pipe[0]);
		log_fd = log_pipe[1];
		return;
	}
	is_logger = 1;

	/*
	 *  In the child logger process, so shutdown the writer side
	 *  of the pipeline.
	 */
	io_close(log_pipe[1]);
	process_fd = log_pipe[0];
	logged_pid = getpid();
	ps_title_set("biod-logger", (char *)0, (char *)0);

	logger(process_fd);
}

void
log_open()
{
	if (log_fd > 0 && log_fd != 2)
		panic("log_open: log already open");

	open_log_path(0);
	fork_logger();
}

/*
 *  Panicy situation, since logger is failing.
 *  Reset to stderr and do a quick exit.
 *
 *  Note:
 *  	Do we always know the buffer is ascii?
 */
static void
_panic(char *buf, int len, int nwrite)
{
	static char nl[] = "\n";
	static char colon[] = ": ";
	static char prefix[] = "\nERROR: PANIC: logger: _write: ";
	char *err1 = 0, *err2 = 0;
	char pid[24];

	snprintf(pid, sizeof pid, "%d: ", getpid());

	if (nwrite < 0) {
		err1 = "write() failed";
		err2 = strerror(errno);
	} else if (nwrite == 0)
		err1 = "empty write()";
	else if (nwrite < len)
		err1 = "short write()";

	io_write(2, prefix, sizeof prefix - 1);
	io_write(2, pid, strlen(pid));
	io_write(2, err1, strlen(err1));
	if (err2) {
		io_write(2, colon, 2);
		io_write(2, err2, strlen(err2));
	}
	io_write(2, nl, 1);
	io_write(2, buf, len);
	io_write(2, nl, 1);
	io_unlink(pid_path);
	exit(request_pid ? 3 : 255);
}

/*
 *  Synopsis:
 *	Low level atomic write() of an entire buffer to the physical log file.
 *  Note:
 *	An error when writing to the log file is considered a panicy situation.
 *	The caller's error and the write() error is written to 
 *	standard error (file #2) and the process exits.
 */
static void
_write(char *buf, int len)
{
	int nwrite;

	nwrite = io_write(log_fd, buf, len);
	if (nwrite != len)
		_panic(buf, len, nwrite);
}

char *
log_strcpy2(char *buf, int buf_size, char *msg1, char *msg2)
{
	if (msg1 && *msg1) {
		strncpy(buf, msg1, buf_size);
		if (msg2 && *msg2) {
			strncat(buf, ": ", buf_size - (strlen(buf) + 1));
			strncat(buf, msg2, buf_size - (strlen(buf) + 1));
		}
	} else if (msg2 && *msg2)
		strncpy(buf, msg2, buf_size);
	else
		buf[0] = 0;
	return buf;
}

char *
log_strcat2(char *buf, int buf_size, char *msg1, char *msg2)
{
	if (msg1 && *msg1) {
		if (buf[0])
			strncat(buf, ": ", buf_size - (strlen(buf) + 1));
		strncat(buf, msg1, buf_size - (strlen(buf) + 1));
		if (msg2 && *msg2) {
			strncat(buf, ": ", buf_size - (strlen(buf) + 1));
			strncat(buf, msg2, buf_size - (strlen(buf) + 1));
		}
	} else if (msg2 && *msg2)
		strncpy(buf, msg2, buf_size);
	return buf;
}

char *
log_strcat3(char *buf, int buf_size, char *msg1, char *msg2, char *msg3)
{
	if (msg1 && *msg1) {
		if (buf[0])
			strncat(buf, ": ", buf_size - (strlen(buf) + 1));
		strncat(buf, msg1, buf_size - (strlen(buf) + 1));
		return log_strcat2(buf, buf_size, msg2, msg3);
	}
	return log_strcat2(buf, buf_size, msg2, msg3);
}

char *
log_strcpy3(char *buf, int buf_size, char *msg1, char *msg2, char *msg3)
{
	if (msg1 && *msg1) {
		strncpy(buf, msg1, buf_size);
		return log_strcat2(buf, buf_size, msg2, msg3);
	}
	buf[0] = 0;
	return log_strcpy2(buf, buf_size, msg2, msg3);
}

char *
log_strcpy4(char *buf, int buf_size,
			char *msg1, char *msg2, char *msg3, char *msg4)
{
	if (msg1 && *msg1) {
		strncpy(buf, msg1, buf_size);
		return log_strcat3(buf, buf_size, msg2, msg3, msg4);
	}
	buf[0] = 0;
	return log_strcpy3(buf, buf_size, msg2, msg3, msg4);
}

void
log_format(char *msg, char *buf, int buf_size)
{
	struct tm *t;

	/*
	 *  Format a log time stamp into buf:
	 *
	 *	YYYY/MM/DD hh:mm:ss: #pid
	 *
	 *  On the local timezone.
	 */
	time(&last_log_heartbeat);
	t = localtime(&last_log_heartbeat);

	snprintf(buf, buf_size, "%04d/%02d/%02d %02d:%02d:%02d: #%u: ",
			t->tm_year + 1900,
			t->tm_mon + 1,
			t->tm_mday,
			t->tm_hour,
			t->tm_min,
			t->tm_sec,

			//  Note: why not just getpid()?
			logged_pid == 0 ? getpid() : logged_pid
	);

	/*
	 *  For a non null message, build the entry for the log file
	 *  in a single buffer;  otherwise, for a null message get panicy.
	 */
	if (msg && *msg) {
		strncat(buf, msg, buf_size - (strlen(buf) + 1));
		strncat(buf, "\n", buf_size - (strlen(buf) + 1));
	}
	else
		strncat(buf, ": PANIC: info(null parameter)\n",
						buf_size - (strlen(buf) + 1));
}

void
info(char *msg)
{
	char buf[MSG_SIZE];
	int len;
	
	log_format(msg, buf, sizeof buf);
	len = strlen(buf);
	if (is_logger) {
		_write(buf, len);
	} else if (logger_pid) {
		if (io_msg_write(log_fd, buf, len))
			_panic(buf, len, -1);
	} else if (io_write(log_fd, buf, strlen(buf)) < 0)
		_panic(buf, len, -1);
}

/*
 *  Request to shutdown the log file by writing the log message with a leading
 *  0 byte.  The 0 byte requests the logger to shutdown gracefully.
 *  Can only be called from the master biod process.
 *
 *  Note:
 *  	Need to disable SIGCHLD.
 */
void
log_close()
{
	if (getpid() == master_pid) {
		io_close(log_fd);
		log_fd = 2;
	}
}

void
info2(char *msg1, char *msg2)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		info(buf);
	} else
		info(msg2);
}

void
info3(char *msg1, char *msg2, char *msg3)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		info2(buf, msg3);
	} else
		info2(msg2, msg3);
}

void
info4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		info3(buf, msg3, msg4);
	} else
		info3(msg2, msg3, msg4);
}

void
error(char *msg)
{
	/*
	 *  In request?
	 */
	if (request_pid && (request_exit_status & 0x3) == 0)
		request_exit_status = (request_exit_status & 0xFC) |
						REQUEST_EXIT_STATUS_ERROR;
	info2("ERROR", msg);
}

void
error2(char *msg1, char *msg2)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		error(buf);
	} else
		error(msg2);
}

void
error3(char *msg1, char *msg2, char *msg3)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		error2(buf, msg3);
	} else
		error2(msg2, msg3);
}

void
error4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		error3(buf, msg3, msg4);
	} else
		error3(msg2, msg3, msg4);
}

void
error5(char *msg1, char *msg2, char *msg3, char *msg4, char *msg5)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		error4(buf, msg3, msg4, msg5);
	} else
		error4(msg2, msg3, msg4, msg5);
}

void
panic(char *msg1)
{
	if (log_fd != 2) {
		char msg[MSG_SIZE];

		log_format(msg1, msg, sizeof msg);
		write(2, "\n", 1);
		write(2, msg, strlen(msg));
		write(2, "\n", 1);
	}
	error2("PANIC", msg1);
	if (request_pid) {
		/*
		 *  Force a panic in the child request to be a fault.
		 */
		request_exit_status = (request_exit_status & 0xFC) |
						REQUEST_EXIT_STATUS_FAULT;
		leave((int)request_exit_status);
	}
	if (is_logger) {
		log_fd = 2;
		logger_pid = 0;
	}
	leave(1);
}

void
panic2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	panic(log_strcpy2(buf, sizeof buf, msg1, msg2));
}

void
panic3(char *msg1, char *msg2, char *msg3)
{
	char buf[MSG_SIZE];

	panic(log_strcpy3(buf, sizeof buf, msg1, msg2, msg3));
}

void
panic4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	char buf[MSG_SIZE];

	panic(log_strcpy4(buf, sizeof buf, msg1, msg2, msg3, msg4));
}

void
warn(char *msg)
{
	info2("WARN", msg);
}

void
warn2(char *msg1, char *msg2)
{
	char buf[MSG_SIZE];

	warn(log_strcpy2(buf, sizeof buf, msg1, msg2));
}

void
warn3(char *msg1, char *msg2, char *msg3)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy2(buf, sizeof buf, msg1, msg2);
		warn2(buf, msg3);
	} else
		warn2(msg2, msg3);
}

void
warn4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	if (msg1 && *msg1) {
		char buf[MSG_SIZE];

		log_strcpy3(buf, sizeof buf, msg1, msg2, msg3);
		warn2(buf, msg4);
	} else
		warn3(msg2, msg3, msg4);
}
