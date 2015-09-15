/*
 *  Synopsis:
 *	Support functions for signal handling.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 *  Note:
 *  	Replace with strsignal()?
 */
#include "biod.h"

static char *names[] =
{
	"ZERO",
	"HUP",
	"INT",
	"QUIT",
	"ILL",
	"TRAP",
	"ABRT",
	"BUS",
	"FPE",
	"KILL",
	"USR1",
	"SEGV",
	"USR2",
	"PIPE",
	"ALRM",
	"TERM",
	"CHLD",
	"CONT",
	"STOP",
	"TSTP",
	"TTIN",
	"TTOU",
	"URG",
	"XCPU",
	"XFSZ",
	"VTALRM",
	"PROF",
	"WINCH",
	"IO",
	"PWR",
	"SYS"
};

static int sig_count = 30;

/*
 *  Synopsis:
 *	Map signal number on to human readable name.
 *  Note:
 *	Unknown signals are formated into a static buffer.
 *	Seems like a sigstr() function should exist in POSIX or Linux.
 */
char *
sig_name(int sig)
{
	static char buf[37];

	if (0 <sig&&sig <= sig_count)
		return names[sig];
	snprintf(buf, sizeof buf, "unknown signal #%d", sig);
	return buf;
}
