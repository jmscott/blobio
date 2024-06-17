/*
 *  Synopopis:
 *	Set a process title for commands like ps and top under unix.
 *  Note:
 *	Unfortunately, ps output looks odd due to the trailing white space.
 *	Need to investigate using null instead of ' ' character.
 */
#include "bio4d.h"

#define MAX_ARGC	256

static int	_argc;
static char	**_argv;
static char	*argv0;
static int	*argv_size;

/*
 *  Synopsis:
 *	Record global argc/argv and lengths of argv[] strings.
 *  Returns:
 *	0	success
 *	!=0	error
 */
int
ps_title_init(int argc, char **argv)
{
	int i;

#ifndef PS_TITLE_WRITE_ARGV
	return 0;
#endif
	_argc = argc;
	if (argc >= MAX_ARGC)
		argc = MAX_ARGC;
	_argv = argv;
	if (argc == 0)
		return 0;
	argv_size = (int *)malloc(sizeof *argv_size * argc);
	if (!argv_size) {
		panic("ps_title_init: malloc() failed");
		return -1;
	}
	/*
	 *  Record alloced size of argv[] strings.
	 */
	for (i = 0;  i < argc;  i++)
		argv_size[i] = strlen(argv[i]);
	/*
	 *  Record argv0.
	 */
	argv0 = strdup(argv[0]);
	if (argv0 == NULL) {
		panic("ps_title_init: strdup(argv[0]) failed");
		return -1;
	}
	return 0;
}

static int
add_word(int size[], char *word, int add_space)
{
	int i, len;

	len = strlen(word) + 1;
	/*
	 *  Search for space in argv[].
	 */
	for (i = 0;  i < _argc;  i++) {
		int need = len;
		if (add_space)
			need++;
		if (need > size[i])
			continue;
		/*
		 *  Add the word to argv[i].
		 */
		if (add_space)
			strcat(_argv[i], " ");
		strcat(_argv[i], word);
		size[i] -= need;
		return 1;
	}
	return 0;
}

/*
 *  Synopsis:
 *	Set what the ps unix command shows by modifiying argv[].
 *  Note:
 *	Typically the bio4d process is started from init.d with an extra
 *	long first argument.  For example,
 *
 *		/opt/blobio/bin/bio4d --ps-title-123456789012345678901234567890
 */
void
ps_title_set(char *word1, char *word2, char *word3)
{
	int i = 0;
	int size[MAX_ARGC];

#ifndef PS_TITLE_WRITE_ARGV
	return;
#endif
	if (_argc == 0 || word1 == NULL)
		return;
	/*
	 *  Clear out argv and size of original argv.
	 */
	for (i = 0;  i < _argc;  i++) {
		memset(_argv[i], 0, argv_size[i]);
		size[i] = argv_size[i];
	}
	if (add_word(size, word1, 0))
		if (word2 && add_word(size, word2, 1))
			if (word3)
				add_word(size, word3, 1);
	/*
	 *  If not enough room for anything, then just
	 *  copy stashed copy of argv0 back to argv[0].
	 *  Probably ought to restore all of argv[].
	 */
	if (!_argv[0][0])
		strcpy(_argv[0], argv0);
}
