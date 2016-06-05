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

#ifndef _FUSIONISO_H_
#define _FUSIONISO_H_

#include <fuse.h>
#include <glib.h>

#include "fusioniso_fs.h"
#include "fusioniso_isofs.h"

#define true 1
#define false 0

typedef enum sandboxing { sandbox_none, sandbox_home, sandbox_portable, sandbox_data } sandbox_type;

typedef struct _fusioniso_data {
	int maintain_mountpoint;
	int maintain_mtab;

	char *iocharset;                  /* specify iocharset for Joliet filesystem */
	char *mountpoint;                 /* mount point */
	int union_mounting;               /* if true, union-mount data over filesystem */
	sandbox_type sandbox_mounting;    /* options for application sandboxing */

	char *redirect_path;              /* redirect write operations from this path ... */
	int redirect_path_size;

	char *sandbox_path;               /* ... to this sandbox path */
	int sandbox_path_size;

	char *exclude_list[32];           /* exclude some paths in case of sandboxing */
	int exclude_list_size[32];
	int exclude_list_max;

	char *base_root;
	int base_root_size;

	int glibc;                        /* if true, change the loader on the fly */

	isofs_context *isofs;             /* iso image mounted */

	/* files hash tables  */
	GHashTable *lookup_table;
	GHashTable *removed_lookup_table;
	GHashTable *real_lookup_table;
	GHashTable *negative_lookup_table;
} fusioniso_data;


static inline fusioniso_data *fusioniso_get_data(void)
{
	return (fusioniso_data *) fuse_get_context()->private_data;
}

static inline isofs_context *isofs_get_context(void)
{
	return (isofs_context *) fusioniso_get_data()->isofs;
}

#endif /* _FUSIONISO_H_  */
