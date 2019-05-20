#
#  Synopsis:
#	Static make rules for blobio.
#

#  Most extreme compilation flags

CFLAGS?=-Wall -Wextra -Werror -DSHA_FS_MODULE -DBC160_FS_MODULE

#  Setting SHELL forces gmake to disable optimizing of single line productions,
#  forcing make to honor $PATH

SHELL?=/bin/bash

UNAME=$(shell uname)

DIST_USER?=$(USER)
ifeq "$(UNAME)" "Darwin"
	DIST_GROUP?=staff
else
	DIST_GROUP?=$(USER)
endif
