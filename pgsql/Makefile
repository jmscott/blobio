#
#  Synopsis:
#  	Ye old makefile
#
include ../blobio.mk
include ../local.mk

install:
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		blobio.flow.example					\
		contrib/udig/udig.sql.in				\
		eat-unroll.sql						\
		eat-wrap-missed.sql					\
		merge-biod_request_stat.sql				\
		schema.sql						\
		$(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		cron-eat-unroll						\
		cron-rrd2pg						\
		eat-wrap-missed						\
		fs-eat							\
		fs-xor-service						\
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
