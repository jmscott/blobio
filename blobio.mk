#
#  Synopsis:
#	Static make rules for blobio.
#  Blame:
#  	jmscott@setspace.com
#  	setspace@gmail.com
#

#  Most extreme compilation flags

CFLAGS=-Wall -Wextra -Werror -DSHA_FS_MODULE

#  Setting SHELL forces gmake to disable optimizing of single line productions,
#  forcing make to honor $PATH

SHELL=/bin/bash

UNAME=$(shell uname)
