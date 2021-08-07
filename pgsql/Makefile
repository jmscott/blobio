#
#  Synopsis:
#  	Ye old makefile
#
include ../local.mk
include ../blobio.mk

LIBs="
	blobio.flow.example						\
	contrib/udig/udig.sql.in					\
	cron-rummy.sql							\
	eat-unroll.sql							\
	eat-wrap-missed.sql						\
	merge-bio4d_request_stat.sql					\
	schema.sql							\
"
SBINs="
	cron-bio4d-stat2pg						\
	cron-eat-unroll							\
	cron-rummy							\
	eat-wrap-missed							\
	fs-eat								\
	fs-xor-service							\
	minus-service							\
"

install:
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

db:
	createdb blobio 						\
		--encoding=UTF8 					\
		--template=template0 					\
		--owner blobio 						\
		--user postgres

dev-links:
	test -e etc || ln -s . etc
	test -e lib || ln -s . lib
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e log || ln -s . log

frisk ff:
	flowd frisk blobio.flow.example
distclean:
	test ! -d $(DIST_ROOT)/lib || (cd $(DIST_ROOT)/lib;  rm -f $(LIBs))
	test ! -d $(DIST_ROOT)/sbin || (cd $(DIST_ROOT)/sbin;  rm -f $(SBINs))
