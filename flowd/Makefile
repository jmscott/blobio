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

_MAKE=$(MAKE) $(MFLAGS)

DIST=blobio-flowd.dist

SRCs := $(shell  (. ./$(DIST) && echo $$SRCs))
BINs := $(shell  (. ./$(DIST) && echo $$BINs))
SBINs := $(shell  (. ./$(DIST) && echo $$SBINs))
LIBs := $(shell  (. ./$(DIST) && echo $$LIBs))
COMPILEs := $(shell  (. ./$(DIST) && echo $$COMPILEs))
GOSRCs := $(shell  (. ./$(DIST) && echo $$GOSRCs))

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
	$(GO) build $(GCFLAGS) $(GO_BUILD_RACE) $(GOSRCs)

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
	test -e $(DIST_ROOT)/bin && cd $(DIST_ROOT)/bin && rm -f $(BINs)
	test -e $(DIST_ROOT)/lib && cd $(DIST_ROOT)/lib && rm -f $(LIBs)
	test -e $(DIST_ROOT)/sbin && cd $(DIST_ROOT)/sbin && rm -f $(SBINs)

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
	$(_MAKE) clean
	$(_MAKE) all
	$(_MAKE) distclean
	$(_MAKE) install
