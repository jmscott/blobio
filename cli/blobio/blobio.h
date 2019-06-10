/*
 *  Synopsis:
 *	C header for Command Line Interface to BlobIO.
 */
#ifndef BLOBIO_H
#define BLOBIO_H

#include <sys/types.h>
#include <unistd.h>

#define COMPILE_TRACE

#ifndef PIPE_MAX
#define PIPE_MAX		4096
#endif

#ifdef COMPILE_TRACE

#define TRACE(msg) if (tracing) trace(msg);
#define TRACE2(msg1,msg2) if (tracing) trace2(msg1, msg2);
#define TRACE3(msg1,msg2,msg3) if (tracing) trace2(msg1, msg2, msg3);
#define TRACE4(msg1,msg2,msg3,msg4) if (tracing) trace2(msg1, msg2, msg3, msg4);

#else

#define TRACE(msg)
#define TRACE2(msg1,msg2)
#define TRACE3(msg1,msg2,msg3)
#define TRACE4(msg1,msg2,msg3,msg4)

#endif

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
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
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
	char	*(*eat_input)();

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
	 *  Convert the ascii digest to name of a file
	 */
	char	*(*fs_name)(char *name, int size);

	/*
	 *  Convert the ascii digest to a path to the file.
	 */
	char	*(*fs_path)(char *path, int size);

	/*
	 *  Make a file system path from the ascii digest.
	 *  Path points to the parent of the first directory
	 *  build from the hash digest.
	 */
	char	*(*fs_mkdir)(char *path, int size);
};

struct service
{
	char		*name;
	char		end_point[256];

	char		*(*end_point_syntax)(char *end_point);

	/*  Note:  why no close output? */

	char		*(*open_output)();

	char		*(*open)();
	char		*(*close)();
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

extern void	die(int status, char *msg);
extern int	tracing;
extern void	trace(char *);
extern void	trace2(char *, char *);
extern void	trace3(char *, char *, char *);
extern void	trace4(char *, char *, char *, char *);
extern void	hexdump(unsigned char *buf, int buf_size, char direction);

extern int	uni_access(const char *path, int mode);
extern int	uni_close(int fd);
extern int	uni_link(const char *oldpath, const char *newpath);
extern int	uni_mkdir(const char *path, mode_t mode);
extern int	uni_open(const char *pathname, int flags);
extern int	uni_open_mode(const char *pathname, int flags, int mode);
extern int	uni_unlink(const char *path);
extern int	uni_write_buf(int fd, const void *buf, size_t count);
extern ssize_t	uni_read(int fd, void *buf, size_t count);
extern ssize_t	uni_write(int fd, const void *buf, size_t count);

#endif
