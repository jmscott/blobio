#
#  Synopsis:
#  	Ye old makefile
#  Blame:
#  	jmscott@setspace.com
#  	setspace@gmail.com
#
include ../blobio.mk
include ../local.mk

install:
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		blobio.flow.example					\
		contrib/udig.sql.in					\
		eat-unroll.sql						\
		eat-wrap-missed.sql					\
		schema.sql						\
		$(DIST_ROOT)/lib

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=	\
		cron-eat-unroll						\
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

frisk ff:
	flowd frisk blobio.flow.example
