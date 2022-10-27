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
 *	Replace eexit(func) with macro SCAN_EXIT() that references _funcname
 *
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

static int			tracing = 0;
static unsigned long long	line_no = 0;

static char line[371 + 1] = {};

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
eexit(char *what)
{
	if (tracing) {
		if (line_no == 0)
			jmscott_die(1, what);

		char u64_digits[21];

		*jmscott_ulltoa(line_no, u64_digits) = 0;
		jmscott_die4(1, what, "line number", u64_digits, line);
	}
	exit(1);
}

static void
die(char *msg)
{
	if (tracing) {
		if (line_no == 0)
			jmscott_die(3, msg);

		char u64_digits[21];

		*jmscott_ulltoa(line_no, u64_digits) = 0;
		jmscott_die4(1, msg, "line number", u64_digits, line);
	}
	jmscott_die(3, msg);
}

static void
die2(char *msg1, char *msg2)
{
	if (tracing) {
		if (line_no == 0)
			jmscott_die2(3, msg1, msg2);

		char u64_digits[21];

		*jmscott_ulltoa(line_no, u64_digits) = 0;
		jmscott_die5(1, msg1, msg2, "line number", u64_digits, line);
	}
	jmscott_die2(3, msg1, msg2);
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
			eexit("in_set");
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
			eexit("in_range");
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
			eexit("scan_set");
		if (c == end)
			break;
		if (strchr(set, c) == NULL)
			eexit("scan_set");
	}
	*src = s;
}

static void
err_399(char *what)
{
	char msg[JMSCOTT_ATOMIC_WRITE_SIZE];

	msg[0] = 0;
	snprintf(msg, sizeof msg, "scan_rfc399nano: %s", what);
	eexit(msg);
}

static void
err_399r(char *what)
{
	char msg[JMSCOTT_ATOMIC_WRITE_SIZE];

	msg[0] = 0;
	snprintf(msg, sizeof msg, "digit out of range: %s", what);
	err_399(msg);
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
		err_399r("year: first either < 2 or > 5");
	p++;
	in_set(&p, 3, digits);

	/*
	 *  Month:
	 *	match -[01][0-9]
	 */
	if (*p++ != '-')
		err_399("no dash: YYYY-MM");
	if (*p != '0' && *p != '1')
		err_399r("month: first not 0 or 1");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("month: sec < 0 or > 9");
	p++;

	/*
	 *  Day:
	 *	match -[0123][0-9]
	 */
	if (*p++ != '-')
		err_399("no dash: MM-DD");
	if ('0' > *p || *p > '3')
		err_399r("day: first < 0 or > 3");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("day: first < 0 or > 9");
	p++;

	/*
	 *  Hour:
	 *	match T[012][0-9]
	 */
	if (*p++ != 'T')
		err_399("no dayTtime separator");
	if ('0' > *p || *p > '2')
		err_399r("hour: first < 0 or > 2");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("hour: sec < 0 or > 9");
	p++;

	/*
	 *  Minute:
	 *	match :[0-5][0-9]
	 */
	if (*p++ != ':')
		err_399("missing colon: HH:MM");
	if ('0' > *p || *p > '5')
		err_399r("min: first < 0 or > 5");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("min: first < 0 or > 9");
	p++;

	/*
	 *  Second:
	 *	match :[0-5][0-9]
	 */
	if (*p++ != ':')
		err_399("missing colon: MM:SS");
	if ('0' > *p || *p > '5')
		err_399r("sec: first < 0 or > 5");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("sec: sec < 0 or > 9");
	p++;

	if (*p++ != '.')
		err_399("no dot: SS.");
	in_range(&p, 9, 1, digits);

	switch (*p++) {
	case 'Z':
		err_399r("Z not allowed as time zone");
		break;
	case '+': case '-':
		in_set(&p, 2, digits);
		if (*p++ != ':')
			err_399r("no colon: timezone: MM:SS");
		in_set(&p, 2, digits);
		break;
	default:
		err_399("no plus or minus in timezone");
	}
	if (*p++ != '\t')
		eexit("scan_rfc399nano");
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
		eexit("scan_transport");

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
	char c, *p = *src, *verb = 0;

	c = *p++;
	if (c == 'g') {			/* get|give */
		c = *p++;
		if (c == 'e') {			/* get */
			if (*p++ != 't')
				eexit("scan_verb");	/* give */
			verb = "get";

		} else if (c == 'i') {
			if (*p++ != 'v' || *p++ != 'e')
				eexit("scan_verb");
			verb = "give";
		} else
			eexit("scan_verb");
	} else if (c == 'p') {		/* put */
		if (*p++ != 'u' || *p++ != 't')
			eexit("scan_verb");
		verb = "put";
	} else if (c == 't') {		/* take */
		if (*p++ != 'a' || *p++ != 'k' || *p++ != 'e')
			eexit("scan_verb");
		verb = "take";
	} else if (c == 'e') {		/* eat */
		if (*p++ != 'a' || *p++ != 't')
			eexit("scan_verb");
		verb = "eat";
	} else if (c == 'w') {		/* wrap */
		if (*p++ != 'r' || *p++ != 'a' || *p++ != 'p')
			eexit("scan_verb");
		verb = "wrap";
	} else if (c == 'r') {		/* roll */
		if (*p++ != 'o' || *p++ != 'l' || *p++ != 'l')
			eexit("scan_verb");
		verb = "roll";
	} else
		eexit("scan_verb");
	if (*p++ != '\t')
		eexit("scan_verb");
	*src = p;
	return verb;
}
/*
 *  Scan the uniform digest that looks like:
 *
 *  	algorithm:hash-digest
 *  Note:
 *	replace with jmscott_is_udig
 */
static void
scan_udig(char **src)
{
	char *p = *src;

	/*
	 *  First char of algorithm must be lower case alpha.
	 */
	if ('a' > *p || *p > 'z')
		eexit("scan_udig");
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
		eexit("scan_chat_history");
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
		eexit("scan_chat_history");
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
		eexit("scan_chat_history");
	}

	/*
	 *  Only verbs "give" and "take" can do "ok,ok,ok" or "ok,ok,no".
	 */
	if (
		(strcmp("ok,ok,ok",ch)==0 || strcmp("ok,ok,no", ch)==0)	&&
		(v1=='t' || (v1=='g' && v1=='i'))
	)
		goto done;
	eexit("scan_chat_history");
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
		eexit("scan_byte_count");
	*src = p;
}

/*
 *  Duration: seconds.nsecs, where seconds ~ /^\{1,9}$/
 *  	      and nsecs ~ /^\d{9}$/
 *  	      and seconds <= 2147483647
 */
static void
scan_wall_duration(char **src)
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
		eexit("scan_wall_duration");
	in_range(&p, 9, 1, digits);
	if (*p++ != '\n')
		eexit("scan_wall_duration");
	*src = p;
}

int
main(int argc, char **argv)
{
	char *verb;
	int read_input = 0;

	if (argc > 2)
		die("too many cli args: expected 0 or 1");
	if (argc == 2) {
		if (strcmp(argv[1], "--trace"))
			die2("unknown cli arg", argv[1]);
		tracing = 1;
	}
		
	while (fgets(line, sizeof line, stdin)) {
		if (tracing)
			line_no++;
			
		read_input = 1;

		char *p = line;

		//  start time of request
		scan_rfc399nano(&p);

		scan_transport(&p);

		verb = scan_verb(&p);

		scan_udig(&p);

		scan_chat_history(verb, &p);

		//  byte count
		scan_byte_count(&p);

		//  wall duration
		scan_wall_duration(&p);

		line[0] = 0;
	}
	line_no = 0;
	if (ferror(stderr))
		die("ferror(stdin) returned true");
	if (read_input) {
		line_no = 0;
		if (feof(stdin))
			exit(0);
		die("premature end of stdin");
	}
	exit(2);		//  empty list
}
