#
#  Synopsis:
#	Development/Install Makefile for noc.blob.$(NOC_DNS_VHOST_SUFFIX)
#  Note:
#	Need to add permissions to install invocation, in particular,
#	cgi-bin/
#
include ../../../local.mk
include ../../../jmscott.mk
include ../../../blobio.mk
include ../noc.mk

ifndef NOC_DNS_VHOST_SUFFIX
$(error NOC_DNS_VHOST_SUFFIX is not set)
endif

all:
	true
clean:
	true

install: all

	install -d $(WWW_PREFIX)/htdocs/extra
	install								\
		index.shtml						\
		env.shtml						\
		$(WWW_PREFIX)/htdocs/extra

distclean:
	rm -rf $(WWW_PREFIX)/htdocs/extra

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
