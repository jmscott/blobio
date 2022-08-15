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
 */
#define COMPILE_TRACE

#define MAX_ATOMIC_MSG	JMSCOTT_ATOMIC_WRITE_SIZE

#ifdef COMPILE_TRACE

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

#else

/*  compile out any vestiage of traceing */
#define TRACE(msg)
#define TRACE2(msg1,msg2)
#define TRACE3(msg1,msg2,msg3)
#define TRACE4(msg1,msg2,msg3,msg4)

#endif

#define BLOBIO_MAX_URI_QARGS	5

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
	 *	After the blob has been successfully given to the server,
	 *	the gave() is called with the remote reply.
	 */
	char	*(*give_update)(unsigned char *src, int src_size);
	/*
	 *  Is gave still used?
	 */
	char	*(*gave)(char *reply);

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
	 *  Convert the ascii digest to the full a path to file
	 *  stored locally and copy to the buffer that "path"
	 *  points to.  No file is created in the file system.
	 */
	char	*(*fs_path)(char *path, int size);

	/*
	 *  Make a file system path from the ascii digest
	 *  and build all directories in the path.  The full
	 *  path is copied to the buffer that "path" points to.
	 *  Effectively, "fs_mkdir" is equivalent to "mkdir -p".
	 *
	 *  Note:
	 *	Why not rename "fs_mkdir()" to simply "mkdir()"?
	 *
	 *	Can fs_mkdir() be factored out of digest driver?
	 */
	char	*(*fs_mkdir)(char *path, int size);
};

struct service
{
	char		*name;
	char		end_point[256];
	char		query[256];

	char		*(*end_point_syntax)(char *end_point);

	/*  Note:  why no close output? */

	char		*(*open_output)(void);

	char		*(*open)(void);
	char		*(*close)(void);
	char		*(*get)(int *ok_no);
	char		*(*eat)(int *ok_no);
	char		*(*put)(int *ok_no);
	char		*(*take)(int *ok_no);
	char		*(*give)(int *ok_no);
	char		*(*roll)(int *ok_no);
	char		*(*wrap)(int *ok_no);

	struct digest	*digest;
};

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
extern void	die_timeout(char *msg);
extern int	tracing;
extern void	trace(char *);
extern void	trace2(char *, char *);
extern void	trace3(char *, char *, char *);
extern void	trace4(char *, char *, char *, char *);
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

extern char	*BLOBIO_SERVICE_frisk_query(char *query);
extern char	*BLOBIO_SERVICE_get_tmo(char *query, int *tmo);
extern char	*BLOBIO_SERVICE_get_BR(char *query, char *BR);
extern char	*BLOBIO_SERVICE_get_brr(char *query, char *brr);
extern char	*BLOBIO_SERVICE_get_algo(char *query, char *brr_path);

extern void	brr_write();

extern struct digest	*find_digest(char *algorithm);

#endif
