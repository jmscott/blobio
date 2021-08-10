#
#  Synopsis:
#	Ye'old Makefile for sync code used by various daemons.
#

include ../local.mk
include ../blobio.mk

SRCs=jaccard.go jaccard-tip.go
SBINs=									\
	jaccard								\
	jaccard-tip							\
	make-sync-root							\
	purge-sync-cache						\
	reboot-sync							\
	rummy-chain							\
	sync-put							\
	sync-wrap							\
	sync-wrap-chain							\
	walk-wrap-set							\

LIBs=									\
	blobio.sync.remote.flowd.plist.example				\
	blobio.sync.remote.flowd.service.example			\
	jaccard-all.conf.example					\
	jaccard-tip.conf.example					\
	not-in-service.sql						\
	profile-sync.example						\
	rummy-chain.sql							\
	sync-remote.flow.example					\
	syncness.sql							\

all: jaccard jaccard-tip

jaccard-tip: jaccard-tip.go
	$(GODIST)/bin/go build jaccard-tip.go
jaccard: jaccard.go
	$(GODIST)/bin/go build jaccard.go

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
		-m u=rwx,go= -d $(DIST_ROOT)/sync/root
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/sync
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		jaccard.go						\
		jaccard-tip.go						\
		$(DIST_ROOT)/src/sync

distclean:
	cd $(DIST_ROOT)/sbin && rm -f $(SBINs)
	cd $(DIST_ROOT)/lib && rm -f $(LIBs)
clean:
	rm -f jaccard-tip

dev-links:
	test -e sbin || ln -s . sbin
	test -e lib || ln -s . lib
	test -e etc || ln -s . etc
