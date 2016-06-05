bin_PROGRAMS = fusioniso
fusioniso_SOURCES = fusioniso.c fusioniso-fs.c fusioniso-isofs.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
noinst_HEADERS = fusioniso-fs.h fusioniso-isofs.h

AM_CFLAGS = -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse glib-2.0` -Wall
AM_LDFLAGS = `pkg-config --libs fuse glib-2.0` -lz
