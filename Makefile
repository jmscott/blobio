#
#  Synopsis:
#	Root makefile for blobio clients and servers.
#  Depends:
#	local.mk, derived from local-<os>.mk.example
#  Usage:
#	GITHUB=$HOME/src/github.com/jmscott
#	mkdir -p $GITHUB
#	cd $GITHUB
#	svn https://github.com/jmscott/blobio
#	cd blobio/trunk
#	cp local-darwin.mk.example local.mk
#	nano local.mk
#	make clean all distclean install
#  Note:
#	How safe is an group executable $BLOBIO_ROOT/etc/ ?
#	Currently $BLOBIO_ROOT/etc/profile is read by group members.
#
include local.mk
include blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

DIST=blobio.dist
SBINs := $(shell  (. ./$(DIST) && echo $$SBINs))
BINs := $(shell  (. ./$(DIST) && echo $$BINs))
LIBs := $(shell  (. ./$(DIST) && echo $$LIBs))

all:
	cd bio4d &&	$(_MAKE) all
	cd cli &&	$(_MAKE) all
	cd flowd &&	$(_MAKE) all
	cd sync &&	$(_MAKE) all
ifdef DASH_DNS_VHOST_SUFFIX
	cd www &&	$(_MAKE) all
endif

clean:
ifdef DASH_DNS_VHOST_SUFFIX
	cd www &&	$(_MAKE) clean
endif
	cd bio4d &&	$(_MAKE) clean
	cd cli &&	$(_MAKE) clean
	cd flowd &&	$(_MAKE) clean
	cd sync &&	$(_MAKE) clean

distclean:
ifdef DASH_DNS_VHOST_SUFFIX
	cd www &&	$(_MAKE) distclean
endif
	cd bio4d &&	$(_MAKE) distclean

ifeq "$(DIST_ROOT)" "/usr"
	@echo 'refuse to distclean /usr'
	exit 1
endif

ifeq "$(DIST_ROOT)" "/usr/local"
	@echo 'refuse to distclean /usr/local'
	exit 1
endif

	cd flowd &&	$(_MAKE) distclean
	rm -rf $(DIST_ROOT)/bin
	rm -rf $(DIST_ROOT)/lib
	rm -rf $(DIST_ROOT)/sbin
	rm -rf $(DIST_ROOT)/src

install-dirs:
	#  setup the directories
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/pgsql
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/bio4d
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/blobio
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/attic
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/etc
	#  need group read for possible other services
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/spool
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/spool/wrap
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/log
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/run
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/data
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/data/fs_sha
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,go= -d $(DIST_ROOT)/data/fs_sha/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/data/fs_bc160
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,go= -d $(DIST_ROOT)/data/fs_bc160/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/data/fs_btc20
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-m u=rwx,go= -d $(DIST_ROOT)/data/fs_btc20/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/sync
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib
install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(BINs)							\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	cd bio4d;	$(MAKE) $(MFLAGS) install
	cd cli;		$(MAKE) $(MFLAGS) install
	cd pgsql;	$(MAKE) $(MFLAGS) install
	cd flowd;	$(MAKE) $(MFLAGS) install
	#cd www;		$(MAKE) $(MFLAGS) install
	cd sync;	$(MAKE) $(MFLAGS) install
ifdef NOC_DNS_VHOST_SUFFIX
	cd www && $(MAKE) $(MFLAGS) install
endif

dev-links:
	test -e cgi-bin || ln -s . cgi-bin
	test -e htdocs || ln -s . htdocs
	test -e lib || ln -s . lib
	test -e log || ln -s . log
	test -e run || ln -s . run

world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
