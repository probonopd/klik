noinst_LIBRARIES = libCmgUtl.a
noinst_HEADERS = cmg_utl.hh  link_utl.hh
libCmgUtl_a_SOURCES = link_utl.cpp cmg_utl.cpp

bin_PROGRAMS = cmginfo
cmginfo_SOURCES = main.cpp

#INCLUDES= $(all_includes)
LIBS=libCmgUtl.a

AM_CFLAGS = -I./ -Wall
AM_LDFLAGS = -L./ -lCmgUtl -lz

