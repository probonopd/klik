# set the include path for X, qt and KDE
INCLUDES = $(all_includes) -I../../CIsofsUtl
LIBS     = -L../../CIsofsUtl

# these are the headers for your project
noinst_HEADERS = isofscreator.h

#########################################################################
# APPLICATION SECTION
#########################################################################
# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables
kde_module_LTLIBRARIES = isofsthumbnail.la

# the application source, library search path, and link libraries
isofsthumbnail_la_SOURCES = isofsthumbnail.cpp isofsthumbnail.desktop isofscreator.cpp
isofsthumbnail_la_LIBADD = $(LIB_KIO) -lCIsofsUtl
isofsthumbnail_la_LDFLAGS = -module $(all_libraries) $(KDE_PLUGIN)

kde_services_DATA =  isofsthumbnail.desktop

