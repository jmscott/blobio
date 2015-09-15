/*
 *  Synopsis:
 *	Common c header for libblobio library
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#ifndef LIB_BLOBIO_H
#define LIB_BLOBIO_H

/*
 *  Define an implementation of an incremental digest/hash module.
 *
 *  The algorithm must be capable of copying and finalizng a partial state.
 */
struct blobio_digest_module
{
	char	*algorithm;

	/*
	 *  How many bytes are in a state.
	 */
	unsigned	state_nbyte;

	int	(*init)(void *state);
	/*
	 *  Clone an existing state into a new state suitable for using in
	 *  an incremental update.
	 */
	void	(*copy)(void *state_src, void *state_tgt);
	/*
	 *  NOTE:
	 *	Incremental hashing requires updates that are a 
	 *	multiple of 8 bits.  See:
	 *
	 *	    http://csrc.nist.gov/groups/ST/hash/documents/SHA3-C-API.pdf
	 */
	int	(*update)(
			void *state,
			unsigned char *bytes,
			unsigned long long nbits
		);
	int	(*final)(void *state, unsigned char *digest); 

	/*
	 *  Convert unsigned char digest to null terminated, ascii string.
	 */
	void	(*digest2ascii)(unsigned char *digest, char *ascii);

	/*
	 *  Ascii version of digest.
	 *
	 *  Note:
	 *	Should this be a cstring type?
	 */
	int	ascii_nchar;		// includes null byte

	/*
	 *  Convert null terminated, ascii digest to unsigned char array.
	 */
	int	(*ascii2digest)(char *ascii, unsigned char *digest);
	int	digest_nbyte;

	/*
	 *  Is a the ascii string a syntacally correct digest.
	 */
	int	(*is_ascii_digest)(char *ascii);

	char	*empty_ascii;
};

void blobio_digest2nab		(unsigned char *digest, char *ascii);
int blobio_nab2digest		(char *ascii, unsigned char *digest);
struct blobio_digest_module *	blobio_digest_module_get(char *algorithm);

#endif
