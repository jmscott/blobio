/*
 * Synopsis:
 *   	Trivial stress test of fork/exec.
 *  Blame:
 *	jmscott@setspace.com
 *	setspace@gmail.com
 */
#include <sys/wait.h>

#include <unistd.h>
#include <stdio.h>

int
main(int argc, char **argv)
{
	int i = 0;
	pid_t pid;
	int status;
	char *path;

	if (argc != 2) {
		static char badargc[] = "ERROR: wrong number of arguments\n";

		write(2, badargc, sizeof badargc - 1);
		_exit(1);
	}

	path = argv[1];
	while (i++ < 100000) {

		pid = fork();
		if (pid < 0) {
			perror("fork() failed");
			_exit(1);
		}

		if (pid == 0) {
			//  extra NULL eilinates wierd 
			execl(path, path, NULL);
			perror(path);
			_exit(1);
		}

		if (wait(&status) < 0) {
			perror("wait() failed");
			_exit(1);
		}
		if (WIFEXITED(status)) {
			if (status != 0) {
				static char notzero[] =
					"ERROR: child exit status not 0\n";

				write(2, notzero, sizeof notzero - 1);
				_exit(1);
			}
		} else {
			static char badkid[] =
				"ERROR: child process did not exit normally\n";
			write(2, badkid, sizeof badkid - 1);
			_exit(1);
		}
	}
	return 0;
}
