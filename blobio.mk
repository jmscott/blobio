#
#  Synopsis:
#	Static make rules for blobio.
#  Note:
#	Need to eliminate vars BLOBIO_{GROUP,USER,PREFIX} and replace with
#	DIST_{GROUP,USER,PREFIX}.  github.com/setspace has same problem.
#
ifndef DIST_ROOT
$(error var DIST_ROOT is not set)
endif

UNAME=$(shell uname)

#  Most extreme compilation flags

CFLAGS?=-Wall -Wextra -Werror -DFS_SHA_MODULE -DFS_BC160_MODULE -DFS_BTC20_MODULE
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

BLOBIO_PREFIX=$(DIST_ROOT)
BLOBIO_GROUP=$(DIST_GROUP)
BLOBIO_USER=$(DIST_USER)

PG_CONFIG?=/usr/local/pgsql/bin/pg_config
PGLIB=$(shell $(PG_CONFIG) --libdir)
PGINC=$(shell $(PG_CONFIG) --includedir)

JMSCOTT_INC=$(JMSCOTT_ROOT)/include
