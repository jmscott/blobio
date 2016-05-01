/*
 *  Synopsis:
 *  	Trace/debug service flow with --trace command line option.
 *  Note:
 *	Only trace.c refers to stdio, I (jmscott) believe.
 */
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>

#include "blobio.h"

extern char	*verb;
int		tracing = 0;

static int	trace_fd = 2;

void
trace(char *msg)
{
	char buf[PIPE_MAX];

	buf[0] = 0;

	bufcat(buf, sizeof buf, "TRACE: ");
	if (verb)
		buf2cat(buf, sizeof buf, verb, ": ");
	if (msg)
		bufcat(buf, sizeof buf, msg);
	bufcat(buf, sizeof buf, "\n");

	uni_write(trace_fd, buf, strlen(buf));
}

void
trace2(char *msg1, char *msg2)
{

	if (msg1) {
		char buf[PIPE_MAX];

		buf[0] = 0;
		bufcat(buf, sizeof buf, msg1);
		if (msg2)
			buf2cat(buf, sizeof buf, ": ", msg2);
		trace(buf);
	} else
		trace(msg2);
}

void
trace3(char *msg1, char *msg2, char *msg3)
{
	if (msg1) {
		char buf[PIPE_MAX];

		buf[0] = 0;
		bufcat(buf, sizeof buf, msg1);
		if (msg2)
			buf2cat(buf, sizeof buf, ": ", msg2);
		trace2(buf, msg3);
	} else 
		trace2(msg2, msg3);
}

void
trace4(char *msg1, char *msg2, char *msg3, char *msg4)
{
	if (msg1) {
		char buf[PIPE_MAX];

		buf[0] = 0;
		bufcat(buf, sizeof buf, msg1);
		if (msg2)
			buf2cat(buf, sizeof buf, ": ", msg2);
		trace3(buf, msg3, msg4);
	} else 
		trace3(msg2, msg3, msg4);
}
/*
 *  Synopsis:
 *	Format a byte buffer into ascii/hex suitable for debuging network flow.
 *  Example:
 *
     0  >  67 65 74 20 70 69 6e 67 3a 61 62 63 64 65 66 0a  get ping:abcdef.   
    16  >  3a 35 34 20 43 44 54 09 31 32 37 2e 30 2e 30 2e  :54 CDT.127.0.0.
    32  >  01 02 03                                         ... 

0.........1.........2.........3.........4.........5.........6.........7.........

 *	Each formattedd line is 76 chars plus new line.
 *	The characters '>' and '<' represent either read or write direction,
 *	respectively. 
 *
 *  Note:
 *	If the source buffer requires more space than the target buffer
 *	can provide, then none of the source buffer is formated,
 *	which is hosed.  This is a mistake.  Instead, the source
 *	should be truncated and a truncation message should be written
 *	to the target.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */

static void
_write(char *buf)
{
	uni_write(trace_fd, buf, strlen(buf));
}

static short
_cksum(unsigned char *buf, int size)
{
	unsigned short sum;
	int i;

	for (i = 0, sum = 0;  i < size;  i++)
		sum += i * buf[i];
	return sum;
}

/*
 *  Ye'ol hex dump.
 *
 *  Derived from a hexdump written by Andy Fullford, eons ago.
 *
 *  	https://github.com/akfullfo
 */
void
hexdump(unsigned char *src, int src_size, char direction)
{
	char *t;
	unsigned char *s, *s_end;
	int need;
	char tbuf[64 * 1024], *tgt;
	int tgt_size;
	char buf[1024];

	if (src_size == 0) {
		static char zero[] = "hexdump: 0 bytes in source buffer\n";
		_write(zero);
		return;
	}

	tgt = tbuf;
	tgt_size = sizeof tbuf;

	static char hexchar[] = "0123456789abcdef";

	/*
	 *  Insure we have plenty of room in the target buffer.
	 *
	 *  Each line of ascii output formats up to 16 bytes in the source
	 *  buffer, so the number of lines is the ceiling(src_size/16).
	 *  The number of characters per line is 76 characters plus the 
	 *  trailing new line.
	 *
	 *	((src_size - 1) / 16 + 1) * (76 + 1) + 1
	 *
	 *  with the entire ascii string terminated with a null byte.
	 */
	need = ((src_size - 1) / 16 + 1) * (76 + 1) + 1;

	/*
	 *  Not enough space in the target buffer ... probably ought
	 *  to truncate instead of punting.
	 */
	if (need > tgt_size) {
		snprintf(tgt, tgt_size,
			"hexdump: source bigger than target: %d > %d\n", need,
				tgt_size);
		_write(tgt);
		return;
	}

	s = src;
	s_end = s + src_size;
	t = tgt;

	while (s < s_end) {
		int i;

		/*
		 *  Start of line in the target.
		 */
		char *l = t;

		/*
		 *  Write the offset into the source buffer.
		 */
		sprintf(l, "%6lu", (long unsigned)(s - src));

		/*
		 *  Write direction of flow
		 */
		l[6] = l[7] = ' ';
		l[8] = direction;
		l[9] = ' ';

		/*
		 *  Format up to next 16 bytes from the source.
		 */
		for (i = 0;  s < s_end && i < 16;  i++) {
			l[10 + i * 3] = ' ';
			l[10 + i * 3 + 1] = hexchar[(*s >> 4) & 0x0F];
			l[10 + i * 3 + 2] = hexchar[*s & 0x0F];
			if (isprint(*s))
				l[60 + i] = *s;
			else
				l[60 + i] = '.';
			s++;
		}
		/*
		 *  Pad out last line with white space.
		 */
		while (i < 16) {
			l[10 + i * 3] =
			l[10 + i * 3 + 1] =
			l[10 + i * 3 + 2] = ' ';
			l[60 + i] = ' ';
			i++;
		}
		l[58] = l[59] = ' ';
		l[76] = '\n';
		t = l + 77;
	}
	tgt[need - 1] = 0;

	snprintf(buf, sizeof buf,
			"\ndump of %d bytes, cksum %hu\n",
				src_size, _cksum(src, src_size));
	_write(buf);
	_write(tgt);
	_write("\n");
}
