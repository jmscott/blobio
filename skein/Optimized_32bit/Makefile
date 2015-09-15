include ../../blobio.mk
include ../../local.mk

INCS=	brg_endian.h		\
	brg_types.h		\
	skein.h			\
	skein_port.h

OBJS=	skein_block.o		\
	skein.o

all: $(OBJS) $(INCS)

skein_block.o: skein_block.c
	gcc $(CFLAGS) -c skein_block.c
skein.o: skein.c
	cc $(CFLAGS) -c skein.c
clean:
	rm -f $(OBJS)
install: all
	ar rcs ../libskein.a skein.o skein_block.o
	cp $(INCS) ..
