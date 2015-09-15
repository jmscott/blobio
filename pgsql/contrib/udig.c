/*
 *  Synopsis:
 *	Implement PostgreSQL UDIG Data Types for Skein, SHA1 and Text.
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
#define UDIG_SK		2

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
Datum	udig_sha_cmp_sk(PG_FUNCTION_ARGS);

/*
 *  Cross type SHA -> SK(ein)
 */
Datum	udig_sha_eq_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_ne_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_gt_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_ge_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_lt_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_le_sk(PG_FUNCTION_ARGS);
Datum	udig_sha_cmp_sk(PG_FUNCTION_ARGS);

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
 *  Core SK (Skein)
 */
Datum	udig_sk_in(PG_FUNCTION_ARGS);
Datum	udig_sk_out(PG_FUNCTION_ARGS);
Datum	udig_sk_eq(PG_FUNCTION_ARGS);
Datum	udig_sk_ne(PG_FUNCTION_ARGS);
Datum	udig_sk_gt(PG_FUNCTION_ARGS);
Datum	udig_sk_ge(PG_FUNCTION_ARGS);
Datum	udig_sk_lt(PG_FUNCTION_ARGS);
Datum	udig_sk_le(PG_FUNCTION_ARGS);
Datum	udig_sk_cmp(PG_FUNCTION_ARGS);

/*
 *  Cross Type SK -> SHA (Skein) functions.
 */
Datum	udig_sk_in(PG_FUNCTION_ARGS);
Datum	udig_sk_out(PG_FUNCTION_ARGS);
Datum	udig_sk_eq_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_ne_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_gt_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_ge_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_lt_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_le_sha(PG_FUNCTION_ARGS);
Datum	udig_sk_cmp_sha(PG_FUNCTION_ARGS);

/*
 *  Cross type Skein (sk) -> UDIG
 */
Datum	udig_sk_eq_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_ne_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_gt_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_ge_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_lt_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_le_udig(PG_FUNCTION_ARGS);
Datum	udig_sk_cmp_udig(PG_FUNCTION_ARGS);

/*
 *  Cross Type SK -> Genric Udig functions.
 */
Datum	udig_sk_cast(PG_FUNCTION_ARGS);

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
 *  Cross Type UDIG,Skein
 */
Datum	udig_eq_sk(PG_FUNCTION_ARGS);
Datum	udig_ne_sk(PG_FUNCTION_ARGS);
Datum	udig_gt_sk(PG_FUNCTION_ARGS);
Datum	udig_ge_sk(PG_FUNCTION_ARGS);
Datum	udig_lt_sk(PG_FUNCTION_ARGS);
Datum	udig_le_sk(PG_FUNCTION_ARGS);
Datum	udig_cmp_sk(PG_FUNCTION_ARGS);

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
 *  Cross SHA, Skein (sk) Operators
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */
PG_FUNCTION_INFO_V1(udig_sha_eq_sk);

Datum
udig_sha_eq_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sha_ne_sk);

Datum
udig_sha_ne_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sha_gt_sk);

Datum
udig_sha_gt_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sha_ge_sk);

Datum
udig_sha_ge_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sha_lt_sk);

Datum
udig_sha_lt_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sha_le_sk);

Datum
udig_sha_le_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sha_cmp_sk);

Datum
udig_sha_cmp_sk(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(1);
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
	case UDIG_SK:
		return 1;				/* sha always > sk */
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
 *  The following routines, sk2nab() and nab2sk(), transform a 32 byte/256 bit 
 *  binary digest to and from a c string expressed in the colloquial
 *  "nab" format.
 *
 *  Nab encoding uses the same character set as the common base64 except '+'
 *  and '/' are swapped with '_' and '@', making the encoded value 
 *  suitable for unix file paths, uri arguments and xml names.
 */
static char n2a[] =
{
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',

	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',

	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',

	'_', '@'
};

/*
 *  Convert 256 bit/32 byte digest to 43 ascii character "nab" format,
 *  a variation on base64 format.  The ascii string is null terminated.
 *
 *  Note:
 *	Note sure if the array lengths should be specified.
 *	A specific, grammatically correct ascii digest implies
 *	a particular byte length.
 */
static void
sk2nab(unsigned char *d32, char *ascii)
{
	char *a;
	unsigned char *d, *d_end;

	a = ascii;
	d = d32;
	d_end = &d32[30];
	while (d < d_end) {
		/*
		 *  Convert 3 byte chunks to 4 ascii chars.
		 *
		 *  Text Value:          M       a	  n
		 *  Byte Value:      01001101 01100001 01101110
		 *   Nab Index:      [ 19 ][  22 ][  5  ][ 46 ]
		 *  Base64 Enc:      [  J ][  M  ][  5  ][  k ] 
		 *
		 */

		/*
		 *  Most significant 6 bits of d[0] stored in a[0].
		 */
		*a++ = 	n2a[(d[0] & 0xfc) >> 2];

		/*
		 *  Least significant 2 bits of d[0] and
		 *  MS 4 Bits of d[1] stored in a[1]
		 */
		*a++ =  n2a[((d[0] & 0x03) << 4) | ((d[1] & 0xf0) >> 4)];

		/*
		 *  LS 4 Bits of d[1] and MS 2 Bits of d[3] stored in a[2].
		 */
		*a++ =  n2a[((d[1] & 0x0f) << 2) | ((d[2] & 0xc0) >> 6)];

		/*
		 *  LS 6 bits of d[2] stored in a[3].
		 */
		*a++ =  n2a[d[2] & 0x3f];
		d += 3;
	}
	/*
	 *  MS 6 bits of d32[30] stored in ascii[40].
	 */
	*a++ = n2a[(d[0] & 0xfc) >> 2];

	/*
	 *  Least significant 2 bits of d32[30] and
	 *  MS 4 Bits of d32[31] stored in ascii[41]
	 */
	*a++ = n2a[((d[0] & 0x03) << 4) | ((d[1] & 0xf0) >> 4)];

	/*
	 *  LS 4 Bits of d32[31] stored in ascii[42].
	 */
	*a++ = n2a[d[1] & 0x0f];
	*a = 0;
}

static int
asc2nab(char c, unsigned char *nab)
{
	if ('0' <=c&&c <= '9')
		*nab = c - '0';
	else if ('A' <=c&&c <= 'Z')
		*nab = (c - 'A') + 10;
	else if ('a' <=c&&c <= 'z')
		*nab = (c - 'a') + 36;
	else if (c == '_')
		*nab = 62;
	else if (c == '@')
		*nab = 63;
	else
		return -1;
	return 0;
}

/*
 *  Convert a 43 character "nab" format to a 32 byte.
 */
static int
nab2sk(char *ascii, unsigned char *digest)
{
	unsigned char n1, n2, n3, n4;
	unsigned char *d;
	char *a;
	int i;

	/*
	 *  Decode the ascii nab string into a 32 byte/256 bit digest.
	 */
	a = ascii;
	d = digest;
	for (i = 0;  i < 40;  i += 4, a += 4, d += 3 ) {
		if (asc2nab(a[0], &n1) || asc2nab(a[1], &n2) ||
		    asc2nab(a[2], &n3) || asc2nab(a[3], &n4))
			return -1;

		d[0] = (n1 << 2) | (0x3 & (n2 >> 4));
		d[1] = ((n2 & 0x3f) << 4) | (0xf & (n3 >> 2));
		d[2] = ((n3 & 0x3) << 6) | n4;
	}
	/*
	 *  Derive bytes 31 and 32 from last 3 
	 *  ascii characters of the digest.
	 */
	if (asc2nab(a[0], &n1) || asc2nab(a[1], &n2) || asc2nab(a[2], &n3))
		return -1;
	d[0] = (n1 << 2) | (0x3 & (n2 >> 4));
	d[1] = (n2 << 4) | (0xf & n3);
	return 0;
}

/*
 *  Convert skein text digest in base64 "nab" format into binary 32 bytes.
 */
PG_FUNCTION_INFO_V1(udig_sk_in);

Datum
udig_sk_in(PG_FUNCTION_ARGS)
{
	char *nab43 = PG_GETARG_CSTRING(0);
	unsigned char *d32;

	d32 = (unsigned char *)palloc(32);

	/*
	 *  Parse the skein udig digest.
	 */
	if (nab2sk(nab43, d32)) {
		pfree(d32);
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
			errmsg(
			      "invalid input syntax for udig sk digest: \"%s\"",
						nab43)));
	}
	PG_RETURN_POINTER(d32);
}

/*
 *  Convert binary, 32 byte digest into 43 character text nab format.
 */
PG_FUNCTION_INFO_V1(udig_sk_out);

Datum
udig_sk_out(PG_FUNCTION_ARGS)
{
	unsigned char *d32 = (unsigned char *)PG_GETARG_POINTER(0);
	char *nab43;

	nab43 = (char *)palloc(44);

	/*
	 *  Parse the skein udig digest.
	 */
	sk2nab(d32, nab43);
	PG_RETURN_CSTRING(nab43);
}

/*
 *  Core Skein Binary Operators:
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */
PG_FUNCTION_INFO_V1(udig_sk_eq);

Datum
udig_sk_eq(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) == 0);
}

PG_FUNCTION_INFO_V1(udig_sk_ne);

Datum
udig_sk_ne(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) != 0);
}

PG_FUNCTION_INFO_V1(udig_sk_gt);

Datum
udig_sk_gt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) > 0);
}

PG_FUNCTION_INFO_V1(udig_sk_ge);

Datum
udig_sk_ge(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) >= 0);
}

PG_FUNCTION_INFO_V1(udig_sk_lt);

Datum
udig_sk_lt(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) < 0);
}

PG_FUNCTION_INFO_V1(udig_sk_le);

Datum
udig_sk_le(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(memcmp(a, b, 32) <= 0);
}

PG_FUNCTION_INFO_V1(udig_sk_cmp);

Datum
udig_sk_cmp(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32((int32)memcmp(a, b, 32));
}

/*
 *  Cross Skein (sk), SHA Operators
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */
PG_FUNCTION_INFO_V1(udig_sk_eq_sha);

Datum
udig_sk_eq_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sk_ne_sha);

Datum
udig_sk_ne_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sk_gt_sha);

Datum
udig_sk_gt_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sk_ge_sha);

Datum
udig_sk_ge_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(0);
}

PG_FUNCTION_INFO_V1(udig_sk_lt_sha);

Datum
udig_sk_lt_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sk_le_sha);

Datum
udig_sk_le_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(1);
}

PG_FUNCTION_INFO_V1(udig_sk_cmp_sha);

Datum
udig_sk_cmp_sha(PG_FUNCTION_ARGS)
{
	PG_RETURN_INT32(-1);
}

/*
 *  Compare sha and generic udig.
 */
static int
_udig_sk_cmp_udig(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *b;

	b = UDIG_VARDATA(b_operand);

	/*
	 *  Consult the type byte in the variable length udig.
	 */
	switch (b[0]) {
	case UDIG_SHA:
		return -1;		/* any sk less than all sha */
	case UDIG_SK:
		return memcmp(a_operand, &b[1], 32);
	}
	ereport(PANIC,
		(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
		errmsg("_udig_sk_cmp_udig: corrupted udig internal type byte"))
	);
	return -1;
}

/*
 *  Cross Type Skein (sk) -> UDIG
 */
PG_FUNCTION_INFO_V1(udig_sk_eq_udig);

Datum
udig_sk_eq_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) == 0);
}

PG_FUNCTION_INFO_V1(udig_sk_ne_udig);

Datum
udig_sk_ne_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) != 0);
}

PG_FUNCTION_INFO_V1(udig_sk_gt_udig);

Datum
udig_sk_gt_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) > 0);
}

PG_FUNCTION_INFO_V1(udig_sk_ge_udig);

Datum
udig_sk_ge_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(udig_sk_lt_udig);

Datum
udig_sk_lt_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) < 0);
}

PG_FUNCTION_INFO_V1(udig_sk_le_udig);

Datum
udig_sk_le_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_sk_cmp_udig(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(udig_sk_cmp_udig);

Datum
udig_sk_cmp_udig(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32(_udig_sk_cmp_udig(a, b));
}

/*
 *  Cross Type SK to UDIG Functions.
 */
PG_FUNCTION_INFO_V1(udig_sk_cast);

/*
 *  Cast a udig_sk to a generic udig.
 */
Datum
udig_sk_cast(PG_FUNCTION_ARGS)
{
	unsigned char *src = (unsigned char *)PG_GETARG_POINTER(0);
	Size size;
	unsigned char *udig;

	/*
	 *  Length + udig type byte + 32 bytes for digest.
	 */
	size = VARHDRSZ + 1 + 32;
	udig = palloc(size);
	udig[VARHDRSZ] = UDIG_SK;
	memcpy(udig + VARHDRSZ + 1, src, 32);
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
	 *  Store Skein and SHA digests in binary form.
	 */
	if (strcmp("sk", a16) == 0) {
		/*
		 *  Length + udig type byte + 32 bytes for digest.
		 */
		size = VARHDRSZ + 1 + 32;	/* size header + type + data */
		d = palloc(size);
		d[4] = UDIG_SK;			/* type */
		/*
		 *  Parse the skein udig digest.
		 */
		if (nab2sk(u, &d[VARHDRSZ + 1])) {
			pfree(d);
			ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				errmsg(
			      "invalid input syntax for udig sk digest: \"%s\"",
								u)));
		}
	} else if (strcmp("sha", a16) == 0) {
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
	case UDIG_SK:
		udig = palloc(2 + 1 + 43 + 1);
		strcpy(udig, "sk:");
		sk2nab((unsigned char *)&d[1], udig + 3);
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
		case UDIG_SK:
			return memcmp(&a[1], &b[1], 32);
		default:
			ereport(PANIC, (errcode(ERRCODE_DATA_CORRUPTED),
						errmsg(BAD_TYPE_GCC_BUG)));
		}
	}
	/*
	 *  Operands differ, so either sha/sk or sk/sha
	 */
	switch (a[0]) {
	case UDIG_SHA:
		return 1;		/* any sha > all sk */
	case UDIG_SK:
		return -1;		/* any sk < all sha */
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
	case UDIG_SK:
		return -1;
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

/*
 *  Cross Type UDIG, Skein Functions
 *	=		equal
 *	!=		not rqual
 *	>		greater than
 *	>=		greater than or equal
 *	<		less than
 *	<=		less than or equal
 */

/*
 *  Compare text udig and sk.
 */
static int
_udig_cmp_sk(unsigned char *a_operand, unsigned char *b_operand)
{
	unsigned char *a;

	a = UDIG_VARDATA(a_operand);
	
	/*
	 *  Left operand is also sk type, so just compare bytes.
	 */
	switch (a[0]) {
	case UDIG_SHA:			/* any sha  > all sk */
		return 1;
	case UDIG_SK:
		return memcmp(&a[1], b_operand, 32);
	}
	ereport(PANIC,
		(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
		errmsg("_udig_cmp_sk: corrupted udig internal type byte")));
	return -1;
}

PG_FUNCTION_INFO_V1(udig_eq_sk);

Datum
udig_eq_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) == 0);
}

PG_FUNCTION_INFO_V1(udig_ne_sk);

Datum
udig_ne_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) != 0);
}

PG_FUNCTION_INFO_V1(udig_gt_sk);

Datum
udig_gt_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) > 0);
}

PG_FUNCTION_INFO_V1(udig_ge_sk);

Datum
udig_ge_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(udig_lt_sk);

Datum
udig_lt_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) < 0);
}

PG_FUNCTION_INFO_V1(udig_le_sk);

Datum
udig_le_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(_udig_cmp_sk(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(udig_cmp_sk);

Datum
udig_cmp_sk(PG_FUNCTION_ARGS)
{
	unsigned char *a = (unsigned char *)PG_GETARG_POINTER(0);
	unsigned char *b = (unsigned char *)PG_GETARG_POINTER(1);

	PG_RETURN_INT32(_udig_cmp_sk(a, b));
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
	case UDIG_SK:
		PG_RETURN_CSTRING("sk");
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
	/*
	 *  Store Skein and SHA digests in binary form.
	 */
	if (strcmp("sk", a16) == 0) {
		unsigned char d[VARHDRSZ + 1 + 32];

		d[4] = UDIG_SK;			/* type */
		/*
		 *  Parse the skein udig digest.
		 */
		if (nab2sk(u, &d[VARHDRSZ + 1]))
			goto no;
	} else if (strcmp("sha", a16) == 0) {
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
