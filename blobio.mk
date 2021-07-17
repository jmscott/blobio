#
#  Synopsis:
#	Static make rules for blobio.
#
ifndef DIST_ROOT
$(error var DIST_ROOT is not set)
endif

UNAME=$(shell uname)

#  Most extreme compilation flags

CFLAGS?=-Wall -Wextra -Werror -DFS_SHA_MODULE -DFS_BC160_MODULE
export GO111MODULE?=off

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
