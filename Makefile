#
#  Synopsis:
#	Root makefile for blobio clients and servers.
#  Depends:
#	local.mk, derived from local-<os>.mk.example
#  Usage:
#	mkdir -p $HOME/dev
#	cd $HOME/dev
#	svn https://github.com/jmscott/blobio
#	cd blobio/trunk
#	cp local-<os>.mk.example local.mk
#	<edit local.mk>
#	make -j 4 clean all distclean install
#
include local.mk

all:
ifdef DIST_ROOT
	cd biod;	$(MAKE) $(MAKEFLAGS) all
endif
	cd cli;		$(MAKE) $(MAKEFLAGS) all
	cd flowd;	$(MAKE) $(MAKEFLAGS) all
	cd www;		$(MAKE) $(MAKEFLAGS) all

clean:
	cd biod;	$(MAKE) $(MAKEFLAGS) clean
	cd flowd;	$(MAKE) $(MAKEFLAGS) clean
	cd cli;		$(MAKE) $(MAKEFLAGS) clean
	cd www;		$(MAKE) $(MAKEFLAGS) clean

distclean:
	cd biod;	$(MAKE) $(MAKEFLAGS) distclean

ifeq "$(DIST_ROOT)" "/usr"
	@echo 'refuse to distclean /usr'
	exit 1
endif

ifeq "$(DIST_ROOT)" "/usr/local"
	@echo 'refuse to distclean /usr/local'
	exit 1
endif

ifdef DIST_ROOT
	cd flowd;	$(MAKE) $(MAKEFLAGS) distclean
	cd www;		$(MAKE) $(MAKEFLAGS) distclean
	rm -rf $(DIST_ROOT)/bin
	rm -rf $(DIST_ROOT)/lib
	rm -rf $(DIST_ROOT)/sbin
	rm -rf $(DIST_ROOT)/src
endif

install: all
ifdef PREFIX
	echo "Installing to $(PREFIX)"
	test -d $(PREFIX)/bin && cp cli/blobio/blobio $(PREFIX)/bin
endif

	#  setup the directories
ifdef DIST_ROOT
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/pgsql
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/biod
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/src/blobio
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/etc
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/log
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/run
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/spool
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-d $(DIST_ROOT)/spool/wrap
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/data
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/www
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-d $(DIST_ROOT)/data/sha_fs
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-d $(DIST_ROOT)/data/sha_fs/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		pgsql/bio-merge-service					\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=	\
		cron-reboot						\
		dev-reboot						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		bash_login-dev.example					\
		crontab.example						\
		profile.example						\
		psqlrc.example						\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		README							\
		$(DIST_ROOT)/lib/README.blobio
	cd biod; $(MAKE) install
	cd cli; $(MAKE) install
	cd pgsql; $(MAKE) install
	cd flowd; $(MAKE) install
	cd www; $(MAKE) install
endif

dev-links:
	test -e log || ln -s . log
	test -e htdocs || ln -s . htdocs
	test -e cgi-bin || ln -s . cgi-bin
	test -e lib || ln -s . lib

#  Note:  breaks on gnu make!
world:
	$(MAKE) $(MAKEFLAGS) clean
	$(MAKE) $(MAKEFLAGS) all
	$(MAKE) $(MAKEFLAGS) distclean
	$(MAKE) $(MAKEFLAGS) install
