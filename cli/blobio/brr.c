/*
 *  Synopsis:
 *	Functions related to blobi request records (brr).
 */
#include <limits.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>		//  quiets gcc and wierd tls link error

#include "jmscott/libjmscott.h"
#include "blobio.h"

#define BRR_SIZE	371 + 1 + 1	//  brr size + new-line + null

extern struct timespec		start_time;
extern void			die(char *msg);
extern void			die2(char *msg1, char *msg2);
extern int			errno;
extern long long		blob_size;

static char		*brr_format =
	"%04d-%02d-%02dT%02d:%02d:%02d.%09ld+00:00\t"	//  RFC3339Nano time
	"%s\t"						//  transport
	"%s\t"						//  verb
	"%s\t"						//  udig
	"%s\t"						//  chat history
	"%lld\t"					//  byte count
	"%ld.%09ld\n"					//  wall duration
;

char *
brr_mask2ascii(unsigned char mask)
{
	static char ascii[5];
	static char hexmap[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
	};

	ascii[0] = '0';
	ascii[1] = 'x';

	ascii[2] = hexmap[(mask >> 4) & 0x0f];
	ascii[3] = hexmap[mask & 0x0f];
	ascii[4] = 0;

	return ascii;
}

/*
 *  Is a particular verb set in the brr mask?
 *
 *	Bit 	Verb
 *	---	----
 *
 *	1	"get"
 *	2	"take"
 *	3	"put"
 *	4	"give"
 *	5	"eat"
 *	6	"wrap"
 *	7	"roll"
 *	8	"cat"
 */

int
brr_mask_is_set(char *verb, unsigned char mask)
{
	switch (verb[0])
	{
	case 'c':
		return (mask & 0x80) ? 1 : 0;		// verb "cat"
	case 'e':
		return (mask & 0x10) ? 1 : 0;		// verb "eat"
	case  'g':
		if (verb[1] == 'e')
			return (mask & 0x01) ? 1 : 0;	//  verb "get"
		return (mask & 0x08) ? 1 : 0;		//  verb "give"
	case 'p':
		return (mask & 0x04) ? 1 : 0;		//  verb "put"
	case 'r':
		return (mask & 0x40) ? 1 : 0;		//  verb "roll"
	case 't':
		return (mask & 0x02) ? 1 : 0;		//  verb "take"
	case 'w':
		return (mask & 0x20) ? 1 : 0;		//  verb "wrap"
	}
	return 0;
}

char *
brr_service(struct service *srv)
{
	struct brr brr = {0};
	struct tm *t;
	struct timespec	end_time;
	long int sec, nsec;

	TRACE("entered");

	brr.start_time = start_time;

	brr.udig[0] = 0;
	jmscott_strcat(brr.udig, sizeof brr.udig, udig);

	brr.verb[0] = 0;
	jmscott_strcat(brr.verb, sizeof brr.verb, verb);

	brr.transport[0] = 0;

	brr.chat_history[0] = 0;
	jmscott_strcat(brr.chat_history, sizeof brr.chat_history, chat_history);

	//  verify blob sizes are consistent

	if (!digest_module)
		return "impossible empty digest module";
	if (digest_module->empty()) {
		if (blob_size != 0)
			return "empty blob has size > 0";
	} else if (blob_size == 0 && strchr("gtp", verb[0])) {
		size_t len = strlen(chat_history);
		if (len == 0)
			return "zero length chat history";
		if (chat_history[len - 1] == 'k')
			return "ok: blob size == 0 for non empty blob";
	}
	brr.blob_size = blob_size;

	char *err = srv->brr_frisk(&brr);
	if (err)
		return err;

	if (brr.log_fd < 0)
		return "log file < 0";
	if (!brr.verb[0])
		return "variable is empty: verb";
	if (!brr.transport[0])
		return "variable is empty: transport";
	if (!brr.chat_history[0])
		return "variable is empty: chat_history";
	if (!brr.udig[0]) {
		if (brr.verb[0] != 'w' || brr.chat_history[0] != 'n')
			return "variable is empty: udig";
	}

	/*
	 *  Build the ascii version of the start time.
	 */
	t = gmtime(&brr.start_time.tv_sec);
	if (!t)
		return strerror(errno);
	/*
	 *  Get end time for wall duration.
	 */
	if (clock_gettime(CLOCK_REALTIME, &end_time) < 0)
		return strerror(errno);
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
	 *  peridocally negative times occur on Linux hosted by VirtualBox
	 *  under Mac OSX.
	 */
	if (sec < 0) {
		sec = 0;
		nsec = 0;
	}
	if (nsec < 0)
		nsec = 0;

	//  cheap sanity tests
	/*
	 *  Write the record to log_fd.
	 */
	if (jmscott_flock(brr.log_fd, LOCK_EX))
		return strerror(errno);
	if (dprintf(brr.log_fd, brr_format,
		t->tm_year + 1900,
		t->tm_mon + 1,
		t->tm_mday,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		start_time.tv_nsec,
		brr.transport,
		brr.verb,
		brr.udig,
		brr.chat_history,
		brr.blob_size,
		sec,
		nsec
	) < 0)
		err = strerror(errno);
	if (jmscott_flock(brr.log_fd, LOCK_UN) && !err)
		return strerror(errno);
	return err;
}
