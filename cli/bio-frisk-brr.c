/*
 *  Synopsis:
 *	Is a stream of data a well formed brr log file: yes or no or empty?
 *  Description:
 *  	A blob request record log file consists of lines of ascii text
 *  	with 7 tab separated fields matching
 *
 *		start time: YYYY-MM-DDTHH:MM:SS.NS9(([-+]hh:mm)|Z)
 *		\t
 *		transport: matches perl5 re:
 *
 *			[a-z][a-z0-9]{,7}~[[:graph:]]{1,128})
 *
 *		\t
 *		verb: get|put|give|take|eat|wrap|roll
 *		\t
 *		udig: [a-z][a-z0-9]{7}:hash-digest{1,128}
 *		\t
 *		chat history:
 *			ok|no			all verbs
 *
 *			ok,ok|ok,no		when verb is "give" or "take"
 *
 *			ok,ok,ok|ok,ok,no	only "take"
 *		\t
 *		byte count: 0 <= 2^63 - 1
 *		\t
 *		wall duration: sec.ns where
 *			       0<=sec&&sec <= 2^31 -1 && 0<=ns&&ns <=999999999
 *  Exit:
 *  	0	-> is a brr log
 *  	1	-> is not a brr log
 *	2	-> is empty
 *  	3	-> unexpected error
 *  Blame:
 *  	jmscott@setspace.com
 *  Note:
 *	Would be interesting to compare this code to a versiosn matching
 *	with posix regexp library (and perl lib).
 *
 *	Would be nice to remove dependency upon stdio library.
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>

#include "jmscott/libjmscott.h"

char *jmscott_progname =	"bio-frisk-brr";

static char digits[] = "0123456789";
static char lower_alnum[] = "abcdefghijklmnopqrstuvwxyz0123456789";

/*
 *  ASCII graphical characters allowed in brr transport and udig.
 */
static char graph_set[] =
		"!\"#$%&'()*+,-./"	//  0x32 -> 0x47
		"0123456789:;<=>?"	//  0x48 -> 0x63
		"@ABCDEFGHIJKLMNO"	//  0x64 -> 0x79
		"PQRSTUVWXYZ[\\]^_"	//  0x50 -> 0x5f
		"`abcdefghijklmno"	//  0x60 -> 0x6f
		"pqrstuvwxyz{|}~"	//  0x70 -> 0x7e
;

static void
die(char *msg)
{
	jmscott_die(3, msg);
}

/*
 *  Verify that exactly the first 'n' characters in a string are in a 
 *  set.  If a char is not in the set, then exit this process falsely;
 *  otherwise, reset the source pointer to src + n;
 */
static void 
in_set(char **src, int n, char *set)
{
	char *s, *s_end;

	s = *src;
	s_end = s + n;
	while (s < s_end) {
		char c = *s++;

		if (c == 0 || strchr(set, c) == NULL)
			exit(1);
	}
	*src = s_end;
}
/*
 *  Verify that a range of characters in a string are in a 
 *  set.  If a char is not in the set, then exit this process falsely;
 *  otherwise, reset the source pointer to first char not in the set.
 */
static void 
in_range(char **src, int upper, int lower, char *set)
{
	char *s, *s_end;

	s = *src;
	s_end = s + upper;
	while (s < s_end) {
		char c = *s++;

		if (strchr(set, c) != NULL)
			continue;
		if (s - *src <= lower)	//  too few characters
			exit(1);
		--s;
		break;
	}
	*src = s;
}

/*
 *  Scan up to 'limit' chars that are in set
 *  and set src to first char after the end char.
 *  non-ascii always fails.
 *
 *  Note:
 *	what happens when zero chars are in the set?
 */
static void 
scan_set(char **src, int limit, char *set, char end)
{
	char *s, *s_end;

	s = *src;
	s_end = s + limit;
	while (s < s_end) {
		char c = *s++;

		if (!isascii(c))
			exit(1);
		if (c == end)
			break;
		if (strchr(set, c) == NULL)
			exit(1);
	}
	*src = s;
}

/*
 *  Match RFC3339Nano time stamp:
 *
 *	2010-12-03T04:47:05.123456789+00:01
 *	2010-12-03T04:47:05.123456789-00:03
 *	2010-12-03T04:47:05.123456789Z
 */
static void
scan_rfc399nano(char **src)
{
	char *p = *src;

	/*
	 *  Year:
	 *	match [2345]YYY
	 */
	if ('2' > *p || *p > '5')
		exit(1);
	p++;
	in_set(&p, 3, digits);

	/*
	 *  Month:
	 *	match -[01][0-9]
	 */
	if (*p++ != '-')
		exit(1);
	if (*p != '0' && *p != '1')
		exit(1);
	p++;
	if ('0' > *p || *p > '9')
		exit(1);
	p++;

	/*
	 *  Day:
	 *	match -[0123][0-9]
	 */
	if (*p++ != '-')
		exit(1);
	if ('0' > *p || *p > '3')
		exit(1);
	p++;
	if ('0' > *p || *p > '9')
		exit(1);
	p++;

	/*
	 *  Hour:
	 *	match T[012][0-9]
	 */
	if (*p++ != 'T')
		exit(1);
	if ('0' > *p || *p > '2')
		exit(1);
	p++;
	if ('0' > *p || *p > '9')
		exit(1);
	p++;

	/*
	 *  Minute:
	 *	match :[0-5][0-9]
	 */
	if (*p++ != ':')
		exit(1);
	if ('0' > *p || *p > '5')
		exit(1);
	p++;
	if ('0' > *p || *p > '9')
		exit(1);
	p++;

	/*
	 *  Second:
	 *	match :[0-5][0-9]
	 */
	if (*p++ != ':')
		exit(1);
	if ('0' > *p || *p > '5')
		exit(1);
	p++;
	if ('0' > *p || *p > '9')
		exit(1);
	p++;
	if (*p++ != '.')
		exit(1);
	in_range(&p, 9, 1, digits);

	switch (*p++) {
	case 'Z':
		break;
	case '+': case '-':
		in_set(&p, 2, digits);
		if (*p++ != ':')
			exit(1);
		in_set(&p, 2, digits);
		break;
	default:
		exit(1);
	}
	if (*p++ != '\t')
		exit(1);
	*src = p;
}

/*
 *  Scan the transport field which matches this perl5 regex:
 *
 *	[a-z][a-z0-9]{0,7}~([[:graph:]{1,128})
 */
static void
scan_transport(char **src)
{
	char c, *p = *src;

	/*
	 *  Scan the trqnsport field which matches this perl5 regex:
	 *
	 *	[a-z][a-z0-9]{0,7}~([[:graph:]{1,128})
	 */
	c = *p++;
	if ('a' > c || c > 'z')
		exit(1);

	scan_set(&p, 7, lower_alnum, '~');

	//  Note: does at least one char exist?
	scan_set(&p, 128, graph_set, '\t');

	*src = p;
}

/*
 *  Blobio verb: get|put|give|take|eat|what.
 */
static char *
scan_verb(char **src)
{
	char c, *p = *src, *verb;

	c = *p++;
	if (c == 'g') {			/* get|give */
		c = *p++;
		if (c == 'e') {			/* get */
			if (*p++ != 't')
				exit(1);	/* give */
			verb = "get";

		} else if (c == 'i') {
			if (*p++ != 'v' || *p++ != 'e')
				exit(1);
			verb = "give";
		} else
			exit(1);
	} else if (c == 'p') {		/* put */
		if (*p++ != 'u' || *p++ != 't')
			exit(1);
		verb = "put";
	} else if (c == 't') {		/* take */
		if (*p++ != 'a' || *p++ != 'k' || *p++ != 'e')
			exit(1);
		verb = "take";
	} else if (c == 'e') {		/* eat */
		if (*p++ != 'a' || *p++ != 't')
			exit(1);
		verb = "eat";
	} else if (c == 'w') {		/* wrap */
		if (*p++ != 'r' || *p++ != 'a' || *p++ != 'p')
			exit(1);
		verb = "wrap";
	} else if (c == 'r') {		/* roll */
		if (*p++ != 'o' || *p++ != 'l' || *p++ != 'l')
			exit(1);
		verb = "roll";
	} else
		exit(1);
	if (*p++ != '\t')
		exit(1);
	*src = p;
	return verb;
}
/*
 *  Scan the uniform digest that looks like:
 *
 *  	algorithm:hash-digest
 */
static void
scan_udig(char **src)
{
	char *p = *src;

	/*
	 *  First char of algorithm must be lower case alpha.
	 */
	if ('a' > *p || *p > 'z')
		exit(1);
	p++;
	/*
	 *  Up to 7 alphnum chars before colon.
	 */
	scan_set(&p, 7, lower_alnum, ':');
	scan_set(&p, 128, graph_set, '\t');
	*src = p;
}

/*
 *  Chat History:
 *	ok		->	verb in {get,put,eat,wrap,roll}
 *	ok,ok		->	verb in {give}
 *	ok,ok,ok	->	verb in {take}
 *	no		->	verb in	{
 *					get,put,eat,
 *					give,take,
 *					wrap,roll
 *				}
 *	ok,no		->	verb in {give,take}
 *	ok,ok,no	->	verb in {take}
 *
 *  All chat histories start with character 'o' or 'n'.
 *  Scan the chat history, verifying the reposnse
 *  is correct for the verb.
 */
static void
scan_chat_history(char *verb, char **src)
{
	char *p, *end, ch[9], v1, v2;
	int len;

	p = *src;
	end = p;

	scan_set(&end, 8, "nok,", '\t');
	len = end - p - 1;
	if (len < 2)
		exit(1);
	memcpy(ch, p, len);
	ch[len] = 0;

	/*
	 *  Konwing the verb is valid (get/put/give/take/eat/wrap/roll),
	 *  implies that the first two chars determine uniquess of verb.
	 */
	v1 = verb[0];
	v2 = verb[1];

	if (strcmp("no", ch) == 0)	//  any verb can reply "no"
		goto done;
	if (strcmp("ok", ch) == 0) {
		if (
			//  "get"
			(v1 == 'g' && v2 == 'e')		||

			//  "eat"
			v1 == 'e'				||

			//  "wrap"
			v1 == 'w'				||

			//  "roll"
			v1 == 'r'
		)
			goto done;
		exit(1);
	}

	/*
	 *  chat history must start with "ok,"
	 *
	 *  Note:
	 *	One day i (jmscott) will optimize away strcmp()
	 */

	/*
	 *  Verbs that reply with: ok,ok or ok,no: give, take, put.
	 */
	if (strcmp("ok,ok", ch) == 0 || strcmp("ok,no", ch) == 0) {
		if (
			//  "give"
			(v1 == 'g' && v2 == 'i')			||

			//  "take"
			v1 == 't'					||

			//  "put"
			v1 == 'p'
		)
			goto done;
		exit(1);
	}

	/*
	 *  Only verbs "give" and "take" can do "ok,ok,ok" or "ok,ok,no".
	 */
	if (
		(strcmp("ok,ok,ok",ch)==0 || strcmp("ok,ok,no", ch)==0)	&&
		(v1=='t' || (v1=='g' && v1=='i'))
	)
		goto done;
	exit(1);
done:
	*src = end;
}

/*
 *  Scan for an unsigned int <= 2^63 -1.
 *
 *  Note:
 *  	Need a compile time pragma to insure the
 *
 *  		sizeof long long int == 8
 */
static void
scan_byte_count(char **src)
{
	char *p, *q;
	long long v;

	p = *src;
	q = p;

	scan_set(&p, 21, digits, '\t');

	errno = 0;
	v = strtoll(q, (char **)0, 10);

	if ((v == LLONG_MAX && errno == ERANGE) ||
	    (LLONG_MAX > 9223372036854775807 &&
	     v > 9223372036854775807))
		exit(1);
	*src = p;
}

/*
 *  Duration: seconds.nsecs, where seconds ~ /^\{1,9}$/
 *  	      and nsecs ~ /^\d{9}$/
 *  	      and seconds <= 2147483647
 */
static void
scan_duration(char **src)
{
	char *p, *q;
	long v;

	p = *src;
	q = p;

	scan_set(&p, 10, digits, '.');

	errno = 0;
	v = strtoll(q, (char **)0, 10);

	if ((v == LONG_MAX && errno == ERANGE) ||
	    (LONG_MAX > 2147483647 && v > 2147483647))
		exit(1);
	in_range(&p, 9, 1, digits);
	if (*p++ != '\n')
		exit(1);
	*src = p;
}

int
main()
{
	char buf[10 * 1024], *p;
	char *verb;
	int read_input = 0;

	/*
	 *  Note:
	 *	Please replace with read().
	 */
	while (fgets(buf, sizeof buf, stdin)) {
		read_input = 1;

		p = buf;

		//  start time of request
		scan_rfc399nano(&p);

		scan_transport(&p);

		verb = scan_verb(&p);

		scan_udig(&p);

		scan_chat_history(verb, &p);

		//  byte count
		scan_byte_count(&p);

		//  wall duration
		scan_duration(&p);
	}
	if (ferror(stderr))
		die("ferror(stdin) returned true");
	if (read_input)
		exit(feof(stdin) ? 0 : 1);

	/*
	 *  Empty set.
	 */
	exit(2);
}
