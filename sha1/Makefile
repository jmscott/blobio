include ../blobio.mk
include ../local.mk

all: sha1.o

clean:
	rm -f sha1.o
sha1.o: sha1.c
	gcc $(CFLAGS) -c sha1.c
