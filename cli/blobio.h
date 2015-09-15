/*
 *  Synopsis:
 *	C header for Command Line Interface to BlobIO.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#ifndef BLOBIO_H
#define BLOBIO_H

struct request
{
	char		host[1025];
	short		port;
	char		verb[5];

	int		server_fd;

	char		algorithm[9];
	char		digest[129];

	int		in_fd;
	int		out_fd;

	void 		*module;
	void		*open_data;
};

struct module
{
	char	*algorithm;

	int	(*open)(struct request *);

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
	int	(*get_update)(struct request *,
					unsigned char *src, int src_size);

	/*
	 *  put:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 */
	int	(*put_update)(struct request *,
					unsigned char *src, int src_size);
	/*
	 *  take:
	 *	Update the local digest with a portion of the blob read from a 
	 *	service.  Returns 0 if the full blob has been seen, 1 if 
	 *	more needs to be read, < 0 if an error occured.
	 *	
	 *	After a succesful take, took() is called with the reply
	 *	from the server.
	 */
	int	(*take_update)(struct request *,
					unsigned char *bite, int bite_size);
	int	(*took)(struct request *, char *reply);

	/*
	 *  give:
	 *	give_update() updates the local digest with a portion of the 
	 *	blob read from a service.  Returns 0 if the full blob has 
	 *	been seen, 1 if more needs to be read, < 0 if an error occured.
	 *	
	 *	After the blob has been successfully given to the server,
	 *	the gave() is called with the remote reply.
	 */
	int	(*give_update)(struct request *,
					unsigned char *src, int src_size);
	/*
	 *  Is gave still used?
	 */
	int	(*gave)(struct request *, char *reply);

	/*
	 *  Digest the standard input.
	 */
	int	(*eat)(struct request *);

	/*
	 *  Is the digest valid?
	 *
	 *	0	if syntacically incorrect
	 *	1	if syntactically correct and non empty
	 *	2	if represents the empty blob
	 */
	int	(*is_digest)(char *digest);

	/*
	 *  Is the digest the well-known empty digest for this algorithm
	 *  Returns 1 if empty, 0 if not empty.
	 */
	int	(*is_empty)(char *digest);

	int	(*close)(struct request *);
};

extern int	tracing;
extern void	trace(char *);
extern void	trace2(char *, char *);
extern void	dump(unsigned char *buf, int buf_size, char direction);

//  suppress unused arg warning arcross both gcc and clang
#define UNUSED_ARG(x)	(void)(x)

#endif
