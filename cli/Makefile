#
#  Synopsis:
#	Build blobio binary executable from C source
#  Blame:
#	jmscott@setspace.com
# 	setspace@gmail.com
#

include ../blobio.mk
include ../local.mk

CFLAGS+= -I../libblobio

#
#  Added extra, overriding rules to local.mk
#
ifeq ($(wildcard local.mk), local.mk)
	include local.mk
endif

OBJS=			\
	dump.o		\
	module.o 	\
	sha.o		\
	sk.o

all: blobio

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-cat							\
		bio-eat							\
		bio-file 						\
		bio-get							\
		bio-give						\
		bio-merge						\
		bio-more						\
		bio-put							\
		bio-put-file						\
		bio-roll						\
		bio-take						\
		bio-unroll						\
		bio-wrap						\
		bio-zap							\
		blobio 							\
		$(DIST_ROOT)/bin

blobio: blobio.o $(OBJS)
	gcc -o blobio blobio.o $(OBJS) ../sha1/sha1.o		\
		-L../skein -lskein
blobio.o: blobio.c blobio.h
	gcc -g $(CFLAGS) -c blobio.c
dump.o: dump.c blobio.h
	gcc $(CFLAGS) -c dump.c
module.o: module.c blobio.h
	gcc $(CFLAGS) -c module.c
sha.o: sha.c blobio.h
	gcc $(CFLAGS) -c sha.c
sk.o: sk.c blobio.h
	gcc $(CFLAGS) -I../skein -c sk.c
clean:
	rm -f blobio blobio.o $(OBJS)

skdigest: skdigest.c
	gcc $(CFLAGS) -I../skein -o skdigest skdigest.c -L../skein -lskein
