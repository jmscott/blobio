#
#  Synopsis:
#	Ye'old Makefile for sync code used by various daemons.
#

include ../blobio.mk
include ../local.mk

SBINs=									\
	make-sync-root							\
	reboot-sync							\
	sync-put							\
	sync-wrap							\
	walk-wrap-set							\

LIBs=									\
	blobio.sync.remote.service.example				\
	com.blobio.ssh-tunnel.remote.plist.example			\
	com.blobio.sync.plist.example					\
	not-in-service.sql						\
	service-by-interval.sql						\
	sync-remote.flow.example					\

install:
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
distclean:
	cd $(DIST_ROOT)/sbin && rm -f $(SBINs)
	cd $(DIST_ROOT)/lib && rm -f $(LIBs)

dev-links:
	test -e lib || ln -s . lib
