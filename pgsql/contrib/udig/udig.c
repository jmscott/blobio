/*
 *  Synopsis:
 *	Implement PostgreSQL UDIG Data Types for SHA1.
 *  Note:
 *	udig algorithm is assumed to be 16 ascii chars.  spec says 8 chars.
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
#define UDIG_BC160	2

PG_MODULE_MAGIC;

Datum	udig_sha_in(PG_FUNCTION_ARGS);
Datum	udig_sha_out(PG_FUNCTION_ARGS);

Datum	udig_bc160_in(PG_FUNCTION_ARGS);
Datum	udig_bc160_out(PG_FUNCTION_ARGS);

/*
 *  Core SHA1.
 */
Datum	udig_sha_eq(PG_FUNCTION_ARGS);
Datum	udig_sha_ne(PG_FUNCTION_ARGS);
Datum	udig_sha_gt(PG_FUNCTION_ARGS);
Datum	udig_sha_ge(PG_FUNCTION_ARGS);
Datum	udig_sha_lt(PG_FUNCTION_ARGS);
Datum	udig_sha_le(PG_FUNCTION_ARGS);
Datum	udig_sha_cmp(PG_FUNCTION_ARGS);

/*
 *  Core BitCoin RIPEMD160(SHA256)
 */
Datum	udig_bc160_eq(PG_FUNCTION_ARGS);
Datum	udig_bc160_ne(PG_FUNCTION_ARGS);
Datum	udig_bc160_gt(PG_FUNCTION_ARGS);
Datum	udig_bc160_ge(PG_FUNCTION_ARGS);
Datum	udig_bc160_lt(PG_FUNCTION_ARGS);
Datum	udig_bc160_le(PG_FUNCTION_ARGS);
Datum	udig_bc160_cmp(PG_FUNCTION_ARGS);

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
 *  Cross type BitCoin RIPEMD160(SHA256) -> UDIG
 */
Datum	udig_bc160_eq_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_ne_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_gt_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_ge_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_lt_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_le_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_cmp_udig(PG_FUNCTION_ARGS);
Datum	udig_bc160_cast(PG_FUNCTION_ARGS);

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
 *  Cross Type UDIG,BC160
 */
Datum	udig_eq_bc160(PG_FUNCTION_ARGS);
Datum	udig_ne_bc160(PG_FUNCTION_ARGS);
Datum	udig_gt_bc160(PG_FUNCTION_ARGS);
Datum	udig_ge_bc160(PG_FUNCTION_ARGS);
Datum	udig_lt_bc160(PG_FUNCTION_ARGS);
Datum	udig_le_bc160(PG_FUNCTION_ARGS);
Datum	udig_cmp_bc160(PG_FUNCTION_ARGS);

static char empty_udig[] = "empty udig";
static char big_algo[] = "algorithm > 16 characters: \"%s\"";
static char no_aprint[] = "unprintable character in algorithm name: 0x%x";
#define ALGO_HINT "did you mean sha or bc160"

/*
 *  Transform 40 char hex string to 20 byte sha binary.
 */
static int
hex2dig20(char *d40, unsigned char *d20)
{
	int i;

	memset(d20, 0, 20);

	/*
	 *  Parse the 40 char digest in hex format.
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
dig2hex40(unsigned char *d20, char *d40)
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
	if (hex2dig20(hex40, d20)) {
		pfree(d20);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("invalid sha digest: \"%s\"", hex40)));
	}
	PG_RETURN_POINTER(d20);
}

/*
 *  Convert bc160 text digest in common hex format into binary 20 bytes.
 */
PG_FUNCTION_INFO_V1(udig_bc160_in);

Datum
udig_bc160_in(PG_FUNCTION_ARGS)
{
	char *hex40 = PG_GETARG_CSTRING(0);
	unsigned char *d20;

	d20 = (unsigned char *)palloc(20);
	if (hex2dig20(hex40, d20)) {
		pfree(d20);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg("invalid bc160 digest: \"%s\"", hex40)));
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
	dig2hex40(d20, hex40);
	PG_RETURN_CSTRING(hex40);
}

/*
 *  Convert binary, 20 byte bc160 digest into common, 40 character 
 *  text hex format.
 */
PG_FUNCTION_INFO_V1(udig_bc160_out);

Datum
udig_bc160_out(PG_FUNCTION_ARGS)
{
	unsigned char *d20 = (unsigned char *)PG_GETARG_POINTER(0);
	char *hex40;

	hex40 = (char *)palloc(41);
	dig2hex40(d20, hex40);
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
 *  Compare udig_sha=a versus generic udig=b.
 */
static int32
_udig_sha_cmp_udig(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *b;

	b = UDIG_VARDATA(b_operand);

	if (b[0] == UDIG_SHA)
		return memcmp(a_operand, &b[1], 20);	// sha versus sha
	if (b[0] == UDIG_BC160)
		return -1;
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
 *	{sha,bc160}:[a-f0-9]{40}
 *
 *  to variable length
 *
 *	[algo byte] [20 bytes of digest bits]
 *
 */
PG_FUNCTION_INFO_V1(udig_in);

Datum
udig_in(PG_FUNCTION_ARGS)
{
	char *udig, *u, *u_end;
	char a8[9], *a;
	unsigned char *d;
	Size size = 0;

	static char no_algo[] =
		"unknown algorithm in udig: \"%s\"";
	static char bad_dig[] =
		"invalid input syntax for udig %s digest: \"%s\"";

	udig = PG_GETARG_CSTRING(0);
	if (udig == NULL)
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("null hex in udig digest")));
	/*
	 *  Extract the digest algorithm from the udig.
	 */
	a = a8;
	a8[8] = ':';		/* changes to null on successfull parse */
	u = udig;
	u_end = &udig[8];
	while (*u && u < u_end) {
		char c = *u++;

		if (!isgraph(c))
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(no_aprint, c),
				errhint(ALGO_HINT)
			));

		if (c == ':') {
			*a = a8[8] = 0;	/* successfull parse */
			break;
		}
		*a++ = c;
	}
	if (u == udig)
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg(empty_udig, udig)));
	if (a8[8] == ':')
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(big_algo, udig),
				errhint(ALGO_HINT)
			));
	/*
	 *  Store SHA or BC160 digests in binary form.
	 */

	size = VARHDRSZ + 1 + 20;

	d = palloc(size);

	//  quick test for "bc160" or "sha"
	if (a8[0] == 'b' && a8[1] == 'c' &&
	         a8[2] == '1' && a8[3] == '6' && a8[4] == '0' &&
		 a8[5] == 0
	)
		d[4] = UDIG_BC160;
	else if (a8[0] == 's' && a8[1] == 'h' && a8[2] == 'a' && a8[3] == 0)
		d[4] = UDIG_SHA;
	else {
		pfree(d);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(no_algo, a8),
				errhint(ALGO_HINT)
			)
		);
	}

	if (hex2dig20(u, &d[VARHDRSZ + 1])) {
		pfree(d);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(bad_dig, a8, u),
				errhint("hex digest chars are 0-9 or a-f")
			)
		);
	}

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
	int colon;

	d = UDIG_VARDATA(p);

	udig = palloc(3 + 1 + 40 + 1);
	switch (*d) {
	case UDIG_SHA:
		strcpy(udig, "sha");
		colon = 3;
		break;
	case UDIG_BC160:
		strcpy(udig, "bc160");
		colon = 5;
		break;
	default:
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg("udig_out: corrupted udig internal type byte")));
	}
	udig[colon] = ':';
	dig2hex40((unsigned char *)&d[1], udig + colon + 1);
	PG_RETURN_CSTRING(udig);
}

/*
 *  Lexically compare two udigs.  Udigs of the same algorithm will compare by
 *  digests.  UDigs with different algorithms will compare by ascii order
 *  of the digest name.  For example, "bc160" < "sha".
 */
static int
_udig_cmp(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *a, *b;

/*
 *  gcc compiler bug for static variable gets confused doing print style format
 *  checks for variable.
 */
#define _BAD_TYPE	"_udig_cmp: corrupted udig type byte in first operand"

	a = UDIG_VARDATA(a_operand);
	b = UDIG_VARDATA(b_operand);

	/*
	 *  Operands have identical types.
	 */
	if (a[0] == b[0])
		return memcmp(&a[1], &b[1], 20);
	if (a[0] == UDIG_SHA)
		return 1;
	if (a[0] == UDIG_BC160)
		return -1;
	ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED), errmsg(_BAD_TYPE)));

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

	if (a[0] == UDIG_BC160)		//  all bc160 > than all sha1
		return 1;
	if (a[0] == UDIG_SHA)
		return memcmp(&a[1], b_operand, 20);
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

	if (a[0] == UDIG_BC160)
		PG_RETURN_CSTRING("bc160");
	if (a[0] == UDIG_SHA)
		PG_RETURN_CSTRING("sha");
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
	char a8[9], *a;
	static char empty_udig[] = "empty udig";

	udig = PG_GETARG_CSTRING(0);
	if (udig == NULL)
		ereport(PANIC,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
					errmsg("null hex in udig digest")));
	/*
	 *  Extract the algorithm from the udig.
	 */
	a = a8;
	a8[8] = ':';		/* changes to null on successfull parse */
	u = udig;
	u_end = &udig[8];
	while (*u && u < u_end) {
		char c = *u++;

		if (!isgraph(c))
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(no_aprint, c)));
		if (c == ':') {
			*a = a8[8] = 0;		/* successfull parse */
			break;
		}
		*a++ = c;
	}
	if (u == udig || a8[8] == ':')
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg(empty_udig, udig)));
		
	if (strcmp("sha", a8) == 0 || strcmp("bc160", a8) == 0){
		unsigned char d20[20];

		if (hex2dig20(u, d20) == 0)
			PG_RETURN_BOOL(1);
	}
	PG_RETURN_BOOL(0);
}
