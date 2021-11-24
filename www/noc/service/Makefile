#
#  Synopsis:
#	Development/Install Makefile for editing SQL table "www_serice".
#  Note:
#	Need to add permissions to install invocation, in particular,
#	cgi-bin/
#
include ../../../local.mk
include ../../../jmscott.mk
include ../../../blobio.mk
include ../noc.mk

all: service service.d/help.pl

clean:
	rm -f service service.d/help.pl

install: all

	install -d $(WWW_PREFIX)/lib
	install -d $(WWW_PREFIX)/lib
	install -d $(WWW_PREFIX)/lib/service.d
	install -d $(WWW_PREFIX)/htdocs/service
	install -d $(WWW_PREFIX)/cgi-bin/
	install service $(WWW_PREFIX)/cgi-bin

	install								\
		service.d/help.pl					\
		service.d/post.add.pl					\
		service.d/post.del.pl					\
		service.d/probe-port.pl					\
		service.d/table.del.pl					\
		service.d/table.pg.pl					\
		service.d/table.pl					\
		$(WWW_PREFIX)/lib/service.d
	install								\
		add.shtml						\
		delete.shtml						\
		index.shtml						\
		nav.shtml						\
		pg.shtml						\
		$(WWW_PREFIX)/htdocs/service
	install service.cgi $(WWW_PREFIX)/lib

distclean:
	rm -f $(WWW_PREFIX)/cgi-bin/service
	rm -rf $(WWW_PREFIX)/lib/service.d
	rm -rf $(WWW_PREFIX)/htdocs/service

service service.d/help.pl: service.cgi
	cgi2perl5 --source-path service.cgi
	chmod a+rx service
world: all
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
