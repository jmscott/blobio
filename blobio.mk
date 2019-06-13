#
#  Synopsis:
#	Static make rules for blobio.
#
UNAME=$(shell uname)

#  Most extreme compilation flags

CFLAGS?=-Wall -Wextra -Werror -DSHA_FS_MODULE -DBC160_FS_MODULE

ifeq "$(UNAME)" "Linux"
	CFLAGS+=-std=gnu99
endif

#  Setting SHELL forces gmake to disable optimizing of single line productions,
#  forcing make to honor $PATH

SHELL?=/bin/bash

DIST_USER?=$(USER)
ifeq "$(UNAME)" "Darwin"
	DIST_GROUP?=staff
else
	DIST_GROUP?=$(USER)
endif
