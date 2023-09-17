#
#  Synospsi:
#	Ye old makefile
#  See:
#	https://github.com/jmscott/work/tree/master/make-make
#
include ../local.mk
include ../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

MKMK=blobio-bio4d.mkmk
COMPILEs := $(shell  (. ./$(MKMK) && echo $$COMPILEs))
BINs := $(shell  (. ./$(MKMK) && echo $$BINs))
SBINs := $(shell  (. ./$(MKMK) && echo $$SBINs))
LIBs := $(shell  (. ./$(MKMK) && echo $$LIBs))
SRCs := $(shell  (. ./$(MKMK) && echo $$SRCs))
OBJs := $(shell  (. ./$(MKMK) && echo $$OBJs))

SRC=$(DIST_ROOT)/src
SBIN=$(DIST_ROOT)/sbin
LIB=$(DIST_ROOT)/lib

#  Linux requires linking to realtime timer library

ifeq "$(UNAME)" "Linux"
	RT_LINK=-lrt
endif

all: bio4d append-brr

install-dirs:
	cd .. && $(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/bio4d
install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,g=rx,o=	\
		$(SRCs)							\
		$(DIST_ROOT)/src/bio4d

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,g=rx,o=	\
		$(LIBs)							\
		$(DIST_ROOT)/lib

bio4d: $(OBJs)
	$(CC) $(CFLAGS) -o bio4d $(OBJs)				\
		$(RT_LINK)						\
		$(OPENSSL_LIB)

append-brr: append-brr.c
	$(CC) $(CFLAGS) -o append-brr append-brr.c			\
		-L$(JMSCOTT_ROOT)/lib -ljmscott

arbor.o: arbor.c bio4d.h
	$(CC) $(CFLAGS) -c arbor.c

#  Note: is blobio.o stale from when cli was built here?
blobio: blobio.o
	$(CC) -o blobio blobio.o

bio4d.o: bio4d.c bio4d.h
	$(CC) $(CFLAGS) -c bio4d.c

brr.o: brr.c bio4d.h
	$(CC) $(CFLAGS) -c brr.c

blob_set.o: blob_set.c bio4d.h
	$(CC) $(CFLAGS) -c blob_set.c

fs_sha.o: fs_sha.c bio4d.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c fs_sha.c

fs_bc160.o: fs_bc160.c bio4d.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c fs_bc160.c

fs_btc20.o: fs_btc20.c bio4d.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c fs_btc20.c

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
	test -e bin || ln -s . bin
	test -e data || mkdir data
	test -e data/fs_bc160 || mkdir -p data/fs_bc160
	test -e data/fs_btc20 || mkdir -p data/fs_btc20
	test -e data/fs_sha || mkdir -p data/fs_sha
	test -e htdocs || ln -s . htdocs
	test -e lib || ln -s . lib
	test -e log || ln -s . log
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e spool || mkdir spool
	test -e spool/wrap || mkdir spool/wrap
	test -e tmp || ln -s . tmp

clean-dev-links:
	rm -rf htdocs log run bin sbin lib tmp data spool		\
		fs_sha fs_btc20 fs_bc160
clean:
	rm -f $(COMPILEs)

distclean:
	test ! -e $(SRC) || (cd $(SRC);  rm -f $(SRCs))
	test ! -e $(SBIN) || (cd $(SBIN);  rm -f $(SBINs))
	test ! -e $(LIB) || (cd $(LIB);  rm -f $(LIBs))
world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
