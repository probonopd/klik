/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2008 by Cristian Greco <cgreco at cs.unibo.it>
 *
 * -----------------------------------------------------------------------------
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * -----------------------------------------------------------------------------
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

extern FILE *outfile;
#define OUTFILE (outfile ? outfile : stderr)


#ifdef _DEBUG

/* for logging purposes (internal functions),
 * print to stderr or to file */
#define FSDEBUG(...) \
	do { \
		fprintf(OUTFILE,"[%d] %s:%d %s(): ",getpid(),__FILE__,__LINE__,__func__); \
		fprintf(OUTFILE,__VA_ARGS__); \
		fprintf(OUTFILE,"\n"); \
	} while (0);

/* for warning or error messages (you may want to exit()
 * after calling this macro), print only to stderr */
#define FSWARNING(...) \
	do { \
		fprintf(stderr,"%s %s(): ",PACKAGE_NAME,__func__); \
		fprintf(stderr,__VA_ARGS__); \
		fprintf(stderr,"\n"); \
	} while (0);

#else /* !_DEBUG */

#define FSDEBUG(...)
#define FSWARNING(...)

#endif


int debug_outfile(const char *path);

#endif /* _DEBUG_H_ */
