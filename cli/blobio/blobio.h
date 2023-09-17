/*
 *  Synopsis:
 *	C header for Command Line CLient Interface to BlobIO.
 */
#ifndef BLOBIO_H
#define BLOBIO_H

#include <sys/types.h>
#include <unistd.h>

#include "jmscott/libjmscott.h"

/*
 *  compile code to include network/fs tracing option --trace
 *
 *  Note:
 *	consider adding query argument to enable tracing:
 *	&trace=[0|1|<path/to/output>
 */
#define COMPILE_TRACE

#define MAX_ATOMIC_MSG	JMSCOTT_ATOMIC_WRITE_SIZE

#ifdef COMPILE_TRACE

extern int	tracing;

//  Note: change TRACE to expression: (tracing && trace(...))

#define TRACE(msg)		if (tracing) trace2(			\
					(char *)__FUNCTION__, (char *)msg\
				);
#define TRACE2(msg1,msg2) 	if (tracing) trace3(			\
					(char *)__FUNCTION__,		\
					(char *)msg1,			\
					(char *)msg2			\
				);
#define TRACE3(msg1,msg2,msg3)	if (tracing) trace4(			\
					(char *)__FUNCTION__,		\
					(char *)msg1,			\
					(char *)msg2,			\
					(char *)msg3			\
				);
#define TRACE4(msg1,msg2,msg3,msg4) if (tracing) trace5(		\
					(char *)__FUNCTION__,		\
					(char *)msg1,			\
					(char *)msg2,			\
					(char *)msg3,			\
					(char *)msg4			\
				);
#define TRACE_ULL(msg,ull)	if (tracing) trace_ull(msg,(unsigned long long)ull);
#define TRACE_LL(msg,ll)	if (tracing) trace_ll(msg,(long long)ll);


#else

/*  compile out any vestiage of tracing */

#define TRACE(msg)
#define TRACE2(msg1,msg2)
#define TRACE3(msg1,msg2,msg3)
#define TRACE4(msg1,msg2,msg3,msg4)
#define TRACE_ULL(msg1,ull)
#define TRACE_LL(msg1,ll)

#endif

#define BLOBIO_MAX_URI_QARGS	5

#define BLOBIO_MAX_FS_PATH	255	//  chars, not bytes
#define BLOBIO_MAX_BRR_SIZE	455	//  not counting new-line&null

/*
 *  Hash digest driver.
 */
struct digest
{
	char	*algorithm;

	char	*(*init)();

	/*
	 *  The following get/put/take/give callbacks are called incrementally
	 *  as the blob is being digested while talking to a blobio service.
	 */

	/*
	 *  get:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns "0" if the full blob has been seen, "1" if 
	 *	more needs to be read, otherwise an error string.  The returned
	 *	error string must have length > 1.
	 */
	char	*(*get_update)(unsigned char *src, int src_size);

	/*
	 *  put:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 */
	char	*(*put_update)(unsigned char *src, int src_size);

	/*
	 *  take:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 *	
	 *	After a succesful take, took() is called with the reply
	 *	from the server.
	 */
	char	*(*take_update)(unsigned char *bite, int bite_size);
	char	*(*took)(char *reply);

	/*
	 *  give:
	 *	give_update() updates the local digest with a portion of the 
	 *	blob read from a service.  Returns 0 if the full blob has 
	 *	been seen, 1 if more needs to be read, < 0 if an error occured.
	 *	
	 */
	char	*(*give_update)(unsigned char *src, int src_size);

	/*
	 *  Digest the input stream
	 */
	char	*(*eat_input)(int fd);

	/*
	 *  Is the digest well formed syntactically?
	 *
	 *	0	if syntacically incorrect
	 *	1	if syntactically correct and non empty
	 */
	int	(*syntax)();

	/*
	 *  Is the digest the well-known empty digest for this algorithm
	 *  Returns 1 if empty, 0 if not empty.
	 */
	int	(*empty)();

	/*
	 *  The digest of the empty blob.
	 */
	char	*(*empty_digest)();

	int	(*close)();

	/*
	 *  Convert the ascii digest to name of a file and
	 *  copy to the buffer that "name" points to.
	 *  Typically the name is simply the digest value.
	 *  No file is created in the file system.
	 */
	char	*(*fs_name)(char *name, int size);

	/*
	 *  Convert the ascii digest to a path relative to data/ that points
	 *  to the file the blob is stored in.
	 */
	char	*(*fs_path)(char *file_path, int size);

	/*
	 *  Convert the ascii digest to a path relative to data/ that points
	 *  to the directory the blob is stored in.  See fs_path().
	 */
	char	*(*fs_dir_path)(char *dir_path, int size);
};

struct brr
{
	struct timespec		start_time;
	char			transport[(8+1+128) + 1];
	char			verb[8+1];
	char			udig[(8+1+128)+1];		
	char			chat_history[8+1];
	long long		blob_size;
	struct timespec		end_time;

	int			log_fd;
};

struct service
{
	char		*name;
	char		end_point[256];

	//  the query portion of the service, unparsed

	char		query[256];

	char		*(*end_point_syntax)(char *end_point);

	char		*(*open)(void);

	//  Note: close() ought to be called during error!
	char		*(*close)(void);

	char		*(*get)(int *ok_no);
	char		*(*eat)(int *ok_no);
	char		*(*put)(int *ok_no);
	char		*(*take)(int *ok_no);
	char		*(*give)(int *ok_no);
	char		*(*roll)(int *ok_no);
	char		*(*wrap)(int *ok_no);

	char		*(*brr_frisk)(struct brr *);

	struct digest	*digest;
};

//  replace jmscott_strcat()s with these more readable.
extern char	*bufcat(char *tgt, int tgtsize, const char *src);
extern char	*buf2cat(char *tgt, int tgtsize,
				const char *src, const char *src2);
extern char	*buf3cat(char *tgt, int tgtsize,
			const char *src, const char *src2, const char *src3);
extern char	*buf4cat(char *tgt, int tgtsize,
			const char *src, const char *src2,
			const char *src3, const char *src4);

extern void	die(char *msg);
extern void	die2(char *msg1, char *msg2);
extern void	die3(char *msg1, char *msg2, char *msg3);
extern void	die_timeout(char *msg);
extern int	tracing;
extern void	trace(char *);
extern void	trace2(char *, char *);
extern void	trace3(char *, char *, char *);
extern void	trace4(char *, char *, char *, char *);
extern void	trace5(char *, char *, char *, char *, char *);
extern void	trace_ull(char *, unsigned long long);
extern void	trace_ll(char *, long long);
extern void	hexdump(unsigned char *buf, int buf_size, char direction);

extern int	uni_close(int fd);
extern int	uni_link(const char *oldpath, const char *newpath);
extern int	uni_mkdir(const char *path, mode_t mode);
extern int	uni_open(const char *pathname, int flags);
extern int	uni_open_mode(const char *pathname, int flags, int mode);
extern int	uni_unlink(const char *path);
extern int	uni_write_buf(int fd, const void *buf, size_t count);
extern ssize_t	uni_read(int fd, void *buf, size_t count);
extern ssize_t	uni_write(int fd, const void *buf, size_t count);

extern char	*BLOBIO_SERVICE_frisk_qargs(char *query);

void		BLOBIO_SERVICE_get_brr_mask(char *query, unsigned char *mask);
void		BLOBIO_SERVICE_get_algo(char *query, char *algo);

extern struct digest	*find_digest(char *algorithm);

extern char		udig[8 + 1 + 128 + 1];
extern char		verb[8 + 1];
extern char		algo[8 + 1];
extern char		algorithm[8 + 1];
extern char		chat_history[2 + 1 + 2 + 1 + 2 + 1];
extern struct digest	*digest_module;
extern int 		output_fd;
extern char 		*output_path;
extern int 		input_fd;
extern char 		*input_path;
extern char 		*null_device;
extern long long	blob_size;
extern char		ascii_digest[129];
extern char		transport[8+1+128 + 1];
extern int		trust_fs;

extern unsigned char	brr_mask;
extern char *		brr_service(struct service *);
extern char *		brr_mask2ascii(unsigned char mask);
extern int		brr_mask_is_set(char *verb, unsigned char mask);

#endif
