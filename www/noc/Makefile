#
#  Synopsis:
#	Development/Install Makefile for noc.blob.$(NOC_DNS_VHOST_SUFFIX)
#  Note:
#	Need to add permissions to install invocation, in particular,
#	cgi-bin/
#
include ../../local.mk
include ../../jmscott.mk
include ../../blobio.mk
include noc.mk

ifndef NOC_DNS_VHOST_SUFFIX
$(error NOC_DNS_VHOST_SUFFIX is not set)
endif

all:
	cd service && $(MAKE) $(MFLAGS) all
	cd extra && $(MAKE) $(MFLAGS) all
clean:
	cd service && $(MAKE) $(MFLAGS) clean
	cd extra && $(MAKE) $(MFLAGS) clean

install: all
	install -d $(WWW_PREFIX)/etc
	#  install bootstrap password noc from generic www
	test -e $(WWW_PREFIX)/etc/passwd				||\
		install -m u=rw,g=r,o=					\
			../apache2-passwd.example 			\
			$(WWW_PREFIX)/etc/passwd
	install -d $(WWW_PREFIX)/lib
	install								\
		psqlrc.example						\
		schema.sql						\
		$(WWW_PREFIX)/lib
	install -d -m u=rwx,g=x,o= $(WWW_PREFIX)/cgi-bin
	install -d $(WWW_PREFIX)/htdocs
	install -d $(WWW_PREFIX)/htdocs/extra
	install -m a=rwx -d $(WWW_PREFIX)/tmp

	install 							\
		footer.shtml						\
		header.shtml						\
		index.shtml						\
		nav.shtml						\
		screen.css						\
		$(WWW_PREFIX)/htdocs

	#  set up symbolic links to $(JMSCOTT_ROOT) tools
	test -e $(WWW_PREFIX)/cgi-bin/jmscott				||\
		ln -s $(JMSCOTT_ROOT)/www/cgi-bin $(WWW_PREFIX)/cgi-bin/jmscott
	test -e $(WWW_PREFIX)/lib/jmscott				||\
		ln -s $(JMSCOTT_ROOT)/www/lib $(WWW_PREFIX)/lib/jmscott
	cd service && $(MAKE) $(MFLAGS) install
	cd extra && $(MAKE) $(MFLAGS) install

distclean:
	cd service && $(MAKE) $(MFLAGS) distclean
	cd extra && $(MAKE) $(MFLAGS) distclean
	rm -rf $(WWW_PREFIX)/lib
	rm -rf $(WWW_PREFIX)/cgi-bin
	rm -rf $(WWW_PREFIX)/htdocs

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
