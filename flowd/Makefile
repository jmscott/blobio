#
#  Synopsis:
#  	Development make rules
#  Blame:
#	jmscott@setspace.com
#	setspace@gmail.com
#
include ../blobio.mk
include ../local.mk

export GOPATH := $(DIST_ROOT):$(GODIST)/..
export PATH := $(GODIST)/bin:$(PATH)

GOHOSTOS=$(shell $(GODIST)/bin/go env GOHOSTOS)
GOHOSTARCH=$(shell $(GODIST)/bin/go env GOHOSTARCH)
GOPKG_OSARCH=$(GOHOSTOS)_$(GOHOSTARCH)

#
#  To enable race detection, uncomment GO_BUILD_RACE
#
#GO_BUILD_RACE=-race

EXEC=flowd flowd-os-exec

SRC=									\
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
	xdr.go

#  Disable opimixations like function inline
#GCFLAGS=-gcflags "-N -l"

all: $(EXEC)

flowd: $(SRC)
	go build $(GCFLAGS) $(GO_BUILD_RACE) $(SRC)

install: all
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,go=	\
		-d $(DIST_ROOT)/http/go/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,go=	\
		-d $(DIST_ROOT)/http/go/lib
	install -g $(DIST_GROUP) -o $(DIST_USER) -m u=rwx,g=rx,o=	\
		-d $(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		parser.y						\
		$(SRC)							\
		$(DIST_ROOT)/src/flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ug=r,o=		\
		parser.y						\
		$(SRC)							\
		$(DIST_ROOT)/http/go/src/flowd
	go install $(GO_BUILD_RACE) flowd
	install -g $(DIST_GROUP) -o $(DIST_USER) -m ugo=rx		\
		fdr2pg							\
		fdr2psql						\
		xdr2pg							\
		xdr2psql						\
		$(DIST_ROOT)/bin

clean:
	rm -f $(EXEC) parser.go y.output $(EXEC)

distclean:
	rm -rf 								\
		$(DIST_ROOT)/src/flowd	 				\
		$(DIST_ROOT)/bin/flowd					\
		$(DIST_ROOT)/bin/flowd-os-exec


fmt: $(SRC)
	go fmt $(SRC)
vet: $(SRC)
	go vet $(SRC)

parser.go: parser.y
	go tool yacc -o parser.go parser.y || (rm -f parser.go; exit 1)
	go fmt parser.go

flowd-os-exec: flowd-os-exec.c
	cc $(CFLAGS) -o flowd-os-exec flowd-os-exec.c

dev-links:
	test -e lib || ln -s . lib
	test -e etc || ln -s . etc
	test -e log || ln -s . log
	test -e sbin || ln -s . sbin
	test -e profile || ln -s ../profile.example profile
env:
	env
goenv:
	go env
doc:
	godoc flowd
lint:
	(golint $(filter-out parser.go,$(SRC))				|\
		fgrep -v underscore					|\
		fgrep -v 'use CamelCase'				|\
		fgrep -v 'should not use dot imports'			|\
		grep -v 'receiver name .* should be consistent with previous receiver name'); exit 0
