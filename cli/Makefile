#
#  Synopsis:
#	Install client scripts
#

include ../local.mk
include ../blobio.mk

all:
	cd blobio;	$(MAKE) $(MFLAGS) all

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		bio-bc160						\
		bio-cat							\
		bio-eat							\
		bio-file 						\
		bio-get							\
		bio-give						\
		bio-ls							\
		bio-merge						\
		bio-more						\
		bio-put							\
		bio-put-file						\
		bio-put-stdin						\
		bio-take						\
		bio-unroll						\
		bio-wrap						\
		bio-wrap-roll						\
		bio-zap							\
		$(DIST_ROOT)/bin
	cd blobio;	$(MAKE) $(MFLAGS) install
clean:
	cd blobio;  $(MAKE) $(MFLAGS) clean
