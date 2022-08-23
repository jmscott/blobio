/*
 *  Synopsis:
 *	Functions related to blobi request records (brr).
 */
#include <limits.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#include "jmscott/libjmscott.h"
#include "blobio.h"

#define BRR_SIZE	370		//  terminating null NOT counted

extern char			BR[];
extern struct timespec		start_time;
extern void			die(char *msg);
extern void			die2(char *msg1, char *msg2);
extern int			errno;
extern char			algorithm[];
extern char			algo[];
extern char			ascii_digest[];
extern char			transport[];
extern char			verb[];
extern char			chat_history[];
extern unsigned long long	blob_size;

static char		*brr_format =
	"%04d-%02d-%02dT%02d:%02d:%02d.%09ld+00:00\t"	//  RFC3339Nano time
	"%s\t"						//  transport
	"%s\t"						//  verb
	"%s%s%s\t"					//  algorithm(:digest)?	
	"%s\t"						//  chat history
	"%llu\t"					//  byte count
	"%ld.%09ld\n"					//  wall duration
;

void
brr_write(char *srv_name)
{
	struct tm *t;
	char brr[BRR_SIZE + 1];
	struct timespec	end_time;
	long int sec, nsec;

	TRACE2("service name", srv_name);

	/*
	 *  Build the ascii version of the start time.
	 */
	t = gmtime(&start_time.tv_sec);
	if (!t)
		die2("gmtime() failed", strerror(errno));
	/*
	 *  Get end time.
	 */
	if (clock_gettime(CLOCK_REALTIME, &end_time) < 0)
		die2("clock_gettime(end REALTIME) failed", strerror(errno));
	/*
	 *  Calculate the elapsed time in seconds and
	 *  nano seconds.  The trick of adding 1000000000
	 *  is the same as "borrowing" a one in grade school
	 *  subtraction.
	 */
	if (end_time.tv_nsec - start_time.tv_nsec < 0) {
		sec = end_time.tv_sec - start_time.tv_sec - 1;
		nsec = 1000000000 + end_time.tv_nsec - start_time.tv_nsec;
	} else {
		sec = end_time.tv_sec - start_time.tv_sec;
		nsec = end_time.tv_nsec - start_time.tv_nsec;
	}

	/*
	 *  Verify that seconds and nanoseconds are positive values.
	 *  This test is added until I (jmscott) can determine why
	 *  I peridocally see negative times on Linux hosted by VirtualBox
	 *  under Mac OSX.
	 */
	if (sec < 0)
		sec = 0;
	if (nsec < 0)
		nsec = 0;

	/*
	 *  Format the record buffer.
	 */
	snprintf(brr, BRR_SIZE + 1, brr_format,
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		start_time.tv_nsec,
		transport,
		verb,
		algorithm[0] ? algorithm : "",
		algorithm[0] && ascii_digest[0] ? ":" : "",
		ascii_digest[0] ? ascii_digest : "",
		chat_history,
		blob_size,
		sec, nsec
	);

	/*
	 *  Open local brr file with shared lock, not exclusive.
	 *  Shared does not conflict with other brr writers, cause
	 *  sizeof(brr tuple) are <= insured atomic writes.
	 *
	 *  Note:
	 *	verify that brr file is NOT a sym link
	 */
	char brr_path[PATH_MAX];

	brr_path[0] = 0;
	jmscott_strcat4(brr_path, sizeof brr_path,
		BR,
		"/",
		srv_name,
		".brr"
	);
	int fd = jmscott_open(
		brr_path,
		O_WRONLY|O_CREAT|O_APPEND|O_SHLOCK,
		S_IRUSR|S_IWUSR|S_IRGRP
	);
	if (fd < 0)
		die2("open(brr-path) failed", strerror(errno));
	/*
	 *  Write the entire blob request record in a single write().
	 */
	if (jmscott_write(fd, brr, strlen(brr)) < 0)
		die2("write(brr-path) failed", strerror(errno));
	if (jmscott_close(fd) < 0)
		die2("close(brr-path) failed", strerror(errno));
}
