#
#  Synopsis:
#  	Ye old makefile
#
include ../../local.mk
include ../../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

MKMK=blobio-sync-pgsql.mkmk

COMPILEs := $(shell  (. ./$(MKMK) && echo $$COMPILEs))
SBINs := $(shell  (. ./$(MKMK) && echo $$SBINs))
SRCs := $(shell  (. ./$(MKMK) && echo $$SRCs))
LIBs := $(shell  (. ./$(MKMK) && echo $$LIBs))

all: $(COMPILEs)
clean:
	rm -f $(COMPILEs)

jaccard-tip: jaccard-tip.go
	$(GODIST)/bin/go build jaccard-tip.go
jaccard: jaccard.go
	$(GODIST)/bin/go build jaccard.go

install-dirs:
	cd .. && $(_MAKE) install-dirs

install: $(SBINs) $(LIBs)
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

dev-links:
	test -e etc || ln -s . etc
	test -e lib || ln -s . lib
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e log || ln -s . log
	test -e spool || ln -s . spool

distclean:
	test ! -d $(DIST_ROOT)/lib || (cd $(DIST_ROOT)/lib;  rm -f $(LIBs))
	test ! -d $(DIST_ROOT)/sbin || (cd $(DIST_ROOT)/sbin;  rm -f $(SBINs))
	test ! -d $(DIST_ROOT)/src || (cd $(DIST_ROOT)/src && rm -f $(SRCs))
world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
tar:
	make-make tar $(MKMK)
