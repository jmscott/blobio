#
#  Synopsis:
#  	Ye old makefile
#
include ../local.mk
include ../blobio.mk

install:
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		blobio.flow.example					\
		contrib/udig/udig.sql.in				\
		eat-unroll.sql						\
		eat-wrap-missed.sql					\
		merge-bio4d_request_stat.sql				\
		schema.sql						\
		$(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		cron-eat-unroll						\
		cron-bio4d-stat2pg					\
		eat-wrap-missed						\
		fs-eat							\
		fs-xor-service						\
		minus-service						\
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
