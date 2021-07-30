#
#  Synopsis:
#  	Development make rules
#  Yacc:
#	Compiled with yacc at
#
#		/usr/local/go/bin/go get -v -u golang.org/x/tools/cmd/goyacc
#		export GO111MODULE=off
#		/usr/local/go/bin/go get -v -u github.com/lib/pq
#
#	wish installs goyacc in $GOPATH/bin.
#  Note:
#	Need to eliminate redundant create of dir such as /bin, /sbin.
#
#	Consider always compiling in pprof.
#	Apparently the execution overhead of pprof is not much.
#
include ../local.mk
include ../blobio.mk

export PATH := $(GODIST)/bin:$(PATH)
GO=$(GODIST)/bin/go

export GO111MODULE := off

GOYACC=$(shell $(GO) env GOPATH)/bin/goyacc

#  Note: why?
GOHOSTOS=$(shell $(GO) env GOHOSTOS)
GOHOSTARCH=$(shell $(GO) env GOHOSTARCH)
GOPKG_OSARCH=$(GOHOSTOS)_$(GOHOSTARCH)

#
#  To enable race detection, uncomment GO_BUILD_RACE
#
#GO_BUILD_RACE=-race

EXEC=flowd flowd-execv

GOSRC=									\
	flowd.go							\
	brr.go								\
	command.go							\
	compile.go							\
	fdr.go								\
	file.go								\
	flow.go								\
	log.go								\
	parser.go							\
	pid-log.go							\
	qdr.go								\
	server.go							\
	sql.go								\
	tail.go								\
	tsort.go							\
	xdr.go

#  Do 'go tool compile' for list of compile time flags
#
#  Disable opimixations like function inline
#	-B	disable bounds checking
#	-l	disable inlining
#	-N	disable optimizations
#  
#GCFLAGS=-gcflags "-N -l"
#GCFLAGS=-gcflags "-l"

all: $(EXEC)

flowd: $(GOSRC)
	$(GO) build $(GCFLAGS) $(GO_BUILD_RACE) $(GOSRC)

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=x,o=	\
		-d $(DIST_ROOT)/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,go=		\
		-d $(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		flowd-execv.c						\
		parser.y						\
		$(GOSRC)						\
		$(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		fdr2pg							\
		fdr2psql						\
		flowd							\
		flowd-execv						\
		xdr2pg							\
		xdr2psql						\
		$(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		boot-flowd						\
		launchd-flowd						\
		kill-flowd						\
		$(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=r		\
		blobio.flowd.plist.example				\
		blobio.flowd.service.example				\
		$(DIST_ROOT)/lib

clean:
	rm -f $(EXEC) parser.go y.output y.go

distclean:
	rm -rf 								\
		$(DIST_ROOT)/src/flowd	 				\
		$(DIST_ROOT)/bin/flowd					\
		$(DIST_ROOT)/bin/flowd-execv				\
		$(DIST_ROOT)/sbin/launchd-flowd


fmt: $(GOSRC)
	$(GO) fmt $(GOSRC)
vet: $(GOSRC)
	$(GO) vet $(GOSRC)

parser.go: parser.y
	$(GOYACC) -o parser.go parser.y || (rm -f parser.go; exit 1)
	$(GO) fmt parser.go
	rm -f y.output

flowd-execv: flowd-execv.c
	cc $(CFLAGS) -o flowd-execv flowd-execv.c

dev-links:
	test -e lib || ln -s . lib
	test -e etc || ln -s . etc
	test -e log || ln -s . log
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e spool || ln -s . spool
	test -e profile || ln -s ../profile.example profile
env:
	env
goenv:
	$(GO) env
doc:
	godoc flowd
lint:
	(golint $(filter-out parser.go,$(GOSRC))			|\
		fgrep -v underscore					|\
		fgrep -v 'use CamelCase'				|\
		fgrep -v 'should not use dot imports'			|\
		grep -v 'receiver name .* should be consistent with previous receiver name'); exit 0

world:
	$(MAKE) $(MFLAGS) clean
	$(MAKE) $(MFLAGS) all
	$(MAKE) $(MFLAGS) distclean
	$(MAKE) $(MFLAGS) install
