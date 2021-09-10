#
#  Synopsis:
#	Build blobio binary executable from C source
#  Note:
#	Move variables OBJs and SRCs to dist file.
#
include ../../local.mk
include ../../blobio.mk

OBJs=									\
	bc160.o								\
	bio4.o								\
	buf.o								\
	cache4.o							\
	digest.o 							\
	fs.o								\
	service.o							\
	sha.o								\
	sha512t.o							\
	trace.o								\
	uni.o
SRCs=									\
	bc160.c								\
	bio4.c								\
	blobio.c							\
	blobio.h							\
	buf.c								\
	cache4.c							\
	digest.c 							\
	fs.c								\
	service.c							\
	sha.c								\
	sha512t.c							\
	trace.c								\
	uni.c								\

all: blobio

clean:
	rm -f blobio $(OBJs) blobio.o

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/blobio
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		blobio							\
		$(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SRCs)							\
		$(DIST_ROOT)/src/blobio

blobio: blobio.o $(OBJs)
	$(CC) -o blobio blobio.o $(OBJs) $(OPENSSL_LIB)

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
sha.o: sha.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha.c
sha512t.o: sha512t.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha512t.c
bc160.o: bc160.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c bc160.c
service.o: service.c blobio.h
	$(CC) $(CFLAGS) -c service.c
trace.o: trace.c blobio.h
	$(CC) $(CFLAGS) -c trace.c
uni.o: uni.c blobio.h
	$(CC) $(CFLAGS) -c uni.c

distclean:
	rm -rf $(DIST_ROOT)/src/blobio $(DIST_ROOT)/bin/blobio
world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
