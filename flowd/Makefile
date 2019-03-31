#
#  Synopsis:
#  	Development make rules
#  Yacc:
#	Compiled with yacc at
#
#		/usr/local/go/bin/go get -u golang.org/x/tools/cmd/goyacc
#
#	wish installs goyacc in $GOPATH/bin.
#
#	Note, missing yacc tokens for output genrated by modernc.org/goyacc.
#
include ../blobio.mk
include ../local.mk

export PATH := $(GODIST)/bin:$(PATH)

GOHOSTOS=$(shell $(GODIST)/bin/go env GOHOSTOS)
GOYACC=$(shell $(GODIST)/bin/go env GOPATH)/bin/goyacc
GOHOSTARCH=$(shell $(GODIST)/bin/go env GOHOSTARCH)
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
	go build $(GCFLAGS) $(GO_BUILD_RACE) $(GOSRC)

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,go=	\
		-d $(DIST_ROOT)/http/go/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,go=	\
		-d $(DIST_ROOT)/http/go/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		parser.y						\
		$(GOSRC)						\
		$(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		flowd-execv.c						\
		$(DIST_ROOT)/src
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		parser.y						\
		$(GOSRC)						\
		$(DIST_ROOT)/http/go/src/flowd
	go install $(GCFLAGS) $(GO_BUILD_RACE) flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		fdr2pg							\
		fdr2psql						\
		flowd-execv						\
		xdr2pg							\
		xdr2psql						\
		$(DIST_ROOT)/bin

clean:
	rm -f $(EXEC) parser.go y.output y.go

distclean:
	rm -rf 								\
		$(DIST_ROOT)/src/flowd	 				\
		$(DIST_ROOT)/src/flowd-execv.c 				\
		$(DIST_ROOT)/bin/flowd					\
		$(DIST_ROOT)/bin/flowd-execv


fmt: $(GOSRC)
	go fmt $(GOSRC)
vet: $(GOSRC)
	go vet $(GOSRC)

parser.go: parser.y
	$(GOYACC) -o parser.go parser.y || (rm -f parser.go; exit 1)
	go fmt parser.go

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
	go env
t: t.go
	go build t.go
doc:
	godoc flowd
lint:
	(golint $(filter-out parser.go,$(GOSRC))			|\
		fgrep -v underscore					|\
		fgrep -v 'use CamelCase'				|\
		fgrep -v 'should not use dot imports'			|\
		grep -v 'receiver name .* should be consistent with previous receiver name'); exit 0
