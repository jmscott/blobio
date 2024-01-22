/*
 *  Synopsis:
 *	Is a stream blob request records: yes or no or empty?
 *  Description
 *  	A stream of blob request records consists of lines of ascii text
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
 *  Exit Status:
 *  	0	-> is a brr log
 *  	1	-> is not a brr log
 *	2	-> is empty
 *  	3	-> unexpected error
 *  Note:
 *	Test for blob_size == 0 for non-empty blobs.
 *
 *	What about leap second?
 *
 *  	Need a compile time pragma to insure the
 *
 *  		sizeof long long int == 8
 *
 *	Replace sexit(func) with macro SCAN_EXIT() that references _funcname
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

#define EXIT_IS_BRR	0
#define EXIT_IS_NOT_BRR	1
#define EXIT_IS_EMPTY	2
#define EXIT_ERROR	3

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

//  exit due to a failure in scan.

static void
sexit(char *msg, char *what)
{
	if (!tracing)
		exit(EXIT_IS_NOT_BRR);
	if (line_no == 0)
		jmscott_die3(EXIT_IS_NOT_BRR, "scan", msg, what);

	char u64_digits[21];

	//  tack on the line number when tracing

	*jmscott_ulltoa(line_no, u64_digits) = 0;
	jmscott_die6(
		EXIT_IS_NOT_BRR,
		"scan",
		msg,
		what,
		"line number",
		u64_digits,
		line
	);
}

static void
die(char *msg)
{
	if (tracing) {
		if (line_no == 0)
			jmscott_die(EXIT_ERROR, msg);

		char u64_digits[21];

		*jmscott_ulltoa(line_no, u64_digits) = 0;
		jmscott_die4(1, msg, "line number", u64_digits, line);
	}
	jmscott_die(EXIT_ERROR, msg);
}

static void
die2(char *msg1, char *msg2)
{
	char msg[JMSCOTT_ATOMIC_WRITE_SIZE];

	msg[0] = 0;
	jmscott_strcat3(msg, sizeof msg, msg1, ": ", msg2);
	die(msg);
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
			sexit("in_set", set);
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
			sexit("to few chars", "in_range");
		--s;
		break;
	}
	*src = s;
}

/*
 *  Scan up to 'limit' chars that are exist in char "set"
 *  and set src to first char after the end char.
 *  non-ascii always fails. also assume char 'end_fld' not in the scan set.
 *
 *  Note:
 *	what happens when zero chars are in the set?
 */
static void 
scan_set(char **src, int limit, char *set, char end_fld, char *what)
{
	char *s, *s_end;

	s = *src;
	s_end = s + limit;
	while (s < s_end) {
		char c = *s;

		if (c == end_fld) {
			if (s++ == *src)
				sexit("no chars seen in set", what);
			break;
		}
		if (!c)
			sexit("null char", what); 
		if (!isascii(c))
			sexit("char not ascii", what);
		if (strchr(set, c) != NULL) {
			s++;
			continue;
		}

		//  error, so format message and croak

		char msg[JMSCOTT_ATOMIC_WRITE_SIZE];
		msg[0] = 0;

		if (isascii(c))
			jmscott_strcat(msg, sizeof msg,
					"ascii char not in set"
			);
		else
			jmscott_strcat(msg, sizeof msg,
					"non ascii char not in set"
			);

		char hex[5];
		snprintf(hex, sizeof hex, "0x%x", c);
		jmscott_strcat2(msg, sizeof msg, ": ", hex);
		sexit(msg, what);
	}
	if (s == s_end) {
		if (*s != end_fld)
			sexit("no end of fld char seen", what);
		s++;
	}
	*src = s;
}

static void
err_3339(char *msg)
{
	sexit(msg, "start time");
}

static void
err_399r(char *rmsg)
{
	char msg[JMSCOTT_ATOMIC_WRITE_SIZE];
	msg[0] = 0;

	jmscott_strcat2(msg, sizeof msg,
		"digit out of range: ",
		rmsg
	);
	sexit(msg, "start time");
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
	if ('2' > *p || *p > '5') {
		if (!*p)
			err_399r("null char");

		//  catch common error patterns, to help tracing
		if (*p == '\t')
			err_399r("unexpected tab");
		if (*p == '\n')
			err_399r("unexpected newline");
		if (isalpha(*p))
			err_399r("unexpected alpha char");
		if (!isascii(*p))
			err_399r("char not ascii");
		err_399r("year: first either < 2 or > 5");
	}
	p++;
	in_set(&p, 3, digits);

	/*
	 *  Month:
	 *	match -[01][0-9]
	 */
	if (*p++ != '-')
		err_3339("no dash: YYYY-MM");
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
		err_3339("no dash: MM-DD");
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
		err_3339("no dayTtime separator");
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
		err_3339("missing colon: HH:MM");
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
		err_3339("missing colon: MM:SS");
	if ('0' > *p || *p > '5')
		err_399r("sec: first < 0 or > 5");
	p++;
	if ('0' > *p || *p > '9')
		err_399r("sec: sec < 0 or > 9");
	p++;

	if (*p++ != '.')
		err_3339("no dot: SS.");
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
		err_3339("no plus or minus in timezone");
	}
	if (*p++ != '\t')
		sexit("tab end char not seen", "start_time");
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
		sexit("first char not in [a-z]", "transport");

	if (*p == '~')		//  transport protocol single char in [a-z]
		p++;
	else
		scan_set(&p, 7, lower_alnum, '~', "transport");

	//  Note: does at least one char exist?
	scan_set(&p, 128, graph_set, '\t', "transport");

	*src = p;
}

static void
vexit(char *err, char *verb)
{
	char msg[JMSCOTT_ATOMIC_WRITE_SIZE];
	msg[0] = 0;

	jmscott_strcat2(msg, sizeof msg, "verb: ", err);
	sexit(msg, verb);
}

/*
 *  Blobio verb: get|put|give|take|eat|what.
 */
static char *
scan_verb(char **src)
{
	char c, *p = *src, *verb = 0;

	c = *p++;

	switch (c) {
	case 'g':
		c = *p++;
		if (c == 'e') {
			if (*p++ != 't')
				vexit("expected 't'", "get");
			verb = "get";

		} else if (c == 'i') {
			if (*p++ != 'v' || *p++ != 'e')
				vexit("expected 'v' or 'e'", "give");
			verb = "give";
		} else
			vexit("expected char 'e' or 'i'", "{get, give}");
		break;

	case 'p':
		if (*p++ != 'u' || *p++ != 't')
			vexit("expected 'u' or 't'", "put");
		verb = "put";
		break;
	case 't':
		if (*p++ != 'a' || *p++ != 'k' || *p++ != 'e')
			vexit("expected 'a' or 'k' or 'e'", "take");
		verb = "take";
		break;
	case 'e':
		if (*p++ != 'a' || *p++ != 't')
			vexit("expected 'a' or 't'", "eat");
		verb = "eat";
		break;
	case 'w':
		if (*p++ != 'r' || *p++ != 'a' || *p++ != 'p')
			vexit("expected 'r' or 'a'", "wrap");
		verb = "wrap";
		break;
	case 'r':
		if (*p++ != 'o' || *p++ != 'l' || *p++ != 'l')
			vexit("expected 'o' or 'l'", "roll");
		verb = "roll";
		break;
	default: {
		static char errfc[] = "unexpected first char";

		if (c == '\t')
			vexit(errfc, "tab");
		if (c == '\n')
			vexit(errfc, "newline");

		char hex[5];

		snprintf(hex, sizeof hex, "0x%x", c);
		vexit(errfc, hex);
		/*NOTREACHED*/
	}}
	if (*p++ != '\t')
		vexit("no tab field sep char", verb);
	*src = p;
	return verb;
}

/*
 *  Scan the uniform digest that looks like:
 *
 *  	algorithm:hash-digest
 *
 *  Returns 0 if udig exists, 1 if udig is empty.
 */
static int
scan_udig(char **src)
{
	char *p = *src;
	char *err;
	char *tab;

	tab = strchr(p, '\t');
	if (!tab)
		sexit("no terminating tab", "udig");

	//  an empty udig is a valid udig.
	//  caller determines when empty udig is valid.

	if (tab - p == 0) {
		*src = tab + 1;
		return 1;
	}
	if (tab - p < 34)
		sexit("len < 34", "udig");
	if (tab - p > 137)
		sexit("len > 137", "udig");

	*tab = 0;
	err = jmscott_frisk_udig(p);
	*tab = '\t';
	if (err) {
		char msg[JMSCOTT_ATOMIC_WRITE_SIZE];

		msg[0] = 0;
		jmscott_strcat2(msg, sizeof msg,
			"syntax error: ",
			err
		);
		sexit(msg, "udig");
	}
	*src = tab + 1;
	return 0;
}

static void
errch(char *err)
{
	sexit(err, "chat history");
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
 *  Scan the chat history first, and then verify the reposnse
 *  is correct for the verb.
 */
static int
scan_chat_history(char *verb, char **src)
{
	char *p, *end, ch[9];
	int ch_len;

	p = *src;
	end = p;

	scan_set(&end, 8, "nok,", '\t', "chat history");

	ch_len = (end - 1) - p;

	if (ch_len != 2 && ch_len != 5 && ch_len != 8)
		errch("length not 2 and 5 and 8");
	memcpy(ch, p, ch_len);
	ch[ch_len] = 0;

	if (strcmp("no", ch) == 0)	//  any verb can reply "no"
		goto done;

	//  first and second chars of the verb is unique.
	char v1 = verb[0];
	char v2 = verb[1];

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
		errch("ok: verb: expected first char: [gewr]");
	}

	/*
	 *  chat history must start with "ok,"
	 *
	 *  Note:
	 *	One day i (jmscott) will optimize away strcmp()
	 */

	/*
	 *  Verbs that reply with: ok,ok or ok,no are only the following:
	 *
	 *	give, take, put
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
		errch("verb: not in {give,take,put}");
	}

	/*
	 *  Only verbs "give" and "take" can do "ok,ok,ok" or "ok,ok,no".
	 */
	if (
		(strcmp("ok,ok,ok",ch)==0 || strcmp("ok,ok,no", ch)==0)&&
		(v1=='t' || (v1=='g' && v2=='i'))
	)
		goto done;

	char err[JMSCOTT_ATOMIC_WRITE_SIZE];
	err[0] = 0;
	jmscott_strcat2(err, sizeof err,
		"unknown chat history: ",
		ch
	);
	errch(err);
done:
	*src = end;
	return ch[ch_len-1] == 'o' ? 1 : 0;
}

/*
 *  Scan for an unsigned int <= 2^63 -1.  A size of zero only valid for the
 *  empty blob.
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

	scan_set(&p, 20, digits, '\t', "byte count");

	errno = 0;
	v = strtoll(q, (char **)0, 10);

	if ((v == LLONG_MAX && errno == ERANGE) ||
	    (LLONG_MAX > 9223372036854775807 &&
	     v > 9223372036854775807))
		sexit("out of range", "byte count");
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

	scan_set(&p, 10, digits, '.', "wall duration: sec");

	errno = 0;
	v = strtoll(q, (char **)0, 10);

	if ((v == LONG_MAX && errno == ERANGE) ||
	    (LONG_MAX > 2147483647 && v > 2147483647))
		sexit("sec of range", "wall duration");
	in_range(&p, 9, 1, digits);
	if (*p++ != '\n')
		sexit("no new-line terminator", "wall durtion");
	*src = p;
}

int
main(int argc, char **argv)
{
	int read_input = 0;

	if (argc > 2)
		die("too many cli args: expected 0 or 1");
	if (argc == 2) {
		if (strcmp(argv[1], "--trace"))
			die2("unknown cli arg", argv[1]);
		tracing = 1;
	}
		
	while (fgets(line, sizeof line, stdin)) {
		if (ferror(stdin))
			die("fgets(stdin) error");
		if (tracing)
			line_no++;
			
		read_input = 1;

		char *p = line;

		//  start time of request
		scan_rfc399nano(&p);

		//  network transport
		scan_transport(&p);

		char *verb = scan_verb(&p);

		int no_udig = scan_udig(&p);
		int is_no = scan_chat_history(verb, &p);

		if (*verb == 'w' || *verb == 'r') {
			if (!no_udig && is_no)
				die("{wrap,roll}/no: udig exists");
			if (no_udig && !is_no)
				die("{wrap,roll}/ok: no udig");
		}

		scan_byte_count(&p);

		//  wall duration
		scan_wall_duration(&p);
	}
	line_no = 0;		// disable die() putting line no in ERROR
	if (ferror(stderr))
		die("ferror(stdin) returned true");
	if (read_input) {
		if (feof(stdin))
			exit(EXIT_IS_BRR);
		die("premature end of stdin");
	}
	if (fclose(stdin))
		die("fclose(stdin) failed");
	exit(EXIT_IS_EMPTY);		//  empty file
}
