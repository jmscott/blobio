#
#  Synopsis:
#  	Development make rules
#  Yacc:
#	Compiled with yacc at
#
#		export GO111MODULE=off
#		/usr/local/go/bin/go get -v -u golang.org/x/tools/cmd/goyacc
#		/usr/local/go/bin/go get -v -u github.com/lib/pq
#
#	which installs goyacc in $GOPATH/bin.
#
#	ubunutu22 packages goyacc in
#
#		apt install golang-golang-x-tools
#  Note:
#	Need to eliminate redundant create of dir such as /bin, /sbin.
#
#	Consider always compiling in pprof.
#	Apparently the execution overhead of pprof is not much.
#
include ../local.mk
include ../blobio.mk

_MAKE=$(MAKE) $(MFLAGS)

MKMK=blobio-flowd.mkmk

SRCs := $(shell  (. ./$(MKMK) && echo $$SRCs))
BINs := $(shell  (. ./$(MKMK) && echo $$BINs))
SBINs := $(shell  (. ./$(MKMK) && echo $$SBINs))
LIBs := $(shell  (. ./$(MKMK) && echo $$LIBs))
COMPILEs := $(shell  (. ./$(MKMK) && echo $$COMPILEs))
GOSRCs := $(shell  (. ./$(MKMK) && echo $$GOSRCs))

export GO111MODULE := off

GOYACC=$(shell $(GOEXE) env GOPATH)/bin/goyacc

#  Note: why?
GOHOSTOS=$(shell $(GOEXE) env GOHOSTOS)
GOHOSTARCH=$(shell $(GOEXE) env GOHOSTARCH)
GOPKG_OSARCH=$(GOHOSTOS)_$(GOHOSTARCH)

#
#  To enable race detection, uncomment GO_BUILD_RACE
#
#GO_BUILD_RACE=-race

#  Do 'go tool compile' for list of compile time flags
#
#  Disable opimixations like function inline
#	-B	disable bounds checking
#	-l	disable inlining
#	-N	disable optimizations
#  
#GCFLAGS=-gcflags "-N -l"
#GCFLAGS=-gcflags "-l"

all: flowd flowd-execv

flowd: $(GOSRCs)
	$(GOEXE) build $(GCFLAGS) $(GO_BUILD_RACE) $(GOSRCs)

install-dirs:
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/flowd
install: all
	$(_MAKE) install-dirs

	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		$(SRCs)							\
		$(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		$(BINs)							\
		$(DIST_ROOT)/bin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rx,go=		\
		$(SBINs)						\
		$(DIST_ROOT)/sbin
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=r		\
		$(LIBs)							\
		$(DIST_ROOT)/lib

clean:
	rm -f $(COMPILEs) y.output

distclean:
	rm -rf $(DIST_ROOT)/src/flowd
	test ! -d $(DIST_ROOT)/bin || (cd $(DIST_ROOT)/bin && rm -f $(BINs))
	test ! -d $(DIST_ROOT)/lib || (cd $(DIST_ROOT)/lib && rm -f $(LIBs))
	test ! -d $(DIST_ROOT)/sbin || (cd $(DIST_ROOT)/sbin && rm -f $(SBINs))

fmt: $(GOSRC)
	$(GOEXE) fmt $(GOSRC)
vet: $(GOSRC)
	$(GOEXE) vet $(GOSRC)

parser.go: parser.y
	$(GOYACC) -o parser.go parser.y || (rm -f parser.go; exit 1)
	$(GOEXE) fmt parser.go
	rm -f y.output

flowd-execv: flowd-execv.c
	cc $(CFLAGS)							\
		flowd-execv.c						\
		-o flowd-execv						\
		-L $(JMSCOTT_ROOT)/lib					\
		-ljmscott

dev-links:
	mkdir -p attic
	test -e attic/log || mkdir attic/log
	test -e data || ln -s . data
	test -e etc || ln -s . etc
	test -e lib || ln -s . lib
	test -e log || ln -s . log
	test -e profile || ln -s ../profile.example profile
	test -e rrd || ln -s . rrd
	test -e run || ln -s . run
	test -e sbin || ln -s . sbin
	test -e spool || ln -s . spool
clean-links:
	rm -rf attic data etc lib log profile rrd run sbin spool
goenv:
	$(GOEXE) env
doc:
	godoc flowd
lint:
	(golint $(filter-out parser.go,$(GOSRC))			|\
		fgrep -v underscore					|\
		fgrep -v 'use CamelCase'				|\
		fgrep -v 'should not use dot imports'			|\
		grep -v 'receiver name .* should be consistent with previous receiver name'); exit 0

world:
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
