/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2005, 2006 by Dmitry Morozhnikov <dmiceman AT mail.ru>
 *   Original version (http://ubiz.ru/dm/)
 *
 *   Copyright (C) 2005 by Renzo Davoli <renzo AT cs.unibo.it>
 *   Auto-endianess conversion
 *
 *   Copyright (C) 2006, 2007 by Lionel Tricon <lionel.tricon AT free.fr>
 *   Klik2 support - union-fuse + sandbox feature
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
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * -----------------------------------------------------------------------------
 */

/*
 * We keep inside this file all functions dedicated to read cramfs image
 *
 * FUNCTIONS
 *  cramfs_real_init
 *  cramfs_read_block
 *
 */

#include "fs.h"
#include "cramfs.h"

extern pthread_mutex_t fd_mutex;

extern GHashTable *lookup_table;
extern GHashTable *real_lookup_table;
extern GHashTable *removed_lookup_table;
extern GHashTable *negative_lookup_table;

extern cramfs_context context;
extern cramfs_inode_context *root_context;
extern int last_node;
extern int byteswap;

void *cramfs_real_init(char *imagefile)
{
    //     pthread_mutex_init(&fd_mutex, NULL);
    int fd = open(imagefile, O_RDONLY);
    if (fd == -1)
    {
        perror("open image file");
        exit(EINVAL);
    }
    context.imagefile = imagefile;
    context.fd = fd;
    ssize_t size = read(fd, & context.super, sizeof(context.super));
    if (size != sizeof(context.super))
    {
        fprintf(stderr, "only %d bytes read from superblock, %d required\n", size, sizeof(context.super));
        exit(EIO);
    }

    // filesystem check code directly copied from linux kernel
    // thanks to Linus Torvalds, original author!
    if (context.super.magic != CRAMFS_MAGIC &&
        context.super.magic != bswap_32(CRAMFS_MAGIC))
    {
        /* check at 512 byte offset */
        lseek(fd, 512, SEEK_SET);
        ssize_t size = read(fd, & context.super, sizeof(context.super));
        if (size != sizeof(context.super))
        {
            fprintf(stderr, "only %d bytes read from [possible] shifted superblock, %d required\n",
                size, sizeof(context.super));
            exit(EIO);
        }
        if (context.super.magic != CRAMFS_MAGIC &&
            bswap_32(context.super.magic) != CRAMFS_MAGIC)
        {
            fprintf(stderr, "wrong magic! is it really cramfs image file?\n");
            exit(EINVAL);
        }
    }

    if (bswap_32(context.super.magic) == CRAMFS_MAGIC)
    {
        if (*((char *)(&(context.super.magic))) != CRAMFS_MAGIC >> 24)
        {
            fprintf(stderr, "Swap endianess: reading LE file system on a BE machine\n");
            byteswap=LE2BE;
        }
        else
        {
            fprintf(stderr, "Swap endianess: reading BE file system on a LE machine\n");
            byteswap=BE2LE;
        }
    }

    /* get feature flags first */
    if (SW32(context.super.flags) & ~CRAMFS_SUPPORTED_FLAGS)
    {
        fprintf(stderr, "unsupported filesystem features, sorry :-(\n");
        exit(EINVAL);
    }

    /* Check that the root inode is in a sane state */
    if (!S_ISDIR(SW16(context.super.root.mode)))
    {
        fprintf(stderr, "init: root is not a directory\n");
        exit(EINVAL);
    }
    unsigned long root_offset = SWOFFSET(&(context.super.root)) << 2;
    if (!(SW32(context.super.flags) & CRAMFS_FLAG_FSID_VERSION_2))
    {
        // nothing to do with this?
    }
    if (root_offset == 0)
    {
        fprintf(stderr, "warning: empty filesystem\n");
    } else if (!(SW32(context.super.flags) & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
        ((root_offset != sizeof(struct cramfs_super)) &&
        (root_offset != 512 + sizeof(struct cramfs_super))))
    {
        fprintf(stderr, "init: bad root offset %lu\n", root_offset);
        exit(EINVAL);
    }

    lookup_table = g_hash_table_new(g_str_hash, g_str_equal);
    removed_lookup_table = g_hash_table_new(g_str_hash, g_str_equal);
    negative_lookup_table = g_hash_table_new(g_str_hash, g_str_equal);
    real_lookup_table = g_hash_table_new(g_str_hash, g_str_equal);

    root_context = (cramfs_inode_context *) malloc(sizeof(cramfs_inode_context));
    root_context->inode = & context.super.root;
    root_context->ino = 1;
    last_node = 2;
    g_hash_table_insert(lookup_table, "/", root_context);
    return &context;
}

int cramfs_read_block(off_t offset, size_t bsize, char *data, size_t *size)
{
    char ibuf[PAGE_CACHE_SIZE * 2];
    if (pthread_mutex_lock(& fd_mutex))
    {
        int err = errno;
        perror("read_block: can`l lock fd_mutex");
        return -err;
    }
    if (lseek(context.fd, offset, SEEK_SET) == -1)
    {
        perror("read_block: can`t lseek()");
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    }
    if (read(context.fd, ibuf, bsize) != bsize)
    {
        fprintf(stderr, "readdir: can`t read full block, errno %d, message %s\n",
            errno, strerror(errno));
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    }
    pthread_mutex_unlock(& fd_mutex);
    int rc = uncompress((unsigned char*)data, (uLongf *) size, (unsigned char*)ibuf, bsize);
    //     printf("read_block: offset %d, bsize %d, size %d, rc %d\n", (int) offset, bsize, *size, rc);
    return rc;
}
