#
#  Synopsis:
#  	Ye old makefile
#
include ../local.mk
include ../blobio.mk

SRCs=	roll2stat_json.go merge-stdin-roll2stat_json.pgc
LIBs=									\
	blobio.flow.example						\
	contrib/udig/udig.sql.in					\
	eat-unroll.sql							\
	eat-wrap-missed.sql						\
	merge-bio4d_request_stat.sql					\
	schema.sql							\
	select-rummy.sql						\
	select-rummy-since.sql						\

SBINs=									\
	cron-bio4d-stat2pg						\
	cron-eat-unroll							\
	cron-rummy							\
	eat-wrap-missed							\
	fs-eat								\
	fs-xor-service							\
	minus-service							\
	roll2stat_json							\

all: roll2stat_json merge-stdin-roll2stat_json

roll2stat_json: roll2stat_json.go
	$(GODIST)/bin/go build roll2stat_json.go

install: $(SBINs) $(LIBs)
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(LIBs)							\
		$(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(SRCs)							\
		$(DIST_ROOT)/src

	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin

merge-stdin-roll2stat_json: merge-stdin-roll2stat_json.pgc
	ecpg merge-stdin-roll2stat_json.pgc
	$(CC) $(CFLAGS) 						\
		-I$(JMSCOTT_INC)					\
		-I$(PGINC) 						\
		merge-stdin-roll2stat_json.c				\
		-o merge-stdin-roll2stat_json -L$(PGLIB) -lecpg
	rm merge-stdin-roll2stat_json.c

dev-links:
	test -e etc || ln -s . etc
	test -e lib || ln -s . lib
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e log || ln -s . log
	test -e spool || ln -s . spool

distclean:
	test ! -d $(DIST_ROOT)/lib || (cd $(DIST_ROOT)/lib;  rm -f $(LIBs))
	test ! -d $(DIST_ROOT)/sbin || (cd $(DIST_ROOT)/sbin;  rm -f $(SBINs))
	test ! -f $(DIST_ROOT)/src/roll2stat_json.go || 		\
		rm -f $(DIST_ROOT)/src/roll2stat_json.go
clean:
	rm -f merge-stdin-roll2stat_json roll2stat_json
world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
