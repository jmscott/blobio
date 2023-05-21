#
#  Synopsis:
#	Install client scripts and blobio client interface
#

include ../local.mk
include ../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

MKMK=blobio-cli.mkmk
COMPILEs := $(shell  (. ./$(MKMK) && echo $$COMPILEs))
BINs := $(shell  (. ./$(MKMK) && echo $$BINs))
LIBs := $(shell  (. ./$(MKMK) && echo $$LIBs))
SRCs := $(shell  (. ./$(MKMK) && echo $$SRCs))

all: $(COMPILEs)
	cd blobio && $(_MAKE) all

install-dirs:
	cd .. && $(_MAKE) install-dirs
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/cli

install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(SRCs)							\
		$(DIST_ROOT)/src/cli
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(BINs)							\
		$(DIST_ROOT)/bin

	cd blobio && $(_MAKE) install
clean:
	cd blobio && $(_MAKE) clean
	rm -fv $(COMPILEs)
distclean:
	cd blobio && $(_MAKE) distclean
	test -d $(DIST_ROOT)/bin && (cd $(DIST_ROOT)/bin && rm -f $(BINs))
	test -d $(DIST_ROOT)/lib && (cd $(DIST_ROOT)/lib && rm -f $(LIBs))
	rm -rf $(DIST_ROOT)/src/cli

dev-links:
	test -e lib || ln -s . lib

world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install

bio-frisk-brr.o: bio-frisk-brr.c
	$(CC) $(CFLAGS) -c bio-frisk-brr.c 

bio-frisk-brr: bio-frisk-brr.o
	$(CC) -o bio-frisk-brr bio-frisk-brr.o -L$(JMSCOTT_ROOT)/lib -ljmscott
