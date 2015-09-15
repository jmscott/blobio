/*
 *  Synopsis:
 *	Routines to transform a 32 byte/256 bit digest to and from an
 *	ascii string in the "nab" format.
 *
 *  Description:
 *	Nab encoding uses the same character set as the common base64 with '+' 
 *	and '/' being swapped with '_' and '@', making the encoded value 
 *	suitable for unix file paths, uri arguments and xml names.
 *
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#include <string.h>
#include <stdio.h>
#include <errno.h>

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
 *  Convert 256 bit digest to 43 ascii character "nab" format,
 *  a variation on base64 format.  The ascii string is null terminated.
 *
 *  Note:
 *	Note sure if the array lengths should be specified.
 *	A specific, grammatically correct ascii digest implies
 *	a particular byte length.
 */
void
skein_digest2nab(unsigned char *digest, char *ascii)
{
	char *a;
	unsigned char *d, *d_end;

	a = ascii;
	d = digest;
	d_end = &digest[30];
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
	 *  MS 6 bits of digest[30] stored in ascii[40].
	 */
	*a++ = n2a[(d[0] & 0xfc) >> 2];
	/*
	 *  Least significant 2 bits of digest[30] and
	 *  MS 4 Bits of digest[31] stored in ascii[41]
	 */
	*a++ = n2a[((d[0] & 0x03) << 4) | ((d[1] & 0xf0) >> 4)];
	/*
	 *  LS 4 Bits of digest[31] stored in ascii[42].
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
 *  Is the digest a valid nabified digest.
 *  Return:
 *
 *	0	if not syntacically correct
 *	1	if syntactically correct
 *	2	if digest for zero length blob
 */
int
skein_is_nab(char *ascii)
{
	int i;
	unsigned char c;

	for (i = 0;  i < 43 && ascii[i];  i++)
		if (asc2nab(ascii[i], &c))
			return 0;
	if (i != 43 || ascii[43] != 0)
		return 0;
	return strcmp(ascii, "7W3GlyqKvCUqLw03HHrwZ63l0DGGfSH3zV0xij24Yk3")==0?
		2 : 1;
}

/*
 *  Convert a 43 character "nab" format to a 32 byte, binary database.
 */
int
skein_nab2digest(char *ascii, unsigned char *digest)
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
			return EINVAL;

		d[0] = (n1 << 2) | (0x3 & (n2 >> 4));
		d[1] = ((n2 & 0x3f) << 4) | (0xf & (n3 >> 2));
		d[2] = ((n3 & 0x3) << 6) | n4;
	}
	/*
	 *  Derive bytes 31 and 32 from last 3 
	 *  ascii characters of the digest.
	 */
	if (asc2nab(a[0], &n1) || asc2nab(a[1], &n2) || asc2nab(a[2], &n3))
		return EINVAL;
	d[0] = (n1 << 2) | (0x3 & (n2 >> 4));
	d[1] = (n2 << 4) | (0xf & n3);
	return 0;
}
