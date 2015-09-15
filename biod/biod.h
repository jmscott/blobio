/*
 *  Synopsis:
 *	Global C header file for biod program.
 *  Blame:
 *  	jmscott@setspace.com
 *  	setspace@gmail.com
 */
#ifndef BIOD_H
#define BIOD_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

#define MAX_DIGEST_SIZE		128
#define MAX_ALGORITHM_SIZE	8
#define MAX_UDIG_SIZE		(MAX_DIGEST_SIZE + MAX_ALGORITHM_SIZE + 1)

#ifndef MAX_FILE_PATH_LEN
#define MAX_FILE_PATH_LEN	256
#endif

typedef unsigned long long	u8;
typedef unsigned int		u4;
typedef unsigned short		u2;
typedef unsigned char		u1;

/*
 *  Must be 255 since the interprocess message stream use leading byte for
 *  length. Must also be < PIPE_MAX
 */
#define MSG_SIZE		255

struct request
{
	int	client_fd;		/* pipe to the client */

	/*
	 *  What verb this request is handling from client.
	 *
	 *	get/put/give/take/eat/wrap/roll
	 */
	char	*verb;

	/*
	 *  Name of digest algorithm.
	 */
	char	*algorithm;

	/*
	 *  The digest value as printable character string sent by client.
	 */
	char	*digest;

	/*
	 *  Number of bytes scanned ahead during the intial read of request.
	 */
	unsigned char	*scan_buf;
	int		scan_size;

	struct timespec	start_time;		/* fork() start time */
	struct timespec	ttfb_time;		/* time to first byte */
	struct timespec	end_time;		/* child exit time */

	/*
	 *  Private data created by the digest module
	 *  during the open() call.
	 */
	void	*open_data;

	struct sockaddr_in	remote_address;
	socklen_t		remote_len;
	struct sockaddr_in	bind_address;

	/*
	 *  Network flow.  For example, tcp4/remoteip:port;bindip:port
	 */
	char			netflow[129];

	/*
	 *  Track the ok/no chat session between client and biod.
	 */
	char			chat_history[10];

	u8	blob_size;

	u4	read_timeout;	/* # seconds before a request read times out */
	u4	write_timeout;	/* # seconds before a request write times out */

	void	*module_boot_data;	/* allocated by module.boot() */
};
/*
 *  Process Exit Class: Bits 1 and 2 of the request child exit status
 */
#define REQUEST_EXIT_STATUS_SUCCESS	0
#define REQUEST_EXIT_STATUS_ERROR	1
#define REQUEST_EXIT_STATUS_TIMEOUT	2
#define REQUEST_EXIT_STATUS_FAULT	3

/*
 *  Verb: Bits 3, 4 and 5 of the of the request child exit status.
 */
#define REQUEST_EXIT_STATUS_GET		1
#define REQUEST_EXIT_STATUS_PUT		2
#define REQUEST_EXIT_STATUS_GIVE	3
#define REQUEST_EXIT_STATUS_TAKE	4
#define REQUEST_EXIT_STATUS_EAT		5
#define REQUEST_EXIT_STATUS_WRAP	6
#define REQUEST_EXIT_STATUS_ROLL	7

/*
 *  Client Chat History: bits 6 and 7
 */
#define REQUEST_EXIT_STATUS_CHAT_OK	0
#define REQUEST_EXIT_STATUS_CHAT_NO	1
#define REQUEST_EXIT_STATUS_CHAT_NO2	2
#define REQUEST_EXIT_STATUS_CHAT_NO3	3

struct digest_module
{
	char	*name;

	/*
	 *  Boot up a digest module.
	 */
	int	(*boot)();

	/*
	 *  Open a request from a client.
	 */
	int	(*open)(struct request *);

	/*
	 *  Get a blob from this server.
	 */
	int	(*get)(struct request *);

	/*
	 *  Client takes a blob from a server.
	 *  Server may forget the blob.
	 */
	int	(*take_blob)(struct request *);
	int	(*take_reply)(struct request *, char *reply);

	/*
	 *  Put a blob on this server.
	 */
	int	(*put)(struct request *);

	/*
	 *  Give a blob to this server.
	 *  Client may forget the blob.
	 */
	int	(*give_blob)(struct request *);
	int	(*give_reply)(struct request *, char *reply);

	/*
	 *  Eat/verify an existing blob.
	 */
	int	(*eat)(struct request *);

	/*
	 *  Digest a stream up to EOF and copy text digest as c string to
	 *  memory referenced by char *digest.
	 */
	int	(*digest)(struct request *, int fd, char *digest, int do_put);

	/*
	 *  Write a blob to a stream and return 0 if blob exists and is written
	 *  on the stream, otherwise -1.
	 */
	int	(*write)(struct request *, int fd);

	/*
	 *  Is the digest valid, empty, or invalid.
	 *  Return:
	 *	0	if invalid
	 *	1	if valid and non empty
	 *	2	if valid and empty
	 */
	int	(*is_digest)(char *digest);

	/*
	 *  Close a client request
	 */
	int	(*close)(struct request *, int status);

	/*
	 *  Shutdown a digest module.
	 */
	int	(*shutdown)();
};

extern void	info(char *msg);
extern void	info2(char *msg1, char *msg2);
extern void	info3(char *msg1, char *msg2, char *msg3);
extern void	info4(char *msg1, char *msg2, char *msg3, char *msg4);

extern void	warn(char *msg);
extern void	warn2(char *msg1, char *msg2);
extern void	warn3(char *msg1, char *msg2, char *msg3);
extern void	warn4(char *msg1, char *msg2, char *msg3, char *msg4);

extern void	error(char *msg);
extern void	error2(char *msg1, char *msg2);
extern void	error3(char *msg1, char *msg2, char *msg3);
extern void	error4(char *msg1, char *msg2, char *msg3, char *msg4);
extern void	error5(char *msg1, char *msg2,char *msg3,char *msg4,char *msg5);

extern void	panic(char *msg);
extern void	panic2(char *msg1, char *msg2);
extern void	panic3(char *msg1, char *msg2, char *msg3);
extern void	panic4(char *msg1, char *msg2, char *msg3, char *msg4);

/*
 *  Event logging to log/biod.log, defined in log.c
 */
extern void	log_close();
extern void	log_open();
extern int	log_roll();
extern int	log_roll_final();
extern int	log_stat(struct stat *);
extern void	log_write(char *buf);
extern void	log_roll_free_all();
extern char	*log_strcpy2(char *buf, int buf_size, char *msg1, char *msg2);
extern char	*log_strcpy3(char *buf, int buf_size,
					char *msg1, char *msg2, char *msg3);
extern char	*log_strcpy4(char *buf, int buf_size,
				char *msg1, char *msg2, char *msg3, char *msg4);
extern char	*log_strcat2(char *buf, int buf_size, char *msg1, char *msg2);
extern char	*log_strcat3(char *buf, int buf_size,
					char *msg1, char *msg2, char *msg3);

/*
 *  Generic unix i/o, defined in io.c.  Ought to be name fs.c
 */
extern int	is_file(char *path);
extern int	burp_text_file(char *buf, char *path);
extern int	slurp_text_file(char *path, char *buf, int buf_size);
extern int	write_blob(struct request *, unsigned char *buf, int buf_size);
extern int	write_ok(struct request *rp);
extern int	write_no(struct request *rp);
extern int	read_buf(int fd, unsigned char *buf, int buf_size,
			 unsigned timeout);
extern int	write_buf(int fd, unsigned char *buf, int buf_size,
			unsigned timeout);
extern int	read_blob(struct request *, unsigned char *buf, int buf_size);
extern char	*read_reply(struct request *);
extern char	*addr2text(u_long addr);

extern char	*sig_name(int sig);

extern char	*BLOBIO_ROOT;		/* BLOBIO_ROOT environment variable */

/*
 *  Wrappers around various interuptable unix system calls
 */
extern int		io_close(int fd);
extern ssize_t		io_read(int fd, void *buf, size_t count);
extern ssize_t		io_write(int fd, void *buf, size_t count);
extern int		io_pipe(int fds[2]);
extern int		io_open(char *path, int flags, int mode);
extern int		io_open_append(char *path, int truncate);
extern int		io_accept(int listen_fd, struct sockaddr *addr,
					socklen_t *len, int *client_fd,
					unsigned timeout);
extern int		io_select(int nfds, fd_set *readfds, fd_set *writefds,
					fd_set *errorfds,
					struct timeval *timeout);
extern pid_t		io_waitpid(pid_t pid, int *stat_loc, int options);
extern int		io_rename(char *old_path, char *new_path);
extern DIR		*io_opendir(char *path);
extern struct dirent	*io_readdir(DIR *dirp);
extern int		io_unlink(const char *path);
extern int		io_rmdir(const char *path);
extern int		io_mkfifo(char *path, mode_t mode);
extern int		io_chmod(char *path, mode_t mode);
extern off_t		io_lseek(int fd, off_t offset, int whence);
extern int		io_fstat(int fd, struct stat *buf);
extern int		io_closedir(DIR *dirp);
extern int		io_mkdir(const char *pathname, mode_t mode);
extern int		io_stat(const char *pathname, struct stat *st);

/*
 *  Trivial stream orient message by single reader.
 */
struct io_message
{
	int		fd;
	unsigned char	payload[MSG_SIZE];
	ssize_t		len;
};

extern ssize_t		io_msg_write(int fd, void *payload,unsigned char count);
extern void		io_msg_new(struct io_message *ip, int fd);
extern int		io_msg_read(struct io_message *ip);

/*
 *  Blob request record, defined in brr.c
 */
extern void	brr_close();
extern void	brr_open();
extern void	brr_write(struct request *);
extern int	brr_roll();
extern int	brr_roll_final();
extern void	brr_roll_free_all();
extern int	brr_stat(struct stat *buf);
extern int	wrap(struct request *, struct digest_module *);
extern int	roll(struct request *, struct digest_module *);

/*
 *  OS dependent interface routines for process title, typical used
 *  by the unix 'ps' verb, defined ps_title.c
 */
extern int	ps_title_init(int argc, char **argv);
extern void	ps_title_set(char *word1, char *word2, char *word3);
#define 	PS_TITLE_WRITE_ARGV	1

extern void	leave(int status);

/*
 *  Manage/Reap child processes, defined in process.c
 */
extern pid_t	proc_fork();
extern int	proc_reap();
extern pid_t	proc_is_brr_inode(ino_t inode);
extern pid_t	proc_is_log_inode(ino_t inode);
extern int	proc_get_count();
extern void	proc_heartbeat();

/*
 *  Digest Module interface functions: boot/shutdown/get
 */
extern int			module_boot();
extern void			module_leave();
extern struct digest_module *	module_get(char *name);

/*
 *  Simple set of blobs with limited operations.
 */
void		blob_set_alloc(void **set);
int		blob_set_free(void *set);
int		blob_set_exists(void *set, unsigned char *value, int size);
int		blob_set_put(void *set, unsigned char *value, int size);
int		blob_set_for_each(void *set,
			int (*each_callback)(unsigned char *, int size, void *),
		 	void *context_data
		);

/*
 *  The arborist/file system garbage collector process.
 */
void		arbor_open();
void		arbor_close();
void		arbor_rename(char *tmp_path, char *new_path);
void		arbor_trim(char *blob_path);

//  suppress unsed arg error in both clang and gcc
#define UNUSED_ARG(x)	(void)(x)

#endif
