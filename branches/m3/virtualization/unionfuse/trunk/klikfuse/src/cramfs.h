/*
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
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef _CRAMFS_H
#define _CRAMFS_H

#define NOSWAP 0
#define BE2LE 1
#define LE2BE 2

#include <sys/stat.h>
#include <linux/rock.h>
#include <utime.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <zlib.h>
#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/statfs.h>
#include <iconv.h>
#include <errno.h>
#include <byteswap.h>

#include <linux/cramfs_fs.h>

#define SW32(X) ((byteswap)?bswap_32(X):(X))
#define SW16(X) ((byteswap)?bswap_16(X):(X))
#define SWSIZE(X) ((byteswap)?(bswap_32(X<<8)):(X))
#define SWNAMELEN(PX) ((byteswap)? \
((byteswap==BE2LE)?(bswap_32(((u32*)(PX))[2]) >> 26):(bswap_32(((u32*)(PX))[2]) & 0x3F))\
:(PX)->namelen)
#define SWOFFSET(PX) ((byteswap)? \
((byteswap==BE2LE)?(bswap_32(((u32*)(PX))[2] & 0xffffff03)):(bswap_32(((u32*)(PX))[2]) >> 6))\
:(PX)->offset)

typedef struct _cramfs_context
{
    char *imagefile;
    int fd;
    struct cramfs_super super;
} cramfs_context;

typedef struct _cramfs_inode_context
{
    int ino;
    struct cramfs_inode *inode;
} cramfs_inode_context;

void *cramfs_real_init(char*);
int cramfs_read_block(off_t, size_t, char* , size_t*);

#endif	/* _CRAMFS_H */
