# $PostgreSQL: pgsql/contrib/udig/Makefile,v 1.4 2007/11/13 00:13:19 tgl Exp $

MODULE_big = udig
OBJS = udig.o
DATA_built = udig.sql
DATA = uninstall_udig.sql

#SHLIB_LINK += $(UDIG_LIBS)

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/udig
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
