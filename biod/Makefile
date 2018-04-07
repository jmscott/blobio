#
#  Synospsi:
#	Ye old makefile
#
include ../blobio.mk
include ../local.mk

SRC=$(DIST_ROOT)/src
SBIN=$(DIST_ROOT)/sbin
LIB=$(DIST_ROOT)/lib

OBJS=									\
	arbor.o								\
	bc160_fs.o							\
	biod.o								\
	blob_set.o							\
	brr.o								\
	io.o								\
	log.o								\
	macosx.o							\
	module.o							\
	net.o								\
	ps_title.o							\
	req.o								\
	sha_fs.o							\
	signal.o							\
	tmp.o
CSRC=									\
	arbor.c								\
	biod.c								\
	biod.h								\
	blob_set.c							\
	brr.c								\
	io.c								\
	log.c								\
	macosx.c							\
	macosx.h							\
	module.c							\
	net.c								\
	ps_title.c							\
	req.c								\
	sha_fs.c							\
	signal.c							\
	tmp.c
SBINs=									\
	append-brr							\
	bio-udig2fs							\
	biod								\
	biod_ctl							\
	create-rrd							\
	fs2udig								\
	graph-rrd							\
	sync-put							\
	update-rrd
LIBs=									\
	blobio-sys5.rc.template						\
	blobio.rc.template

#  Linux requires linking to realtime timer library

ifeq "$(UNAME)" "Linux"
	RT_LINK=-lrt
endif

all: biod append-brr

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/biod
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-udig2fs						\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(CSRC) $(DIST_ROOT)/src

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(CSRC) $(DIST_ROOT)/src/biod

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs) $(DIST_ROOT)/lib

biod: $(OBJS)
	$(CC) -o biod $(OBJS)						\
		$(RT_LINK)						\
		$(OPENSSL_LIB)

append-brr: append-brr.c
	$(CC) -o append-brr $(CFLAGS) append-brr.c

arbor.o: arbor.c biod.h
	$(CC) $(CFLAGS) -c arbor.c

blobio: blobio.o
	$(CC) -o blobio blobio.o

biod.o: biod.c biod.h
	$(CC) $(CFLAGS) -c biod.c

brr.o: brr.c biod.h
	$(CC) $(CFLAGS) -c brr.c

blob_set.o: blob_set.c biod.h
	$(CC) $(CFLAGS) -c blob_set.c

sha_fs.o: sha_fs.c biod.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha_fs.c

bc160_fs.o: bc160_fs.c biod.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c bc160_fs.c

io.o: io.c biod.h biod.h
	$(CC) $(CFLAGS) -c io.c

req.o: req.c biod.h biod.h
	$(CC) $(CFLAGS) -c req.c

signal.o: signal.c biod.h biod.h
	$(CC) $(CFLAGS) -c signal.c

log.o: log.c biod.h
	$(CC) $(CFLAGS) -c log.c

macosx.o: macosx.c biod.h
	$(CC) $(CFLAGS) -c macosx.c

net.o: net.c biod.h
	$(CC) $(CFLAGS) -c net.c

ps_title.o: ps_title.c biod.h
	$(CC) $(CFLAGS) -c ps_title.c

module.o: module.c biod.h
	$(CC) $(CFLAGS) -c module.c

tmp.o: tmp.c
	$(CC) $(CFLAGS) -c tmp.c

dev-links:
	test -e htdocs || ln -s . htdocs
	test -e log || ln -s . log
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e tmp || ln -s . tmp
	test -e data || mkdir data
	test -e sha_fs || mkdir sha_fs
	test -e bc160_fs || mkdir bc160_fs
	test -e spool || mkdir spool
	test -e spool/wrap || mkdir spool/wrap

clean-dev-links:
	rm -rf sbin log data run

clean:
	rm -f $(OBJS) biod append-brr

distclean:
	test ! -e $(SRC) || (cd $(SRC);  rm -f $(CSRC))
	test ! -e $(SBIN) || (cd $(SBIN);  rm -f $(SBINs))
	test ! -e $(LIB) || (cd $(LIB);  rm -f $(LIBs))
world:
	$(MAKE) $(MAKEFLAGS) clean
	$(MAKE) $(MAKEFLAGS) all
	$(MAKE) $(MAKEFLAGS) distclean
	$(MAKE) $(MAKEFLAGS) install
