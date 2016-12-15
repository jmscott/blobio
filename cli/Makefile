#
#  Synopsis:
#	Install client scripts
#

include ../blobio.mk
include ../local.mk

all:
	cd blobio;	$(MAKE) all

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-cat							\
		bio-eat							\
		bio-file 						\
		bio-get							\
		bio-give						\
		bio-merge						\
		bio-more						\
		bio-put							\
		bio-put-file						\
		bio-wrap-roll						\
		bio-take						\
		bio-unroll						\
		bio-wrap						\
		bio-zap							\
		$(DIST_ROOT)/bin
	cd blobio;	$(MAKE) install

clean:
	cd blobio;  $(MAKE) clean
