/*
 *  Synopsis:
 *	Global C header file for bio4d program.
 */
#ifndef BIO4D_H
#define BIO4D_H

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

	char	*step;		//  request, bytes, reply

	/*
	 *  Number of bytes scanned ahead during the intial read of request.
	 */
	unsigned char	*scan_buf;
	int		scan_size;

	struct timespec	start_time;		/* fork() start time */
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
	char			transport[129];
	char			transport_tiny[129];

	/*
	 *  Track the ok/no chat session between client and bio4d.
	 */
	char			chat_history[10];

	u8	blob_size;

	u4	read_timeout;	/* # seconds before a request read timeout */
	u4	write_timeout;	/* # seconds before a request write timeout */

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
	int	(*get_request)(struct request *);
	int	(*get_bytes)(struct request *);

	/*
	 *  Client takes a blob from a server.
	 *  Server may forget the blob.
	 */
	int	(*take_request)(struct request *);
	int	(*take_bytes)(struct request *);
	int	(*take_reply)(struct request *, char *reply);

	/*
	 *  Put a blob on this server.
	 */
	int	(*put_request)(struct request *);
	int	(*put_bytes)(struct request *);

	/*
	 *  Give a blob to this server.
	 *  Client may forget the blob.
	 */
	int	(*give_request)(struct request *);
	int	(*give_bytes)(struct request *);
	int	(*give_reply)(struct request *, char *reply);

	/*
	 *  Eat/verify an existing blob.
	 */
	int	(*eat)(struct request *);

	/*
	 *  Digest a local stream up to EOF and copy text digest as c string to
	 *  memory referenced by char *digest.
	 */
	int     (*digest)(struct request *, int fd, char *digest);

	/*
	 *  Write a local blob to a local stream and return 0 if blob exists
	 *  and was copyed to the local stream, otherwise -1.
	 */
	int	(*copy)(struct request *, int fd);

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

void	info(char *msg);
void	info2(char *msg1, char *msg2);
void	info3(char *msg1, char *msg2, char *msg3);
void	info4(char *msg1, char *msg2, char *msg3, char *msg4);

void	warn(char *msg);
void	warn2(char *msg1, char *msg2);
void	warn3(char *msg1, char *msg2, char *msg3);
void	warn4(char *msg1, char *msg2, char *msg3, char *msg4);

void	error(char *msg);
void	error2(char *msg1, char *msg2);
void	error3(char *msg1, char *msg2, char *msg3);
void	error4(char *msg1, char *msg2, char *msg3, char *msg4);
void	error5(char *msg1, char *msg2,char *msg3,char *msg4,char *msg5);

void	panic(char *msg);
void	panic2(char *msg1, char *msg2);
void	panic3(char *msg1, char *msg2, char *msg3);
void	panic4(char *msg1, char *msg2, char *msg3, char *msg4);

/*
 *  Event logging to log/bio4d.log, defined in log.c
 *  Most important are the log_str* functions, which known about limits
 *  on buffers imposed by #define MSG_SIZE.
 */
void	log_close();
void	log_open();
int	log_roll();
int	log_roll_final();
int	log_stat(struct stat *);
void	log_write(char *buf);
void	log_roll_free_all();
char	*log_strcpy2(char *buf, int buf_size, char *msg1, char *msg2);
char	*log_strcpy3(
		char *buf,
		int buf_size,
		char *msg1,
		char *msg2,
		char *msg3
	);
char	*log_strcpy4(
		char *buf,
		int buf_size,
		char *msg1,
		char *msg2,
		char *msg3,
		char *msg4
	);
char	*log_strcat2(char *buf, int buf_size, char *msg1, char *msg2);
char	*log_strcat3(
		char *buf,
		int buf_size,
		char *msg1,
		char *msg2,
		char *msg3
	);
void	log_format(char *msg, char *buf, int buf_size);


int	burp_text_file(char *buf, char *path);
int	slurp_text_file(char *path, char *buf, size_t buf_size);
int	write_ok(struct request *rp);
int	write_no(struct request *rp);
int	read_buf(
		int fd,
		unsigned char *buf,
		int buf_size,
		 unsigned timeout
	);
int	write_buf(int fd, unsigned char *buf, int buf_size, unsigned timeout);
char	*read_reply(struct request *);
char	*net_32addr2text(u_long addr);

char	*sig_name(int sig);

int	trust_fs;

/*
 *  Wrappers around various interuptable unix system calls
 */
int		io_path_exists(char *path);
int		io_close(int fd);
ssize_t		io_read(int fd, void *buf, size_t count);
ssize_t		io_write(int fd, void *buf, size_t count);
int		io_write_buf(int fd, void *buf, size_t count);
int		io_pipe(int fds[2]);
int		io_open(char *path, int flags, int mode);
int		io_open_append(char *path);
int		io_open_trunc(char *path);
int		io_select(
			int nfds,
			fd_set *readfds,
			fd_set *writefds,
			fd_set *errorfds,
			struct timeval *timeout
		);
pid_t		io_waitpid(pid_t pid, int *stat_loc, int options);
int		io_rename(char *old_path, char *new_path);
DIR		*io_opendir(char *path);
struct dirent	*io_readdir(DIR *dirp);
int		io_unlink(const char *path);
int		io_rmdir(const char *path);
int		io_mkfifo(char *path, mode_t mode);
int		io_chmod(char *path, mode_t mode);
off_t		io_lseek(int fd, off_t offset, int whence);
int		io_fstat(int fd, struct stat *buf);
int		io_closedir(DIR *dirp);
int		io_mkdir(const char *pathname, mode_t mode);
int		io_stat(const char *pathname, struct stat *st);
int		io_lstat(const char *pathname, struct stat *st);
int		io_utimes(const char *pathname, const struct timeval times[2]);

int		net_accept(
			int listen_fd, struct sockaddr *addr,
			socklen_t *len,
			int *client_fd,
			unsigned timeout
		);
ssize_t		net_read(
			int fd,
			void *buf,
			size_t count,
			unsigned timeout
		);
int		net_write(
			int fd,
			void *buf,
			size_t count,
			unsigned timeout
		);

ssize_t		req_read(
			struct request *r,
			void *buf,
			size_t buf_size
		);
int		req_write(
			struct request *r,
			void *buf,
			size_t buf_size
		);

ssize_t		blob_read(
			struct request *r,
			void *buf,
			size_t buf_size
		);

int		blob_write(
			struct request *r,
			void *buf,
			size_t buf_size
		);

void		decode_hex(char *hex, unsigned char *bytes);

/*
 *  Trivial stream orient message by single reader.
 */
struct io_message
{
	int		fd;
	unsigned char	payload[MSG_SIZE];
	ssize_t		len;
};

ssize_t		io_msg_write(int fd, void *payload,unsigned char count);
void		io_msg_new(struct io_message *ip, int fd);
int		io_msg_read(struct io_message *ip);

/*
 *  Blob request record, defined in brr.c
 */
void	brr_close();
void	brr_open();
void	brr_send(struct request *);
int	brr_roll();
int	brr_roll_final();
void	brr_roll_free_all();
int	brr_stat(struct stat *buf);
int	wrap(struct request *, struct digest_module *);
int	roll(struct request *, struct digest_module *);

/*
 *  OS dependent interface routines for process title, typical used
 *  by the unix 'ps' verb, defined ps_title.c
 */
int	ps_title_init(int argc, char **argv);
void	ps_title_set(char *word1, char *word2, char *word3);
#define 	PS_TITLE_WRITE_ARGV	1

void	leave(int status);

/*
 *  Manage/Reap child processes, defined in process.c
 */
pid_t	proc_fork();
int	proc_reap();
pid_t	proc_is_brr_inode(ino_t inode);
pid_t	proc_is_log_inode(ino_t inode);
int	proc_get_count();
void	proc_heartbeat();

/*
 *  Digest Module interface functions: boot/shutdown/get
 */
int			module_boot();
void			module_leave();
struct digest_module *	module_get(char *name);

/*
 *  Trivial set stored on heap of blobs with get/put/for-each operations.
 */
void		blob_set_alloc(void **set);
int		blob_set_free(void *set);
int		blob_set_exists(void *set, unsigned char *value, int size);
int		blob_set_put(void *set, unsigned char *value, int size);
int		blob_set_for_each(
			void *set,
			int (*each_callback)(unsigned char *, int size, void *),
		 	void *context_data
		);

/*
 *  The arborist/file process that moves files and garbage collects
 *  empty directories.
 */
void		arbor_open();
void		arbor_close();
void		arbor_rename(char *tmp_path, char *new_path);
void		arbor_move(char *tmp_path, char *new_path);
void		arbor_trim(char *blob_path);

/*
 *  Map digest prefix ont temp directory on same file system as blob storage.
 */
void		tmp_open();
char *		tmp_get(char *algorithm, char *digest_prefix);

#endif
