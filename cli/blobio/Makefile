#
#  Synopsis:
#	Build blobio binary executable from C source
#  Note:
#	CC=cc -g on linux 2.6.32
#
include ../../local.mk
include ../../blobio.mk

OBJS=			\
	bc160.o		\
	bio4.o		\
	buf.o		\
	cache4.o	\
	digest.o 	\
	fs.o		\
	rofs.o		\
	service.o	\
	sha.o		\
	trace.o		\
	uni.o

all: blobio

clean:
	rm -f blobio $(OBJS) blobio.o

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		blobio							\
		$(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		bc160.c							\
		bio4.c							\
		blobio.c						\
		blobio.h						\
		buf.c							\
		cache4.c						\
		digest.c 						\
		fs.c							\
		rofs.c							\
		service.c						\
		sha.c							\
		trace.c							\
		uni.c							\
		$(DIST_ROOT)/src/blobio

blobio: blobio.o $(OBJS)
	$(CC) -o blobio blobio.o $(OBJS) $(OPENSSL_LIB)

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
rofs.o: rofs.c blobio.h
	$(CC) $(CFLAGS) -c rofs.c
fs.o: fs.c blobio.h
	$(CC) $(CFLAGS) -c fs.c
sha.o: sha.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c sha.c
bc160.o: bc160.c blobio.h
	$(CC) $(CFLAGS) $(OPENSSL_INC) -c bc160.c
service.o: service.c blobio.h
	$(CC) $(CFLAGS) -c service.c
trace.o: trace.c blobio.h
	$(CC) $(CFLAGS) -c trace.c
uni.o: uni.c blobio.h
	$(CC) $(CFLAGS) -c uni.c
