#
#  Synopsis:
#	Build blobio command line binary executable from C source
#  Note:
#	Move variables OBJs and SRCs to dist file.
#
include ../../local.mk
include ../../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

DIST=blobio-cli-blobio.dist
BINs := $(shell  (. ./$(DIST) && echo $$BINs))
LIBs := $(shell  (. ./$(DIST) && echo $$LIBs))
SRCs := $(shell  (. ./$(DIST) && echo $$SRCs))
OBJs := $(shell  (. ./$(DIST) && echo $$OBJs))
COMPILEs := $(shell  (. ./$(DIST) && echo $$COMPILEs))

all: blobio

clean:
	rm -f $(COMPILEs)

install-dirs:
	cd .. && $(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/cli/blobio

install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(BINs)							\
		$(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SRCs)							\
		$(DIST_ROOT)/src/cli/blobio

blobio: $(OBJs)
	$(CC) -o blobio $(OBJs) $(OPENSSL_LIB) -L$(JMSCOTT_ROOT)/lib -ljmscott

bio4.o: bio4.c blobio.h
	$(CC) $(CFLAGS) -c bio4.c
cache4.o: cache4.c blobio.h
	$(CC) $(CFLAGS) -c cache4.c
buf.o: buf.c
	$(CC) $(CFLAGS) -c buf.c
blobio.o: blobio.c blobio.h
	$(CC) -g $(CFLAGS) -c blobio.c
digest.o: digest.c blobio.h
	$(CC) $(CFLAGS) -c digest.c
fs.o: fs.c blobio.h
	$(CC) $(CFLAGS) -c fs.c
null.o: null.c blobio.h
	$(CC) $(CFLAGS) -c null.c
sha.o: sha.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha.c
bc160.o: bc160.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c bc160.c
btc20.o: btc20.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c btc20.c
service.o: service.c blobio.h
	$(CC) $(CFLAGS) -c service.c
trace.o: trace.c blobio.h
	$(CC) $(CFLAGS) -c trace.c
uni.o: uni.c blobio.h
	$(CC) $(CFLAGS) -c uni.c

distclean:
	test -d $(DIST_ROOT)/bin && cd $(DIST_ROOT)/bin && rm -f $(BINs)
	rm -rf $(DIST_ROOT)/src/cli/blobio
world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
dist:
	make-dist tar $(DIST)
