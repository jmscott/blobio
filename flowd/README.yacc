If you wish to change the flowd grammar then you will need to install the go
yacc.  Unfortunatly, golang version 1.8 eliminated the builtin yacc tool.

Fortunatly building and installing the yacc tool is relatively easy.
The instructions are for go1.10 with go installed in /usr/local/go

	# GO=/usr/local/go/bin/go
	# GOYACC=golang.org/x/tools/cmd/goyacc
	# SRC=/usr/local/src/golang.org/x/tools/cmd/goyacc
	#
	# GOPATH=/usr/local $GO get -v $DIST
	# GOPATH=/usr/local $GO install -v $GOYACC
	# cd /usr/local/src/$GOYACC
	# $GO build yacc
	# mv yacc /usr/local/bin/goyacc		#  or whereever $PATH points

The Makefile file invokes yacc as "goyacc" and not as "yacc".
