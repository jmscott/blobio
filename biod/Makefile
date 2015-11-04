#
#  Synospsi:
#	Ye old makefile
#  Blame:
#  	jmscott@setspace.com
#  	setspace@gmail.com
#  Note:
#	Change gcc reference to $(CC)
#
include ../blobio.mk
include ../local.mk

OBJS=				\
	arbor.o			\
	biod.o			\
	brr.o			\
	blob_set.o		\
	io.o			\
	log.o			\
	macosx.o		\
	module.o		\
	ps_title.o		\
	sha_fs.o		\
	sk_fs.o			\
	signal.o

#  Linux requires linking to realtime timer library

ifeq "$(UNAME)" "Linux"
	RT_LINK=-lrt
endif

all: biod append-brr

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		append-brr						\
		biod							\
		biod_ctl						\
		create-rrd						\
		fs2udig							\
		graph-rrd						\
		update-rrd						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-udig2fs						\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=	\
		arbor.c							\
		biod.c							\
		biod.h							\
		blob_set.c						\
		brr.c							\
		io.c							\
		log.c							\
		macosx.c						\
		macosx.h						\
		module.c						\
		ps_title.c						\
		sha_fs.c						\
		signal.c						\
		sk_fs.c							\
		$(DIST_ROOT)/src/biod

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		blobio-sys5.rc.template					\
		blobio.rc.template 					\
		$(DIST_ROOT)/lib

biod: $(OBJS) ../skein
	ls biod.o
	gcc -o biod $(OBJS)						\
		$(RT_LINK)						\
		../sha1/sha1.o						\
		-L../skein -lskein
../skein/libskein.a:
	cd ../skein && $(MAKE) libskein.a
../skein/skein.h:
	cd ../skein && $(MAKE) skein.h

append-brr: append-brr.c
	gcc -o append-brr $(CFLAGS) append-brr.c
	
blobio: blobio.o
	gcc -o blobio blobio.o

arbor.o: arbor.c biod.h
	gcc $(CFLAGS) -c arbor.c

blob_set.o: blob_set.c biod.h
	gcc $(CFLAGS) -c blob_set.c

sha_fs.o: sha_fs.c biod.h
	gcc $(CFLAGS) -c sha_fs.c

sk_fs.o: sk_fs.c biod.h ../skein/skein.h
	gcc $(CFLAGS) -c -I../skein sk_fs.c

io.o: io.c biod.h biod.h
	gcc $(CFLAGS) -c io.c

signal.o: signal.c biod.h biod.h
	gcc $(CFLAGS) -c signal.c

is_sha_fs.o: is_sha_fs.c biod.h
	gcc $(CFLAGS) -c is_sha_fs.c

log.o: log.c biod.h
	gcc $(CFLAGS) -c log.c

macosx.o: macosx.c biod.h
	gcc $(CFLAGS) -c macosx.c

biod.o: biod.c biod.h
	gcc $(CFLAGS) -c biod.c

brr.o: brr.c biod.h
	gcc $(CFLAGS) -c brr.c

module.o: module.c biod.h
	gcc $(CFLAGS) -c module.c

dev-links:
	test -e data || ln -s . data
	test -e htdocs || ln -s . htdocs
	test -e log || ln -s . log
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e sha_fs || mkdir sha_fs
	test -e sk_fs || mkdir sk_fs
clean-dev-links:
	rm -rf sbin log data run
clean:
	rm -f $(OBJS) biod append-brr
local.mk:
	cp local.mk.example local.mk
