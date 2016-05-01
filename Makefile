#
#  Synopsis:
#	Root makefile for blobio clients and servers.
#  Depends:
#	local.mk, derived from local-<os>.mk.example
#  Blame:
#  	jmscott@setspace.com
#  	setspace@gmail.com
#  Note:
#  	Parallel jobs have problems due to incorrection productions.
#  	In particular,
#
#  		make -j 4 clean all
#
#	fails, since, I (jmscott), believe that the proper .PHONY target
#	directories have not been identified.  However, doing a
#		
#		make clean && make -j 4 all
#
#	appears to work correctly.
#
#	The 'http' directory must be renamed to 'www'
#
#	Need to specify client of 
#
include local.mk

all:
	cd sha1;	$(MAKE) all
ifdef DIST_ROOT
	cd biod;	$(MAKE) all
endif
	cd cli;		$(MAKE) all
	cd flowd;	$(MAKE) all

clean:
	cd sha1;	$(MAKE) clean
	cd biod;	$(MAKE) clean
	cd flowd;	$(MAKE) clean
	cd cli;		$(MAKE) clean

distclean:

ifeq "$(DIST_ROOT)" "/usr"
	@echo 'refuse to distclean /usr'
	exit 1
endif

ifeq "$(DIST_ROOT)" "/usr/local"
	@echo 'refuse to distclean /usr/local'
	exit 1
endif

ifdef DIST_ROOT
	cd flowd;	$(MAKE) distclean
	rm -rf $(DIST_ROOT)/bin
	rm -rf $(DIST_ROOT)/lib
	rm -rf $(DIST_ROOT)/sbin
	rm -rf $(DIST_ROOT)/src
endif

install: all
ifdef PREFIX
	echo "Installing to $(PREFIX)"
	test -d /usr/local/bin && cp cli/blobio $(PREFIX)/bin
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
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-d $(DIST_ROOT)/data/sha_fs
	install -g $(DIST_GROUP) -o $(DIST_USER) 			\
		-d $(DIST_ROOT)/data/sha_fs/tmp
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		pgsql/bio-merge-service					\
		$(DIST_ROOT)/bin

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=	\
		cron-reboot						\
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
endif

dev-links:
	test -e log || ln -s . log
	test -e htdocs || ln -s . htdocs
	test -e cgi-bin || ln -s . cgi-bin
	test -e lib || ln -s . lib
