#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([klikd], [0.1], [razielmine@mail.com])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/klikd.cpp])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h sys/inotify.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
#AC_TYPE_INT32_T
AC_TYPE_SIZE_T
#AC_TYPE_SSIZE_T
#AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_STAT
AC_CHECK_FUNCS([atexit])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT
