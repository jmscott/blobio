#
#  Synopsis:
#	Ye'old Makefile for sync code used by various daemons.
#
include ../local.mk
include ../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

MKMK=blobio-sync.mkmk
BINs := $(shell  (. ./$(MKMK) && echo $$BINs))
SBINs := $(shell  (. ./$(MKMK) && echo $$SBINs))
LIBs := $(shell  (. ./$(MKMK) && echo $$LIBs))
SRCs := $(shell  (. ./$(MKMK) && echo $$SRCs))
COMPILEs := $(shell  (. ./$(MKMK) && echo $$COMPILEs))

all: jaccard jaccard-tip

jaccard-tip: jaccard-tip.go
	$(GODIST)/bin/go build jaccard-tip.go
jaccard: jaccard.go
	$(GODIST)/bin/go build jaccard.go

install-dirs:
	cd .. && $(_MAKE) install-dirs
install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sync
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/sync
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(SRCs)							\
		$(DIST_ROOT)/src/sync

distclean:
	rm -rf $(DIST_ROOT)/src/sync
	test -d $(DIST_ROOT)/sbin && cd $(DIST_ROOT)/sbin &&rm -f $(SBINs)
	test -d $(DIST_ROOT)/lib && cd $(DIST_ROOT)/sbin &&rm -f $(LIBs)
clean:
	rm -f $(COMPILEs)

dev-links:
	test -e etc || ln -s . etc
	test -e lib || ln -s . lib
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
