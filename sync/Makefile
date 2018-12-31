#
#  Synopsis:
#	Ye'old Makefile for sync code used by various daemons.
#

include ../blobio.mk
include ../local.mk

export GOPATH := $(GODIST)/..

SRCs=jaquard.go
SBINs=									\
	jaquard								\
	make-sync-root							\
	purge-sync-cache						\
	reboot-sync							\
	rummy								\
	sync-put							\
	sync-wrap							\
	sync-wrap-chain							\
	walk-wrap-set							\

LIBs=									\
	blobio.sync.remote.service.example				\
	com.blobio.ssh-tunnel.remote.plist.example			\
	com.blobio.sync.plist.example					\
	config.jaq.example						\
	lo0-alias-127.0.0.2.plist.example				\
	not-in-service.sql						\
	profile-sync.example						\
	rummy.sql							\
	sync-remote.flow.example					\
	syncness.sql							\

all: jaquard

jaquard: jaquard.go
	$(GODIST)/bin/go build  jaquard.go

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sync
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-m u=rwx,go= -d $(DIST_ROOT)/sync/host
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/sync
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		jaquard.go					\
		$(DIST_ROOT)/src/sync

distclean:
	cd $(DIST_ROOT)/sbin && rm -f $(SBINs)
	cd $(DIST_ROOT)/lib && rm -f $(LIBs)

dev-links:
	test -e sbin || ln -s . sbin
	test -e lib || ln -s . lib
	test -e etc || ln -s . etc

dev-sync:
	rm -rf sync
	mkdir sync && cd sync 						&&\
		ln -s . etc						&&\
		ln -s . host						&&\
		ln -s . tmp						&&\
		cp ../profile-sync.example profile
