/*
 * Synopsis:
 *  	Reasonably memory safe and semantically simple string concatenators.
 *  Usage:
 *  	buf[0] = 0
 *  	bufcat(buf, sizeof buf, "hello, world");
 *  	bufcat(buf, sizeof buf, ": ");
 *  	bufcat(buf, sizeof buf, "good bye, cruel world");
 *	...
 *	slen = bufcat(buf, sizeof buf, "hello, world") - buf;
 *	...
 *	buf2cat(buf, sizeof buf, "hello, world", msg);
 *	buf3cat(buf, sizeof buf, "hello, world", ": ", "good bye, cruel world");
 *
 *  Note:
 *	No indication of target being longer than src.
 *
 *	To determine that no all bytes were copied, the caller must check
 *	returned pointer to null byte.
 *
 *	No check that tgtsize > 0.
 */
char *
bufcat(char *tgt, int tgtsize, const char *src)
{
	//  find null terminated end of target buffer

	while (*tgt++)
		--tgtsize;
	--tgt;

	//  copy non-null src bytes, leaving room for trailing null

	while (--tgtsize > 0 && *src)
		*tgt++ = *src++;

	// target always null terminated
	*tgt = 0;

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
