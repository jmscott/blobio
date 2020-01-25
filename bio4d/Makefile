#
#  Synospsi:
#	Ye old makefile
#
include ../local.mk
include ../blobio.mk

SRC=$(DIST_ROOT)/src
SBIN=$(DIST_ROOT)/sbin
LIB=$(DIST_ROOT)/lib

OBJS=									\
	arbor.o								\
	bc160_fs.o							\
	bio4d.o								\
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
	bio4d.c								\
	bio4d.h								\
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
	bio4d								\
	bio4d_ctl							\
	create-rrd							\
	fs2udig								\
	graph-rrd							\
	launchd-bio4d							\
	update-rrd							\

LIBs=									\
	blobio-sys5.rc.template						\
	blobio.bio4d.plist.example					\
	blobio.bio4d.service.example					\
	blobio.flowd.service.example					\
	blobio.rc.template						\

#  Linux requires linking to realtime timer library

ifeq "$(UNAME)" "Linux"
	RT_LINK=-lrt
endif

all: bio4d append-brr

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/bio4d
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-udig2fs						\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,g=rx,o=	\
		$(CSRC) $(DIST_ROOT)/src/bio4d

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,g=rx,o=	\
		$(LIBs) $(DIST_ROOT)/lib

bio4d: $(OBJS)
	$(CC) -o bio4d $(OBJS)						\
		$(RT_LINK)						\
		$(OPENSSL_LIB)

append-brr: append-brr.c
	$(CC) -o append-brr $(CFLAGS) append-brr.c

arbor.o: arbor.c bio4d.h
	$(CC) $(CFLAGS) -c arbor.c

blobio: blobio.o
	$(CC) -o blobio blobio.o

bio4d.o: bio4d.c bio4d.h
	$(CC) $(CFLAGS) -c bio4d.c

brr.o: brr.c bio4d.h
	$(CC) $(CFLAGS) -c brr.c

blob_set.o: blob_set.c bio4d.h
	$(CC) $(CFLAGS) -c blob_set.c

sha_fs.o: sha_fs.c bio4d.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha_fs.c

bc160_fs.o: bc160_fs.c bio4d.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c bc160_fs.c

io.o: io.c bio4d.h bio4d.h
	$(CC) $(CFLAGS) -c io.c

req.o: req.c bio4d.h bio4d.h
	$(CC) $(CFLAGS) -c req.c

signal.o: signal.c bio4d.h bio4d.h
	$(CC) $(CFLAGS) -c signal.c

log.o: log.c bio4d.h
	$(CC) $(CFLAGS) -c log.c

macosx.o: macosx.c bio4d.h
	$(CC) $(CFLAGS) -c macosx.c

net.o: net.c bio4d.h
	$(CC) $(CFLAGS) -c net.c

ps_title.o: ps_title.c bio4d.h
	$(CC) $(CFLAGS) -c ps_title.c

module.o: module.c bio4d.h
	$(CC) $(CFLAGS) -c module.c

tmp.o: tmp.c
	$(CC) $(CFLAGS) -c tmp.c

dev-links:
	test -e htdocs || ln -s . htdocs
	test -e log || ln -s . log
	test -e run || ln -s . run
	test -e bin || ln -s . bin
	test -e sbin || ln -s . sbin
	test -e lib || ln -s . lib
	test -e tmp || ln -s . tmp
	test -e data || mkdir data
	test -e data/sha_fs || mkdir -p data/sha_fs
	test -e data/bc160_fs || mkdir -p data/bc160_fs
	test -e spool || mkdir spool
	test -e spool/wrap || mkdir spool/wrap

clean-dev-links:
	rm -rf htdocs log run bin sbin lib tmp data sha_fs bc160_fs spool

clean:
	rm -f $(OBJS) bio4d append-brr

distclean:
	test ! -e $(SRC) || (cd $(SRC);  rm -f $(CSRC))
	test ! -e $(SBIN) || (cd $(SBIN);  rm -f $(SBINs))
	test ! -e $(LIB) || (cd $(LIB);  rm -f $(LIBs))
world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
