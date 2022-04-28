#
#  Synopsis:
#  	Ye old makefile
#
include ../local.mk
include ../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

DIST=blobio-stat.dist

COMPILEs := $(shell  (. ./$(DIST) && echo $$COMPILEs))
SRCs := $(shell  (. ./$(DIST) && echo $$SRCs))
LIBs := $(shell  (. ./$(DIST) && echo $$LIBs))
SBINs := $(shell  (. ./$(DIST) && echo $$SBINs))

all: $(COMPILEs)

install-dirs:
	cd .. && $(_MAKE) install-dirs

install: $(SBINs) $(LIBs)
	cd .. && $(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(SRCs)							\
		$(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

roll2stat_json: roll2stat_json.go
	$(GODIST)/bin/go build roll2stat_json.go

merge-stdin-roll2stat_json: merge-stdin-roll2stat_json.pgc
	ecpg merge-stdin-roll2stat_json.pgc
	$(CC) $(CFLAGS) 						\
		-I$(JMSCOTT_INC)					\
		-I$(PGINC) 						\
		merge-stdin-roll2stat_json.c				\
		-o merge-stdin-roll2stat_json -L$(PGLIB) -lecpg
	rm merge-stdin-roll2stat_json.c

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
	test ! -d $(DIST_ROOT)/src || (cd $(DIST_ROOT)/sbin && rm -f $(SRCs))
clean:
	rm -f $(COMPILEs) merge-stdin-roll2stat_json.c
world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install

dist:
	make-dist tar $(DIST)
