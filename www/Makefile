#
#  Synopsis:
#	Makefile for www development environment of $(BLOBIO_ROOT)
#  Note:
#	Move "ifdef NOC_DNS_VHOST_SUFFIX" into noc/Makefile
#
include ../local.mk
include ../blobio.mk

WWW_ROOT=$(BLOBIO_PREFIX)/www

all:
ifdef NOC_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) all
endif

install:
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) 		\
		-d $(BLOBIO_PREFIX)
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) 		\
		-d $(WWW_ROOT)
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) 		\
		-d $(WWW_ROOT)/lib
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) 		\
		-m a+wrx						\
		-d $(WWW_ROOT)/tmp
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) 		\
		-d $(WWW_ROOT)/htdocs

	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) -m ug=r,o=	\
		apache2-vhost.conf.example				\
		$(WWW_ROOT)/lib
ifdef NOC_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) install
endif

distclean:
ifdef NOC_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) distclean
endif
	rm -rf $(WWW_ROOT)/htdocs
	rm -rf $(WWW_ROOT)/lib

clean:
ifdef NOC_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) clean
endif

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
