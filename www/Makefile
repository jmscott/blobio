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
		-d $(WWW_ROOT)/htdocs/icon

	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) -m ug=r,o=	\
			screen.css					\
		$(WWW_ROOT)/htdocs
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) -m ug=r,o=	\
		icon/README						\
		icon/android-chrome-192x192.png				\
		icon/android-chrome-512x512.png				\
		icon/apple-touch-icon.png				\
		icon/browserconfig.xml					\
		icon/favicon-16x16.png					\
		icon/favicon-32x32.png					\
		icon/favicon.ico					\
		icon/favicon.svg					\
		icon/favicon_package_v0.16.zip				\
		icon/index.html.inc					\
		icon/mstile-150x150.png					\
		icon/safari-pinned-tab.svg				\
		icon/site.webmanifest					\
		$(WWW_ROOT)/htdocs/icon
	install -g $(BLOBIO_GROUP) -o $(BLOBIO_USER) -m ug=r,o=	\
		apache2-vhost.conf.example				\
		apache2-passwd.example					\
		common-json.pl						\
		common-time.pl						\
		utf82blob.pl						\
		$(WWW_ROOT)/lib
ifdef DASH_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) install
endif

distclean:
ifdef DASH_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) distclean
endif
	rm -rf $(WWW_ROOT)/htdocs
	rm -rf $(WWW_ROOT)/lib

clean:
ifdef DASH_DNS_VHOST_SUFFIX
	cd noc && $(MAKE) $(MFLAGS) clean
endif

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
