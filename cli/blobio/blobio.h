/*
 *  Synopsis:
 *	C header for Command Line Interface to BlobIO.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#ifndef BLOBIO_H
#define BLOBIO_H

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
	int	(*get_update)(unsigned char *src, int src_size);

	/*
	 *  put:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 */
	int	(*put_update)(unsigned char *src, int src_size);
	/*
	 *  take:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 *	
	 *	After a succesful take, took() is called with the reply
	 *	from the server.
	 */
	int	(*take_update)(unsigned char *bite, int bite_size);
	int	(*took)(char *reply);

	/*
	 *  give:
	 *	give_update() updates the local digest with a portion of the 
	 *	blob read from a service.  Returns 0 if the full blob has 
	 *	been seen, 1 if more needs to be read, < 0 if an error occured.
	 *	
	 *	After the blob has been successfully given to the server,
	 *	the gave() is called with the remote reply.
	 */
	int	(*give_update)(unsigned char *src, int src_size);
	/*
	 *  Is gave still used?
	 */
	int	(*gave)(char *reply);

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

	int	(*close)();

	/*
	 *  Convert the ascii digest to file system path.
	 */
	char	*(*fs_path)(char *path, int size);

	/*
	 *  Maximum length file system path to digest.
	 */
	char	*(*fs_path_length)(char *path, int size);
};

struct service
{
	char		*name;
	char		*end_point;

	char		*(*end_point_syntax)(char *end_point);
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

extern void	die(int status, char *msg);
extern void	bufcat(char *tgt, int tgtsize, const char *src);
extern void	ecat(char *buf, int size, char *msg);

extern int	tracing;
extern void	trace(char *);
extern void	trace2(char *, char *);
extern void	trace3(char *, char *, char *);
extern void	trace4(char *, char *, char *, char *);
extern void	hexdump(unsigned char *buf, int buf_size, char direction);

extern int	uni_close(int fd);
extern ssize_t	uni_write(int fd, const void *buf, size_t count);
extern ssize_t	uni_read(int fd, void *buf, size_t count);
extern int	uni_write_buf(int fd, const void *buf, size_t count);
extern int	uni_open(const char *pathname, int flags);
extern int	uni_open_mode(const char *pathname, int flags, int mode);

//  suppress unused arg warning arcross both gcc and clang
#define UNUSED_ARG(x)	(void)(x)

#endif
