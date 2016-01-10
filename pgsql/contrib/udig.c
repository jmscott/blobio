/*
 *  Synopsis:
 *	Implement PostgreSQL UDIG Data Types for SHA1.
 *  Note:
 *	Can the operator functions be prevented from being called explicily?
 *	In other words, prevent the following query?
 *
 *		select udig_eq(u1, u2);
 *
 *	Think about reserving high three order bits of algorithm byte value
 *	to indicate that the total number of bits in the digest is divied by
 *	2, 4, 8.
 */

#include "postgres.h"
#include "fmgr.h"
#include "libpq/pqformat.h"             /* needed for send/recv functions */

#define UDIG_VARDATA(p)	(unsigned char *)VARDATA(p)

/*
 *  Used by generic 'udig' type.
 *  Note: need to prefix these symbols with 'PG_'.
 */
#define UDIG_SHA	1

PG_MODULE_MAGIC;

Datum	udig_sha_in(PG_FUNCTION_ARGS);
Datum	udig_sha_out(PG_FUNCTION_ARGS);

/*
 *  Core SHA.
 */
Datum	udig_sha_eq(PG_FUNCTION_ARGS);
Datum	udig_sha_ne(PG_FUNCTION_ARGS);
Datum	udig_sha_gt(PG_FUNCTION_ARGS);
Datum	udig_sha_ge(PG_FUNCTION_ARGS);
Datum	udig_sha_lt(PG_FUNCTION_ARGS);
Datum	udig_sha_le(PG_FUNCTION_ARGS);
Datum	udig_sha_cmp(PG_FUNCTION_ARGS);

/*
 *  Cross type SHA -> UDIG
 */
Datum	udig_sha_eq_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_ne_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_gt_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_ge_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_lt_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_le_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_cmp_udig(PG_FUNCTION_ARGS);
Datum	udig_sha_cast(PG_FUNCTION_ARGS);

/*
 *  Core generic/text udig type.
 */
Datum	udig_in(PG_FUNCTION_ARGS);
Datum	udig_out(PG_FUNCTION_ARGS);
Datum	udig_eq(PG_FUNCTION_ARGS);
Datum	udig_ne(PG_FUNCTION_ARGS);
Datum	udig_gt(PG_FUNCTION_ARGS);
Datum	udig_ge(PG_FUNCTION_ARGS);
Datum	udig_lt(PG_FUNCTION_ARGS);
Datum	udig_le(PG_FUNCTION_ARGS);
Datum	udig_cmp(PG_FUNCTION_ARGS);
Datum	udig_algorithm(PG_FUNCTION_ARGS);
Datum	udig_can_cast(PG_FUNCTION_ARGS);

/*
 *  Cross Type UDIG,SHA
 */
Datum	udig_eq_sha(PG_FUNCTION_ARGS);
Datum	udig_ne_sha(PG_FUNCTION_ARGS);
Datum	udig_gt_sha(PG_FUNCTION_ARGS);
Datum	udig_ge_sha(PG_FUNCTION_ARGS);
Datum	udig_lt_sha(PG_FUNCTION_ARGS);
Datum	udig_le_sha(PG_FUNCTION_ARGS);
Datum	udig_cmp_sha(PG_FUNCTION_ARGS);


/*
 *  Transform 40 char hex string to 20 byte sha binary.
 */
static int
hex2sha(char *d40, unsigned char *d20)
{
	int i;

	memset(d20, 0, 20);

	/*
	 *  Parse the sha digest in hex format.
	 */
	for (i = 0;  i < 40;  i++) {
		char c;
		unsigned char nib = 0;		// gcc4 complains if not inited

		c = *d40++;
		if (c >= '0' && c <= '9')
			nib = c - '0';
		else if (c >= 'a' && c <= 'f')
			nib = c - 'a' + 10;
		else
			return -1;
		if ((i & 1) == 0)
			nib <<= 4;
		d20[i >> 1] |= nib;
	}
	if (*d40)
		return -1;
	return 0;
}

static char nib2hex[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

/*
 *  Transform 20 byte, sha binary to 40 char hex string.
 */
static void
sha2hex(unsigned char *d20, char *d40)
{
	unsigned char *d20_end;
	char *d;

	/*
	 *  Convert the binary digest to 40 byte hex text.
	 */
	d = d40;
	d20_end = d20 + 20;
	while (d20 < d20_end) {
		*d++ = nib2hex[(*d20 & 0xf0) >> 4];
		*d++ = nib2hex[*d20 & 0xf];
		d20++;
	}
	*d = 0;
}

/*
 *  Convert sha1 text digest in common hex format into binary 20 bytes.
 */
PG_FUNCTION_INFO_V1(udig_sha_in);

Datum
udig_sha_in(PG_FUNCTION_ARGS)
{
	char *hex40 = PG_GETARG_CSTRING(0);
	unsigned char *d20;

	d20 = (unsigned char *)palloc(20);
	if (hex2sha(hex40, d20)) {
		pfree(d20);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("invalid sha digest: \"%s\"", hex40)));
	}
	PG_RETURN_POINTER(d20);
}

/*
 *  Convert binary, 20 byte sha digest into common, 40 character 
 *  text hex format.
 */
PG_FUNCTION_INFO_V1(udig_sha_out);

Datum
udig_sha_out(PG_FUNCTION_ARGS)
{
	unsigned char *d20 = (unsigned char *)PG_GETARG_POINTER(0);
	char *hex40;

	hex40 = (char *)palloc(41);
	sha2hex(d20, hex40);
	PG_RETURN_CSTRING(hex40);
}

/*
 *  Core SHA Operators
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */
PG_FUNCTION_INFO_V1(udig_sha_eq);

Datum
udig_sha_eq(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) == 0);
}

PG_FUNCTION_INFO_V1(udig_sha_ne);

Datum
udig_sha_ne(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) != 0);
}

PG_FUNCTION_INFO_V1(udig_sha_gt);

Datum
udig_sha_gt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) > 0);
}

PG_FUNCTION_INFO_V1(udig_sha_ge);

Datum
udig_sha_ge(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) >= 0);
}

PG_FUNCTION_INFO_V1(udig_sha_lt);

Datum
udig_sha_lt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) < 0);
}

PG_FUNCTION_INFO_V1(udig_sha_le);

Datum
udig_sha_le(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL((int32)memcmp(a, b, 20) <= 0);
}

PG_FUNCTION_INFO_V1(udig_sha_cmp);

Datum
udig_sha_cmp(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32((int32)memcmp(a, b, 20));
}

/*
 *  Cross SHA, Generic UDIG Operators
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */

/*
 *  Compare udig_sha versus generic udig.
 */
static int32
_udig_sha_cmp_udig(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *b;

	b = UDIG_VARDATA(b_operand);

	switch (b[0]) {
	case UDIG_SHA:
		return memcmp(a_operand, &b[1], 20);	/* sha versus sha */
	}
	ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED),
		errmsg("_udig_sha_cmp_udig: corrupted udig internal type")));
	return -1;
}

/*
 *  Cross Type SHA -> UDIG
 */
PG_FUNCTION_INFO_V1(udig_sha_eq_udig);

Datum
udig_sha_eq_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) == 0);
}

PG_FUNCTION_INFO_V1(udig_sha_ne_udig);

Datum
udig_sha_ne_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) != 0);
}

PG_FUNCTION_INFO_V1(udig_sha_gt_udig);

Datum
udig_sha_gt_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) > 0);
}

PG_FUNCTION_INFO_V1(udig_sha_ge_udig);

Datum
udig_sha_ge_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(udig_sha_lt_udig);

Datum
udig_sha_lt_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) < 0);
}

PG_FUNCTION_INFO_V1(udig_sha_le_udig);

Datum
udig_sha_le_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sha_cmp_udig(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(udig_sha_cmp_udig);

Datum
udig_sha_cmp_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32((int32)_udig_sha_cmp_udig(a, b));
}

/*
 *  Cast a udig_sha to generic udig.
 */
PG_FUNCTION_INFO_V1(udig_sha_cast);

Datum
udig_sha_cast(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *)PG_GETARG_POINTER(0);
	Size size;
	unsigned char *udig;

	size = VARHDRSZ + 21;		/* size bytes + udig type byte + data */
	udig = palloc(size);
	udig[4] = UDIG_SHA;		/* sha type */
	memcpy(udig + 5, src, 20);
	SET_VARSIZE(udig, size);
	PG_RETURN_POINTER(udig);
}

/*
 *  Convert text udig in the format
 *
 *	algorithm:digest
 *
 *  to variable length
 *
 *	[algo byte] [data ...]
 *
 */
PG_FUNCTION_INFO_V1(udig_in);

Datum
udig_in(PG_FUNCTION_ARGS)
{
	char *udig, *u, *u_end;
	char a16[17], *a;
	unsigned char *d = 0;
	Size size = 0;
	static char no_aprint[] =
		"unprintable character in algorithm name: 0x%x";
	static char big_algo[] =
		"algorithm > 16 characters: \"%s\"";
	static char empty_udig[] =
		"empty udig";
	static char no_algo[] =
		"unknown algorithm in udig: \"%s\"";

	udig = PG_GETARG_CSTRING(0);
	if (udig == NULL)
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("null hex in udig digest")));
	/*
	 *  Extract the algorithm from the udig.
	 */
	a = a16;
	a16[16] = ':';		/* changes to null on successfull parse */
	u = udig;
	u_end = &udig[16];
	while (*u && u < u_end) {
		char c = *u++;

		if (!isgraph(c))
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(no_aprint, c)));
		if (c == ':') {
			*a = a16[16] = 0;	/* successfull parse */
			break;
		}
		*a++ = c;
	}
	if (u == udig)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg(empty_udig, udig)));
	if (a16[16] == ':')
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg(big_algo, udig)));
	/*
	 *  Store SHA digests in binary form.
	 */
	if (strcmp("sha", a16) == 0) {
		size = VARHDRSZ + 1 + 20;
		d = palloc(size);
		d[4] = UDIG_SHA;
		if (hex2sha(u, &d[VARHDRSZ + 1])) {
			pfree(d);
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(
		            "invalid input syntax for udig sha digest: \"%s\"",
								u)));
		}
	}
	else
		ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(no_algo, a16)));

	SET_VARSIZE(d, size);
	PG_RETURN_POINTER(d);
}

/*
 *  Convert udig format into character string.
 *  text hex format.
 */
PG_FUNCTION_INFO_V1(udig_out);

Datum
udig_out(PG_FUNCTION_ARGS)
{
	unsigned char *p = (unsigned char *)PG_GETARG_POINTER(0);
	char *udig = 0;
	unsigned char *d;

	d = UDIG_VARDATA(p);

	switch (*d) {
	case UDIG_SHA:
		udig = palloc(3 + 1 + 40 + 1);
		strcpy(udig, "sha:");
		sha2hex((unsigned char *)&d[1], udig + 4);
		break;
	default:
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg("udig_out: corrupted udig internal type byte")));
		break;
	}
	PG_RETURN_CSTRING(udig);
}

/*
 *  Lexically compare two udigs.  Udigs of the same algorithm will compare by
 *  digests.  Udigs with differnt algorithms will compare the algorithms only.
 */
static int
_udig_cmp(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *a, *b;

/*
 *  gcc compiler bug for static variable gets confused doing print style format
 *  checks.
 */
#define BAD_TYPE_GCC_BUG	"_udig_cmp: corrupted udig type byte in first operand"

	a = UDIG_VARDATA(a_operand);
	b = UDIG_VARDATA(b_operand);

	/*
	 *  Operands have identical types.
	 */
	if (a[0] == b[0]) {
		switch (a[0]) {
		case UDIG_SHA:
			return memcmp(&a[1], &b[1], 20);
		default:
			ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED),
						errmsg(BAD_TYPE_GCC_BUG)));
		}
	}
	/*
	 *  Eventually operands differ.
	 *
	 *  Note:
	 *  	This is legacay code when the skein digest existed.
	 *  	Probably need to simplify.
	 */
	switch (a[0]) {
	case UDIG_SHA:
		return 1;		/* is a sha */
	default:
		ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED),
						errmsg(BAD_TYPE_GCC_BUG)));
	}
	/*NOTREACHED*/
	return -1;
}

/*
 *  Generic UDIG Core Type Binary Operators:
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */
PG_FUNCTION_INFO_V1(udig_eq);

Datum
udig_eq(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) == 0);
}

PG_FUNCTION_INFO_V1(udig_ne);

Datum
udig_ne(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) != 0);
}

PG_FUNCTION_INFO_V1(udig_gt);

Datum
udig_gt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) > 0);
}

PG_FUNCTION_INFO_V1(udig_ge);

Datum
udig_ge(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(udig_lt);

Datum
udig_lt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) < 0);
}

PG_FUNCTION_INFO_V1(udig_le);

Datum
udig_le(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(udig_cmp);

Datum
udig_cmp(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32(_udig_cmp(a, b));
}

/*
 *  Cross Type UDIG, SHA Functions
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */

/*
 *  Compare generic udig and sha.
 */
static int32
_udig_cmp_sha(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *a;

	a = UDIG_VARDATA(a_operand);

	switch (a[0]) {
	case UDIG_SHA:
		return memcmp(&a[1], b_operand, 20);
	}
	ereport(PANIC, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
		errmsg("_udig_cmp_sha: corrupted udig internal type byte")));
	return -1;
}

PG_FUNCTION_INFO_V1(udig_eq_sha);

Datum
udig_eq_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) == 0);
}

PG_FUNCTION_INFO_V1(udig_ne_sha);

Datum
udig_ne_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) != 0);
}

PG_FUNCTION_INFO_V1(udig_gt_sha);

Datum
udig_gt_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) > 0);
}

PG_FUNCTION_INFO_V1(udig_ge_sha);

Datum
udig_ge_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(udig_lt_sha);

Datum
udig_lt_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) < 0);
}

PG_FUNCTION_INFO_V1(udig_le_sha);

Datum
udig_le_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sha(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(udig_cmp_sha);

Datum
udig_cmp_sha(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32(_udig_cmp_sha(a, b));
}

PG_FUNCTION_INFO_V1(udig_algorithm);

/*
 *  Extract the algorithm of a udig as a string.
 */
Datum
udig_algorithm(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)UDIG_VARDATA(PG_GETARG_POINTER(0));

	switch (a[0]) {
	case UDIG_SHA:
		PG_RETURN_CSTRING("sha");
	}
	ereport(PANIC,
		(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
		errmsg("udig_algorithm: corrupted udig internal type byte"
		)));
	/*NOTREACHED*/
	PG_RETURN_CSTRING("corrupted udig");
}
/*
 *  Is the text string a recognized udig.  In other words,  the text string 
 */
PG_FUNCTION_INFO_V1(udig_can_cast);

Datum
udig_can_cast(PG_FUNCTION_ARGS)
{
	char *udig, *u, *u_end;
	char a16[17], *a;

	udig = PG_GETARG_CSTRING(0);
	if (udig == NULL)
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("null hex in udig digest")));
	/*
	 *  Extract the algorithm from the udig.
	 */
	a = a16;
	a16[16] = ':';		/* changes to null on successfull parse */
	u = udig;
	u_end = &udig[16];
	while (*u && u < u_end) {
		char c = *u++;

		if (!isgraph(c))
			goto no;
		if (c == ':') {
			*a = a16[16] = 0;	/* successfull parse */
			break;
		}
		*a++ = c;
	}
	if (u == udig)
		goto no;
	if (a16[16] == ':')
		goto no;
	f (strcmp("sha", a16) == 0) {
		unsigned char d[VARHDRSZ + 1 + 20];

		d[4] = UDIG_SHA;
		if (hex2sha(u, &d[VARHDRSZ + 1]))
			goto no;
	}
	else
		goto no;
	PG_RETURN_BOOL(1);
no:
	PG_RETURN_BOOL(0);
}
