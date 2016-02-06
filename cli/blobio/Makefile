#
#  Synopsis:
#	Build blobio binary executable from C source
#  Blame:
#	jmscott@setspace.com
# 	setspace@gmail.com
#  Note:
#	CC=cc -g on linux 2.6.32
#
include ../../blobio.mk
include ../../local.mk

OBJS=			\
	digest.o 	\
	service.o	\
	sha.o		\
	trace.o		\
	unistd.o	\
	bio4.o

all: blobio

clean:
	rm -f blobio $(OBJS) blobio.o

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		blobio							\
		$(DIST_ROOT)/bin

blobio: blobio.o $(OBJS) ../../sha1/sha1.c
	$(CC) -o blobio blobio.o $(OBJS) ../../sha1/sha1.o

bio4.o: bio4.c blobio.h
	$(CC) $(CFLAGS) -c bio4.c
blobio.o: blobio.c blobio.h
	$(CC) -g $(CFLAGS) -c blobio.c
digest.o: digest.c blobio.h
	$(CC) $(CFLAGS) -c digest.c
sha.o: sha.c blobio.h
	$(CC) $(CFLAGS) -c sha.c
service.o: service.c blobio.h
	$(CC) $(CFLAGS) -c service.c
trace.o: trace.c blobio.h
	$(CC) $(CFLAGS) -c trace.c
unistd.o: unistd.c blobio.h
	$(CC) $(CFLAGS) -c unistd.c
