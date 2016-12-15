include ../local.mk

DIST_WWW=$(DIST_ROOT)/www

all: env blobio

env: env.cgi
	cgi2perl5 --source-path env.cgi
	chmod +x env

clean:

distclean:
	rm -rf $(DIST_WWW)/htdocs
	rm -rf $(DIST_WWW)/cgi-bin
	rm -rf $(DIST_WWW)/lib

install: all
	#  setup the directories
ifdef DIST_ROOT
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_WWW)/cgi-bin
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_WWW)/htdocs
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_WWW)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER)			\
		-d $(DIST_WWW)/lib/env.d
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		env.shtml						\
		index.shtml						\
		favicon.ico						\
		$(DIST_WWW)/htdocs
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		env.d/div.pl						\
		env.d/dl.pl						\
		env.d/help.pl						\
		env.d/input.pl						\
		env.d/text.pl						\
		$(DIST_WWW)/lib/env.d
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		apache2-vhost.conf.example				\
		env.cgi							\
		$(DIST_WWW)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=xr,o=		\
		env							\
		$(DIST_WWW)/cgi-bin
endif

dev-links:
	test -e htdocs || ln -s . htdocs
	test -e cgi-bin || ln -s . cgi-bin
	test -e lib || ln -s . lib

blobio: blobio.cgi
	cgi2perl5 --source-path blobio.cgi
	chmod +x blobio
