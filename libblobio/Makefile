#
#  Synopsis:
#	Ye old makefile
#  Blame:
#	jmscott@setspace.com
#	setspace@gmail.com
#
include ../blobio.mk
include ../local.mk

OBJS=module.o nab.o

all: libblobio.a
	cd sha1;  $(MAKE) all

clean:
	rm -f $(OBJS) libblobio.a
	cd sha1 && $(MAKE) clean

libblobio.a: $(OBJS)
	ar rcs libblobio.a $(OBJS)

nab.o: nab.c libblobio.h
	cc $(CFLAGS) -c nab.c

module.o: module.c libblobio.h
	gcc $(CFLAGS) -c module.c
