## Makefile.am for folder file meta info plugin

INCLUDES         = $(all_includes) -I../../CIsofsUtl
LIBS             = -L../../CIsofsUtl

# these are the headers for your project
noinst_HEADERS   = kfile_cmg.h

kde_module_LTLIBRARIES = kfile_cmg.la

kfile_cmg_la_SOURCES = kfile_cmg.cpp
kfile_cmg_la_LDFLAGS = $(all_libraries) -module $(KDE_PLUGIN)
kfile_cmg_la_LIBADD = $(LIB_KIO) -lCIsofsUtl

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO

services_DATA = kfile_cmg.desktop
servicesdir = $(kde_servicesdir)

messages: rc.cpp
	$(EXTRACTRC) `find . -name \*.ui -o -name \*.rc` > rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kfile_cmg.pot
