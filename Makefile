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

all:
	cd bio4d;	$(MAKE) $(MFLAGS) all
	cd cli;		$(MAKE) $(MFLAGS) all
	cd flowd;	$(MAKE) $(MFLAGS) all
	cd sync;	$(MAKE) $(MFLAGS) all

clean:
	cd bio4d;	$(MAKE) $(MFLAGS) clean
	cd cli;		$(MAKE) $(MFLAGS) clean
	cd flowd;	$(MAKE) $(MFLAGS) clean
	cd sync;	$(MAKE) $(MFLAGS) clean

distclean:
	cd bio4d;	$(MAKE) $(MFLAGS) distclean

ifeq "$(DIST_ROOT)" "/usr"
	@echo 'refuse to distclean /usr'
	exit 1
endif

ifeq "$(DIST_ROOT)" "/usr/local"
	@echo 'refuse to distclean /usr/local'
	exit 1
endif

	cd flowd;	$(MAKE) $(MFLAGS) distclean
	#cd www;		$(MAKE) $(MFLAGS) distclean
	rm -rf $(DIST_ROOT)/bin
	rm -rf $(DIST_ROOT)/lib
	rm -rf $(DIST_ROOT)/sbin
	rm -rf $(DIST_ROOT)/src

install: all
	#  setup the directories
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/root
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
		-m u=rwx,g=x,o= -d $(DIST_ROOT)/etc
	#  need group read for possible other services
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/spool
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/cache
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/cache/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/cache/data
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
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/sync
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,g=rx,o= -d $(DIST_ROOT)/sync/root
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib
	#install -g $(DIST_GROUP) -o $(DIST_USER)			\
		#-d $(DIST_ROOT)/www
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		pgsql/bio-merge-service					\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		BLOBIO_ROOT-bash					\
		brr-stat						\
		cron-pg_dump-daily					\
		cron-reboot						\
		dev-reboot						\
		ls-service						\
		run-stat						\
		run-stat-report						\
		tail-log						\
		$(DIST_ROOT)/sbin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		bash_login.example					\
		bash_login-dev.example					\
		crontab.example						\
		hamlet.txt						\
		profile.example						\
		psqlrc.example						\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		README							\
		$(DIST_ROOT)/lib/README.blobio
	cd bio4d;	$(MAKE) $(MFLAGS) install
	cd cli;		$(MAKE) $(MFLAGS) install
	cd pgsql;	$(MAKE) $(MFLAGS) install
	cd flowd;	$(MAKE) $(MFLAGS) install
	#cd www;		$(MAKE) $(MFLAGS) install
	cd sync;	$(MAKE) $(MFLAGS) install

dev-links:
	test -e log || ln -s . log
	test -e htdocs || ln -s . htdocs
	test -e cgi-bin || ln -s . cgi-bin
	test -e lib || ln -s . lib

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
