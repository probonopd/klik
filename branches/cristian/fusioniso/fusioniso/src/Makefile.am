moddir = $(libdir)/umview/modules

DEVELFLAGS =

if ENABLE_DEBUG
DEVELFLAGS += -D_DEBUG
endif

COMMONSOURCES = fusioniso.c fusioniso_fs.c fusioniso_isofs.c fusioniso_debug.c

AM_CFLAGS = $(GLIB2_CFLAGS) $(FUSE_CFLAGS) -Wall #-ggdb
AM_LDFLAGS = -module -avoid-version -export-dynamic
AM_LIBTOOLFLAGS = --tag=disable-static
AM_CPPFLAGS = -D_FILE_OFFSET_BITS=64 $(DEVELFLAGS)

bin_PROGRAMS = fusioniso
mod_LTLIBRARIES = umfuse_fusioniso.la

noinst_HEADERS = fusioniso.h fusioniso_fs.h fusioniso_isofs.h fusioniso_debug.h

fusioniso_SOURCES = $(COMMONSOURCES)
fusioniso_LDFLAGS = $(GLIB2_LIBS) $(FUSE_LIBS) -lz -lpthread
# Leave this also if it seems a no-op
fusioniso_CFLAGS = $(AM_CFLAGS)

umfuse_fusioniso_la_SOURCES = $(COMMONSOURCES)
umfuse_fusioniso_la_LDFLAGS = $(AM_LDFLAGS) $(GLIB2_LIBS) $(FUSE_LIBS) -lz -lpthread #-shared
umfuse_fusioniso_la_CPPFLAGS = $(AM_CPPFLAGS) #-D_EXPERIMENTAL_

install-data-hook:
	cd "$(DESTDIR)/$(moddir)" && rm -f $(mod_LTLIBRARIES)
