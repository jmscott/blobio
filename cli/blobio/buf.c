/*
 *  Strings handling to be replaced with libjmscott.a
 */
#include "blobio.h"

char *
bufcat(char *tgt, int tgtsize, const char *src)
{
	jmscott_strcat(tgt, tgtsize, src);
	return tgt;
}

/*
 *  Synopsis:
 *	Efficently concatenate two strings
 */
char *
buf2cat(char *tgt, int tgtsize, const char *src1, const char *src2)
{
	char *next;
	int nbytes;

	next = bufcat(tgt, tgtsize, src1);
	nbytes = next - tgt;
	if (tgtsize - nbytes <= 0)
		return next;
	return bufcat(next, tgtsize - nbytes, src2);
}

/*
 *  Synopsis:
 *	Efficently concatenate three strings
 */
char *
buf3cat(char *tgt, int tgtsize,
	const char *src1, const char *src2, const char *src3)
{
	char *next;
	int nbytes;

	next = bufcat(tgt, tgtsize, src1);
	nbytes = next - tgt;
	if (tgtsize - nbytes <= 0)
		return next;
	return buf2cat(next, tgtsize - nbytes, src2, src3);
}

/*
 *  Synopsis:
 *	Efficently concatenate four strings
 */
char *
buf4cat(char *tgt, int tgtsize,
	const char *src1, const char *src2, const char *src3, const char *src4)
{
	char *next;
	int nbytes;

	next = bufcat(tgt, tgtsize, src1);
	nbytes = next - tgt;
	if (tgtsize - nbytes <= 0)
		return next;
	return buf3cat(next, tgtsize - nbytes, src2, src3, src4);
}
