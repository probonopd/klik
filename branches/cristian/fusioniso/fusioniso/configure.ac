#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([fusioniso], [0.1], [klik.atekon.de@googlemail.com])
AM_INIT_AUTOMAKE([foreign dist-bzip2 1.10])
AC_CONFIG_SRCDIR([src/fusioniso.c])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
PKG_CHECK_MODULES(FUSE, fuse)
AC_SUBST(FUSE_LIBS)
AC_SUBST(FUSE_CFLAGS)

PKG_CHECK_MODULES(GLIB2, glib-2.0)
AC_SUBST(GLIB2_LIBS)
AC_SUBST(GLIB2_CFLAGS)

AC_CHECK_LIB([z], [uncompress],,
    [AC_MSG_ERROR([Can't find zlib, please install it])])

AC_MSG_CHECKING([if FUSE on this system is too new for us])
AC_EGREP_CPP([yes], [
#include "fuse.h"
#if FUSE_VERSION > 26
yes
#endif
], AC_DEFINE([FUSE_USE_VERSION], [26], [Version of FUSE interface]) AC_MSG_RESULT([yes]),
   AC_DEFINE([FUSE_USE_VERSION], [FUSE_VERSION], [Version of FUSE interface]) AC_MSG_RESULT([no]))

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h langinfo.h locale.h mntent.h stdlib.h string.h sys/param.h sys/statfs.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_GETMNTENT
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([atexit ftruncate getmntent memset mkdir nl_langinfo realpath rmdir setlocale strdup strerror strstr utime])

AC_ARG_ENABLE(
	[debug],
	AS_HELP_STRING([--enable-debug], [Enable debug functionalities]),
	[if test x$enableval != "xno"; then enable_debug=yes ; else enable_debug=no; fi],
	[enable_debug=no]
)
AM_CONDITIONAL([ENABLE_DEBUG], [test x$enable_debug = "xyes"])

AC_CONFIG_FILES([Makefile
		 linux/Makefile
                 man/Makefile
		 src/Makefile])
AC_OUTPUT
