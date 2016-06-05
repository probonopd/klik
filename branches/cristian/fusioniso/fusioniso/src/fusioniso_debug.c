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

#ifndef _DEBUG_C_
#define _DEBUG_C_

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "fusioniso_debug.h"


FILE *outfile=NULL;

int debug_outfile(const char *path)
{
	outfile=fopen(path,"w");
	if (!outfile) {
		fprintf(stderr,"%s: cannot open output file %s (%s)",
					PACKAGE_NAME,path,strerror(errno));
		return -1;
	} else {
		return 0;
	}
}



#endif /* _DEBUG_C_  */
