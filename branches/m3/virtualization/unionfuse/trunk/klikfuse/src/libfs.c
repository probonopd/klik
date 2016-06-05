#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <mntent.h>
#include <sys/param.h>
#include <pwd.h>
#include <glib.h>

#include <zlib.h>
#include <locale.h>
#include <langinfo.h>

#ifdef _ISOFS
#include <linux/iso_fs.h>
#include "fs.h"
#include "isofs.h"
typedef struct fuse_file_info struct_file_info;
isofs_context context;
#else
#include "fs.h"
#include "cramfs.h"
#endif

GHashTable *lookup_table;
char *iocharset = "UTF-8//IGNORE";

int img_init(const char*, const char*);
int img_readdir(const char*);
int img_read(const char*, char*, size_t, off_t);
guint local_g_strv_length(gchar **str_array);

int main(int argc, char*argv[])
{
    char v_data[2048];
    int retval, rc, i;

    rc = img_init("/home/lionel/Desktop/tuxpaint_1:0.9.17-1.cmg", "/usr/bin/tuxpaint");
    if (rc < 0) return 1;

    for (i=0;;i++)
    {
        retval = img_read("/usr/bin/tuxpaint", (char*)v_data, 2048, i*2048);
        printf("retval=%d\n", retval);
        if (retval != 2048) break;
    }

    close(rc);
    return 0;
}

int img_init(const char *imagefile, const char *searchfile)
{
    int rc, i;
    int image_fd = -1;
    gchar **parts;

    // open the image file
    image_fd = open(imagefile, O_RDONLY);
    if (image_fd == -1)
    {
        fprintf(stderr, "Can`t open image file: [%s]\n", imagefile);
        perror("Can`t open image file");
        return -1;
    }

    // will exit in case of failure
    rc = isofs_real_preinit((char*)imagefile, image_fd);
    if (rc < -1) return -1;
    isofs_real_init();

    // analyze the path
    parts = g_strsplit(searchfile, "/", -1);
    guint parts_len = local_g_strv_length(parts);
    gchar *rpath = "";
    img_readdir("/");
    for (i=1; i<(int)parts_len-1; i++)
    {
        rpath = g_strconcat(rpath, "/", parts[i], NULL);
        img_readdir((char*)rpath);
    }
    return image_fd;
}

int img_readdir(const char *path)
{
    char *buf = NULL;
    if (path[0] == '\0')
    {
        fprintf(stderr,"readdir: attempt to read empty path name\n");
        return -EINVAL;
    }

#ifdef _ISOFS
    isofs_inode *current_inode = g_hash_table_lookup(lookup_table, path);
#else
    struct cramfs_inode *current_inode = NULL;
    struct cramfs_inode *inode2 = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    cramfs_inode_context *ncontext2;
    if (ncontext) current_inode=ncontext->inode;
#endif

#ifdef _ISOFS
    struct iso_directory_record *current = current_inode->record;
    if (!ISO_FLAGS_DIR(current->flags))
    {
        fprintf(stderr, "readdir: %s not a dir\n", path);
        return -ENOTDIR;
    }

    size_t current_size = isonum_733(current->size);

    int block = isonum_733(current->extent);
    buf = (char *) malloc(context.data_size);
    if (!buf)
    {
        perror("Can`t malloc: ");
        return -ENOMEM;
    }
    int rc;
    //printf("path %s, current_size %d, block %d\n", path, current_size, block);

    size_t total_size = 0;
    int count = 1;
    int block_count = 0;
    off_t boff = 0;

    while (total_size <= current_size - sizeof(struct iso_directory_record))
    {
        rc = isofs_read_raw_block(block, buf);
        if (rc < 0)
        {
            return rc;
        }
        if (rc != context.data_size)
        {
            // can`t be allowed
            fprintf(stderr, "readdir: can`t read whole block, read only %d bytes, block %d\n", rc, block);
            free(buf);
            return -EIO;
        }
        block_count++;

        if (boff > 0)
        {
            total_size += (context.data_size - boff);
            boff = 0;
        }

        while (boff + sizeof(struct iso_directory_record) <= context.data_size &&
            total_size <= current_size - sizeof(struct iso_directory_record))
        {

            struct iso_directory_record *record = (struct iso_directory_record *) (buf + boff);
            size_t record_length = isonum_711((unsigned char *) record->length);
            size_t name_len = isonum_711(record->name_len);
            size_t pad_len = ((name_len & 1) ? 0 : 1);            // padding byte if name_len is even
            size_t sa_len = record_length - name_len - sizeof(struct iso_directory_record) - pad_len;

            // printf("block %d, boff %d, total_size %d, current_size %d, record length %d, name_len %d, pad_len %d, sa_len %d\n",
            //     block, (int) boff, total_size, current_size, record_length, name_len, pad_len, sa_len);
            if (record_length == 0)
            {
                // possible end of directory or end of block
                total_size += (context.data_size - boff);
                boff = 0;
                break;
            }
            if (record_length < sizeof(struct iso_directory_record))
            {
                if (count > 2)                                     // check if . and .. is already read
                {
                    // possible end of directory
                    // at least mkisofs does not set iso_directory_record.size correct
                    // (this is much possible it was my fault and misunderstanding -- dmiceman)
                    // but we need to try next block to be sure
                    /// TODO this is not clear: what to do if next block not contain next directory?
                    total_size += (context.data_size - boff);
                    boff = 0;
                    break;
                }
                else
                {
                    fprintf(stderr, "readdir: directory record length too small: %d\n", record_length);
                    free(buf);
                    return -EIO;
                }
            }
            if (name_len > NAME_MAX - 1)
            {
                fprintf(stderr, "readdir: file name length too big: %d\n", name_len);
                free(buf);
                return -EIO;
            }
            if (sa_len < 0)
            {
                // probably something wrong with name_len
                fprintf(stderr, "readdir: wrong name_len in directory entry: %d, record_length %d\n",
                    name_len, record_length);
                free(buf);
                return -EIO;
            }
            if (count > 2 && name_len == 1 && record->name[0] == 0)
            {
                // looks like this is normal situation to meet another directory because
                // there is no clear way to find directory end
                //                 fprintf(stderr, "readdir: next directory found while processing another directory! boff %d, total_size %d, current_size %d, block %d, count %d\n",
                //                     (int) boff, total_size, current_size, block, count);
                goto out;
            }

            isofs_inode *inode = (isofs_inode *) malloc(sizeof(isofs_inode));
            if (inode == NULL)
            {
                perror("Can`t malloc: ");
                return -ENOMEM;
            }
            memset(inode, 0, sizeof(isofs_inode));
            inode->st_ino = context.last_ino;
            context.last_ino++;

            struct iso_directory_record *n_rec =
                (struct iso_directory_record *) malloc (sizeof(struct iso_directory_record));
            if (!n_rec)
            {
                perror("Can`t malloc: ");
                return -ENOMEM;
            }
            memcpy(n_rec, record, sizeof(struct iso_directory_record));
            inode->record = n_rec;

            if (context.susp || path[1] == '\0')                   // if susp is known to be present or this is a root dir ("/")
            {
                /*                printf("sa offset %d, sa_len %d\n",
                                                                        sizeof(struct iso_directory_record) + name_len + pad_len, sa_len);*/
                rc = isofs_parse_sa(inode,
                    ((char *) record) + sizeof(struct iso_directory_record) + name_len + pad_len + context.susp_skip,
                    sa_len);
                if (rc)
                {
                    free(buf);
                    isofs_free_inode(inode);
                    return rc;
                }
            }

            char *entry = (char *) malloc(NAME_MAX);
            if (!entry)
            {
                perror("Can`t malloc: ");
                return -ENOMEM;
            }
            if (count == 1)                                        // . entry ('\0' on disk)
            {
                strcpy(entry, ".");
            }                                                     // .. entry ('\1' on disk)
            else if (count == 2)
            {
                strcpy(entry, "..");
            }                                                     // regular entry
            else
            {
                if (inode->NM)                                     // rrip NM entry present and in effect
                {
                    //                     printf("readdir: NM entry is in effect\n");
                    strncpy(entry, inode->nm, inode->nm_len);
                    entry[inode->nm_len] = '\0';
                }                                                 // regular ISO9660 filename
                else
                {
                    //                     printf("readdir: no NM entry found, name len %d\n", name_len);
                    // because there can be '\0' characters because using of UCS-2 encoding we need to use memcpy
                    memcpy(entry, (char *) record->name, name_len);
                    entry[name_len] = '\0';

                    // fixup entry -- lowercase, strip leading ';', etc..
                    entry = isofs_fix_entry(entry, name_len);
                    if (!entry)
                    {
                        fprintf(stderr, "readdir: error during entry fixup\n");
                        isofs_free_inode(inode);
                        free(buf);
                        return -EIO;
                    }
                }
            }

            //fprintf(stderr, "%d -- %s\n\n", count, entry);

            /*if (filler)
            {
                struct stat st;
                memset(& st, '\0', sizeof(struct stat));
                isofs_direntry2stat(& st, inode);
                rc = filler(filler_buf, entry, &st, 0);
                if (rc)
                {
                    // printf("readdir: filler return with %d, entry %s\n", rc, entry);
                    isofs_free_inode(inode);
                    free(buf);
                    free(entry);
                    return -rc;
                }
            }*/

            char absolute_entry[PATH_MAX];
            strcpy(absolute_entry, path);
            if (path[1] != '\0')                                   // not root dir
            {
                strcat(absolute_entry, "/");
            }
            strcat(absolute_entry, entry);
            if (g_hash_table_lookup(lookup_table, absolute_entry))
            {
                // already in lookup cache
                isofs_free_inode(inode);
            }
            else
            {
                g_hash_table_insert(lookup_table, g_strdup(absolute_entry), inode);
            }

            free(entry);

            boff += record_length;
            total_size += record_length;
            count++;
        }

        // read next block

        block++;
    }

#else
    if (!S_ISDIR(SW16(current_inode->mode)))
    {
        fprintf(stderr, "readdir: %s not a dir\n", path);
        return -ENOTDIR;
    }
    if (filler)
    {
        filler(filler_buf, ".", NULL, 0);
        filler(filler_buf, "..", NULL, 0);
    }
    //     printf("%s, offset %d, size %d\n", path, current->offset * 4, current->size);
    off_t ioff = SWOFFSET(ncontext->inode) * 4;
    unsigned char *ibuf = (unsigned char *) malloc(sizeof(struct cramfs_inode) + CRAMFS_MAXPATHLEN + 1);
    size_t dsize = SWSIZE(ncontext->inode->size);
    int count = 0;
    while (dsize > 0)
    {
        if (pthread_mutex_lock(& fd_mutex))
        {
            int err = errno;
            //pthread_mutex_unlock(& main_mutex);
            perror("readdir: can`l lock fd_mutex");
            return -err;
        }
        if (lseek(context.fd, ioff, SEEK_SET) == -1)
        {
            perror("readdir: can`t lseek()");
            pthread_mutex_unlock(& fd_mutex);
            return -EIO;
        }
        if (read(context.fd, ibuf, sizeof(struct cramfs_inode)) != sizeof(struct cramfs_inode))
        {
            fprintf(stderr, "readdir: can`t read full inode, errno %d, message %s\n",
                errno, strerror(errno));
            pthread_mutex_unlock(& fd_mutex);
            return -EIO;
        }
        char *entry = (char *) malloc(CRAMFS_MAXPATHLEN + 1);
        memset(entry, 0, CRAMFS_MAXPATHLEN + 1);
        struct cramfs_inode *inode = (struct cramfs_inode *) malloc(sizeof(struct cramfs_inode));
        memcpy(inode, ibuf, sizeof(struct cramfs_inode));
        int namelen = SWNAMELEN(inode);
        if (namelen * 4 > CRAMFS_MAXPATHLEN)
        {
            pthread_mutex_unlock(& fd_mutex);
            fprintf(stderr, "readdir: too long filename found! namelen %d\n",
                namelen);
            return -EIO;
        }
        if (read(context.fd, entry, namelen * 4) != namelen * 4)
        {
            fprintf(stderr, "readdir: can`t read full direntry, errno %d, message %s\n",
                errno, strerror(errno));
            pthread_mutex_unlock(& fd_mutex);
            return -EIO;
        }
        pthread_mutex_unlock(& fd_mutex);
        entry[namelen * 4] = '\0';
        //        printf("%s: size %d, mode %d namelen %d, offset %d\n",
        //           entry, SWSIZE(inode->size), SW16(inode->mode), namelen * 4, SWOFFSET(inode));
        int esize = sizeof(struct cramfs_inode) + namelen * 4;
        ioff += esize;
        dsize -= esize;
        count++;
        if (entry[0] == '\0')
        {
            fprintf(stderr, "readdir: empty size name found! namelen %d, dsize %d, ioff %d\n",
                namelen, dsize, (int) ioff);
            return 0;
        }
        if (filler)
        {
            int filler_rc = filler(filler_buf, entry, NULL, 0);
            if (filler_rc)
            {
                return filler_rc;
            }
        }
        char absolute_entry[PATH_MAX];
        strcpy(absolute_entry, path);
        if (path[1] != '\0')                                       // not root dir
        {
            strcat(absolute_entry, "/");
        }
        strcat(absolute_entry, entry);
        if (g_hash_table_lookup(lookup_table, absolute_entry))
        {
            // already in lookup cache
            free(inode);
        }
        else
        {
            cramfs_inode_context *node_context = (cramfs_inode_context *) malloc(sizeof(cramfs_inode_context));
            node_context->ino = last_node;
            last_node++;
            node_context->inode = inode;
            g_hash_table_insert(lookup_table, g_strdup(absolute_entry), node_context);
        }
        free(entry);
    }
    free(ibuf);
    //     printf("readdir: exiting from %s, count %d\n", path, count);
#endif
out:
    if (buf != NULL) free(buf);
    return 0;
}

int img_read(const char *path, char *out_buf, size_t size, off_t offset)
{
#ifdef _ISOFS
    isofs_inode *inode = g_hash_table_lookup(lookup_table, path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode == NULL)
    {
        fprintf(stderr, "read: know nothing about %s\n", path);
        return -ENOENT;
    }

#ifdef _ISOFS
    struct iso_directory_record *record = inode->record;
    if (ISO_FLAGS_DIR(record->flags))
    {
        fprintf(stderr, "read: %s not a file\n", path);
        return -EINVAL;
    }
    if (ISO_FLAGS_HIDDEN(record->flags))
    {
        fprintf(stderr, "read: %s is a hidden file\n", path);
        // return -EPERM;
    }

    if (inode->ZF)                                                 // this file is compressed, handle it specially
    {
        return isofs_real_read_zf(inode, out_buf, size, offset);
    }

    size_t fsize = isonum_733(record->size);
    if (offset + size > fsize)
    {
        size = fsize - offset;
    }
    if (size < 1)
    {
        return 0;
    }

    int start_block = isonum_733(record->extent);
    if (start_block == 0)                                          // empty file
    {
        return 0;
    }

    int start = offset / context.data_size;
    int end = (offset + size) / context.data_size;
    int shift = offset % context.data_size;
    int block = start_block + start;
    //     printf("read: path %s, size %d, offset %d, fsize %d, start %d, end %d, shift %d\n",
    //         path, size, (int) offset, fsize, start, end, shift);

    char *buf = (char *) malloc(context.data_size);
    if (!buf)
    {
        perror("Can`t malloc: ");
        return -ENOMEM;
    }

    int i;
    size_t total_size = 0;
    size_t size_left = size;
    int count = 0;

    for (i=start; i<=end; i++)
    {
        int len = isofs_read_raw_block(block, buf);
        if (len < 0)
        {
            free(buf);
            return len;
        }
        //         printf("read: block %d, len %d, size_left %d\n", block, len, size_left);

        if (len > size_left)
        {
            len = size_left;
        }

        total_size += len;

        memcpy(out_buf + count * context.data_size, buf + shift, len - shift);

        count++;
        shift = 0;
        size_left -= len;
        block++;
    }

    free(buf);
    return total_size;
#else
    if (!S_ISREG(SW16(inode->mode)))
    {
        fprintf(stderr, "read: %s not a file\n", path);
        return -EINVAL;
    }
    size_t fsize = SWSIZE(inode->size);
    int start = offset / PAGE_CACHE_SIZE;
    int end = (offset + size) / PAGE_CACHE_SIZE;
    char *obuf = (char *) malloc((end - start + 1) * PAGE_CACHE_SIZE);
    int nblocks = (fsize - 1) / PAGE_CACHE_SIZE + 1;
    int *bbuf = (int *) malloc(nblocks * 4);
    if (pthread_mutex_lock(& fd_mutex))
    {
        int err = errno;
        perror("read: can`l lock fd_mutex");
        return -err;
    }
    if (lseek(context.fd, SWOFFSET(inode) * 4, SEEK_SET) == -1)
    {
        perror("read_block: can`t lseek()");
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    }
    if (read(context.fd, bbuf, nblocks * 4) != nblocks * 4)
    {
        fprintf(stderr, "read: can`t read full block table, errno %d, message %s\n",
            errno, strerror(errno));
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    }
    pthread_mutex_unlock(& fd_mutex);
    int i;
    off_t foffset = SWOFFSET(inode) * 4 + nblocks * 4;
    off_t ooff = 0;
    size_t osize;
    size_t real_size = 0;
    //     printf("read: reading blocks from %d to %d, shift %d, fsize %d, nblocks %d\n",
    //         start, end, (int) (offset % PAGE_CACHE_SIZE), fsize, nblocks);
    for (i=start; i<end; i++)
    {
        int block = SW32(bbuf[i]);
        int boffset = (i ? SW32(bbuf[i - 1]) : (int) foffset);
        size_t bsize = block - boffset;
        if (bsize > PAGE_CACHE_SIZE * 2)
        {
            free(bbuf);
            free(obuf);
            fprintf(stderr, "read: block size bigger than PAGE_CACHE_SIZE * 2 while reading block %i from %s, bsize %i, foffset %i, block %i\n",
                i, path, bsize, (int) foffset, block);
            return -EIO;
        }
        osize = PAGE_CACHE_SIZE;
        if (!bsize)
        {
            // hole
        }
        else
        {
            int rc = cramfs_read_block(boffset, bsize, obuf + ooff, &osize);
            if (rc != Z_OK)
            {
                free(bbuf);
                free(obuf);
                fprintf(stderr, "read: read block error %i: %s\n", rc, strerror(rc));
                return -rc;
            }
        }
        ooff += osize;
        real_size += osize;
    }
    if (real_size < size)
    {
        size = real_size;
    }
    memcpy(out_buf, obuf + (offset % PAGE_CACHE_SIZE), size);
    //     fwrite(out_buf, size, 1, stdout);
    free(bbuf);
    free(obuf);
    return size;
#endif
}

/*
 * locally implement g_strv_length, this is missing in glib2 for rhel3/rhel4
 * -- Chandan Dutta Chowdhury 2007-07-06
 */
guint local_g_strv_length(gchar **str_array)
{
    guint i = 0;
    g_return_val_if_fail(str_array != NULL, 0);
    while (str_array[i])
    {
        ++i;
    }
    return i;
}

