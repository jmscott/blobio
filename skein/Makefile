#
#  Synopsis:
#	Build the skein hash libraries
#  Blame:
#  	jmscott@setspace.com
#  	setspace@gmail.com
#  Note:
#  	The skein libs will be replaced by sha3 and go away.
#
include ../blobio.mk
include ../local.mk

all: libskein.a skein.h
	cd Optimized_32bit && $(MAKE) all && $(MAKE) install
clean:
	cd Optimized_32bit && $(MAKE) clean
	rm -f *.h nab.o libskein.a

libskein.a: nab.c
	cd Optimized_32bit && $(MAKE) all && $(MAKE) install
	gcc $(CFLAGS) -c nab.c
	ar rcs libskein.a nab.o

skein.h:
	$(MAKE) libskein.a
