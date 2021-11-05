#
#  Synopsis:
#	Development/Install Makefile for noc.blobio.$(DASH_DNS_VHOST_SUFFIX)
#  Usage:
#	export DASH_DNS_VHOST_SUFFIX=noc
#	make clean all install
#
include ../../local.mk
include ../../blobio.mk
include noc.mk

ifndef NOC_DNS_VHOST_SUFFIX
$(error NOC_DNS_VHOST_SUFFIX is not set)
endif

all:
	true
clean:
	true

install: all

	install -d $(NOC_PREFIX)/etc
	test -e $(NOC_PREFIX)/etc/passwd				||\
		install -m u=rw,g=r,o=					\
			../apache2-passwd.example 			\
			$(NOC_PREFIX)/etc/passwd
	install -d $(NOC_PREFIX)/lib
	install -d -m u=rwx,g=x,o= $(NOC_PREFIX)/cgi-bin
	install -d $(NOC_PREFIX)/htdocs
	install -m a=rwx -d $(NOC_PREFIX)/tmp

	install 							\
		index.shtml						\
		$(WWW_PREFIX)/htdocs

	#  set up symbolic links to $(JMSCOTT_ROOT) tools
	test -e $(NOC_PREFIX)/cgi-bin/jmscott				||\
		ln -s $(JMSCOTT_ROOT)/www/cgi-bin $(NOC_PREFIX)/cgi-bin/jmscott
	test -e $(NOC_PREFIX)/lib/jmscott				||\
		ln -s $(JMSCOTT_ROOT)/www/lib $(NOC_PREFIX)/lib/jmscott

	#  set symbolic link to $(SETSPACE_ROOT)/www/htdocs/icon
	test -e $(WWW_PREFIX)/htdocs/icon				||\
		ln -s $(SETSPACE_PREFIX)/www/htdocs/icon		\
		      $(WWW_PREFIX)/htdocs/icon

	cd schema && $(MAKE) $(MFLAGS) install
	cd gw && $(MAKE) $(MFLAGS) install
	cd tag-http && $(MAKE) $(MFLAGS) install

distclean:
	cd schema && $(MAKE) $(MFLAGS) distclean
	cd gw && $(MAKE) $(MFLAGS) distclean
	cd tag-http && $(MAKE) $(MFLAGS) distclean
	rm -rf $(WWW_PREFIX)/lib
	rm -rf $(WWW_PREFIX)/cgi-bin
	rm -rf $(WWW_PREFIX)/htdocs

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
