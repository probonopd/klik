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
 *   Copyright (C) 2007 by Chandan Dutta Chowdhury
 *   Locally implement g_strv_length, this is missing in glib2 for rhel3/rhel4
 *
 *   Copyright (C) 2008 by Jason Taylor <killerkiwi2005@gmail.com>
 *   Klik2 support - add the support of the -d option
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
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * -----------------------------------------------------------------------------
 */

#define TRACE 0

/*
 * Common functions between isofs and cramfs or specific to the klik2 runtime
 *
 * existing (but modified since) functions :
 *  local_g_strv_length
 *  fs_lookup
 *  fs_real_opendir
 *  fs_real_readdir
 *  fs_real_getattr
 *  fs_real_readlink
 *  fs_real_open
 *  fs_real_read
 *  fs_real_statfs
 *
 * New functions (klik2) :
 *  fs_path_mkdir
 *  fs_file_dup
 *  fs_home
 *  fs_home_stat
 *  fs_real_write
 *  fs_real_access
 *  fs_real_mknod
 *  fs_real_unlink
 *  fs_real_rmdir
 *  fs_real_mkdir
 *  fs_real_symlink
 *  fs_real_rename
 *  fs_real_link
 *  fs_real_chmod
 *  fs_real_chown
 *  fs_real_truncate
 *  fs_real_utime
 *  fs_real_setxattr
 *  fs_real_getxattr
 *  fs_real_listxattr
 *  fs_real_removexattr
 *
 */

// for struct tm->tm_gmtoff
#define _BSD_SOURCE

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

#include "fs.h"

#ifdef _ISOFS
#include "isofs.h"
#else
#include <byteswap.h>
#include "cramfs.h"
#endif

extern bool _union_mounting;
extern bool _sandbox_mounting;
extern char* _redirect_path;
extern int   _size_redirect_path;
extern char* _sandbox_path;
extern int   _size_sandbox_path;
extern char* _exclude_list[];
extern int   _exclude_length[];
extern int   _list_max;

#ifdef _ISOFS
isofs_context context;
#else
pthread_mutex_t fd_mutex = PTHREAD_MUTEX_INITIALIZER;
cramfs_inode_context *root_context;
cramfs_context context;
int last_node;
int byteswap;
#endif

GHashTable *lookup_table;
GHashTable *removed_lookup_table;
GHashTable *real_lookup_table;
GHashTable *negative_lookup_table;
GList *pid_lookup_table = NULL;
static char *negative_value = "not exists";
static char *removed_value = "removed";

pid_t getsid(pid_t pid);

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

/*
 * Creation of a path when we deal with absolute path
 *
 */
void fs_path_mkdir(const char *p_path, int p_value)
{
    char v_path[FS_MAXPATH];
    char source[FS_MAXPATH];
    char dest[FS_MAXPATH];
    struct stat v_stbuf;
    gchar **parts;
    int retval,i;
    if (TRACE) printf("****** fs_path_mkdir %s\n", p_path);

    strcpy(v_path, p_path);
    retval = stat(v_path, &v_stbuf);
    if (!retval && !S_ISDIR(v_stbuf.st_mode))
    {
        retval = stat(dirname((char*)v_path), &v_stbuf);
        if (!retval && S_ISDIR(v_stbuf.st_mode)) return;
    }

    if (TRACE) printf("****** g_strsplit %s -> %s\n", v_path, &(v_path[_size_sandbox_path]));
    parts = g_strsplit(&(v_path[_size_sandbox_path]), "/", -1);
    strcpy(source, _redirect_path);
    strcpy(dest, _sandbox_path);
    for (i=0; i<local_g_strv_length(parts)-p_value; i++)
    {
        strcat(source, "/");
        strcat(source, parts[i]);
        strcat(dest, "/");
        strcat(dest, parts[i]);
        retval = stat(source, &v_stbuf);
        if (TRACE) printf("****** ISDIR PATH_MKDIR: %s\n", source);
        if (!retval && !S_ISDIR(v_stbuf.st_mode)) break;
        if (TRACE) printf("****** PATH_MKDIR: %s\n", dest);
        mkdir(dest, v_stbuf.st_mode);
    }
    strcpy((char*)p_path, v_path);
}


/*
 * Inside the jail, we need to duplicate a file before modified it
 *
 */
void fs_file_dup(const char *p_path, const char *p_newpath)
{
    char buffer[FS_MAXPATH];
    FILE *file_in, *file_out;
    struct stat v_stbuf;
    int retval, number;
    if (TRACE) printf("****** fs_file_dup %s -> %s\n", p_path, p_newpath);

    retval = stat(p_path, &v_stbuf);
    if (!retval && S_ISDIR(v_stbuf.st_mode))
    {
        fs_path_mkdir(p_newpath, 0);
        return;
    }

    fs_path_mkdir(p_newpath, 1);
    file_in = fopen(p_path, "r");
    if (file_in == NULL) return;
    file_out = fopen(p_newpath, "w");
    if (file_out == NULL)
    {
        fclose(file_in);
        return;
    }
    while (1)
    {
        number = fread(buffer, 1, FS_MAXPATH, file_in);
        fwrite(buffer, 1, number, file_out);
        if (feof(file_in)) break;
    }
    fclose(file_out);
    fclose(file_in);
}


/*
 * Check if we are located inside the $HOME directory or outside
 *
 * RETURN
 *  1: located outside $HOME
 *  0: located into $HOME
 *
 */
int fs_home(const char *p_path, char **p_newpath)
{
    int i, v_size = strlen(p_path);

    if (!_sandbox_mounting || v_size<_size_redirect_path)
    {
        *p_newpath = (char*)p_path;
        return 1;
    }

    // We try to find if we need direct access to a file
    for (i=0; i<_list_max; i++)
    {
        if (_exclude_length[i]>v_size ||
            p_path[_exclude_length[i]-1]!=(_exclude_list[i])[_exclude_length[i]-1] ||
            strncmp(_exclude_list[i],p_path,_exclude_length[i])!=0) continue;
        if (_exclude_length[i]==v_size || p_path[_exclude_length[i]]=='/')
        {
            *p_newpath = (char*)p_path;
            return 1;
        }
    }

    // -B support
    if (_size_redirect_path==1 &&
        p_path[0]=='/')
    {
        strcpy(*p_newpath, _sandbox_path);
        strcat(*p_newpath, p_path);
        return 0;
    }

    // -d support
    if (p_path[_size_redirect_path-2]=='/' &&
        p_path[_size_redirect_path-1]=='.' &&
        !strncmp(p_path,_redirect_path,_size_redirect_path))
    {
        strcpy(*p_newpath, _sandbox_path);
        strcat(*p_newpath, &(p_path[_size_redirect_path]));
        return 0;
    }

    // -b support
    if ((v_size==_size_redirect_path || p_path[_size_redirect_path]=='/') &&
        !strncmp(p_path,_redirect_path,_size_redirect_path))
    {
        strcpy(*p_newpath, _sandbox_path);
        strcat(*p_newpath, &(p_path[_size_redirect_path]));
        return 0;
    }
    
    *p_newpath = (char*)p_path;
    return 1;
}


/*
 * Check if we are located inside or outside $HOME
 * In case the path is located inside $HOME, we check if
 * the path exist on the disk or not
 *
 * RETURN
 *   1: located outside $HOME
 *   0: p_newpath exist on the disk
 *  -1: p_newpath is not found on the disk
 *
 */
int fs_home_stat(const char *p_path, char **p_newpath, struct stat *p_stbuf)
{
    if (fs_home(p_path,p_newpath) == 0) return lstat(*p_newpath,p_stbuf);
    return 1;
}


/*
 * Important function
 * Check if the path is found inside the cmg file
 *
 */
#ifdef _ISOFS
isofs_inode* fs_lookup(const char *p_path)
{
    if (p_path[0] == '\0') return NULL;
    isofs_inode* inode = g_hash_table_lookup(lookup_table, p_path);
#else
cramfs_inode_context* fs_lookup(const char *p_path)
{
    if (p_path[0] == '\0') return NULL;
    cramfs_inode_context* inode = g_hash_table_lookup(lookup_table, p_path);
#endif
    if (inode != NULL) { return inode; }
    if (g_hash_table_lookup(negative_lookup_table, p_path)) return NULL;
    if (_union_mounting && g_hash_table_lookup(real_lookup_table, p_path)) return NULL;
    //printf("start search for [%s]\n", p_path);
    gchar **parts = g_strsplit(p_path, "/", -1);
    guint parts_len = local_g_strv_length(parts);
    int partno = 1;
    gchar *rpath = g_strdup("/");
    gchar *rpath1 = "";
    gchar *part = parts[partno];
    while (part && partno < parts_len)
    {
        rpath1 = g_strconcat(rpath1, "/", part, NULL);
        //printf("looking for [%s] in [%s]...\n", rpath1, rpath);
        if (_union_mounting && g_hash_table_lookup(real_lookup_table, rpath1)) return NULL;
        inode = g_hash_table_lookup(lookup_table, rpath1);
        if (inode == NULL)
        {
            //          printf("trying to load %s...\n", rpath);
            int rc = fs_real_readdir(0, rpath, NULL, NULL);
            if (!_union_mounting && rc)
            {
                fprintf(stderr, "lookup: error %d from readdir: %s\n", rc, strerror(-rc));
                g_strfreev(parts);
                g_free(rpath);
                return NULL;
            }
        }
        partno++;
        part = parts[partno];
        g_free(rpath);
        rpath = rpath1;
    }
    g_strfreev(parts);
    g_free(rpath);
    inode = g_hash_table_lookup(lookup_table, p_path);
    if (inode == NULL)
    {
        struct stat v_stbuf;
        if (_union_mounting && stat(p_path,&v_stbuf)!=0)
        {
            g_hash_table_insert(negative_lookup_table, g_strdup(p_path), negative_value);
            return NULL;
        }
        if (!_union_mounting) g_hash_table_insert(negative_lookup_table, g_strdup(p_path), negative_value);
        return NULL;
    }
    return inode;
}

int fs_real_opendir(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** OPENDIR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode==NULL)
    {
        struct stat v_stbuf;

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISDIR(v_stbuf.st_mode)) return -ENOENT;
            return 0;
        }

        if (_union_mounting && stat(path,&v_stbuf)==0)
        {
            if (!S_ISDIR(v_stbuf.st_mode)) return -ENOENT;
            return 0;
        }

        fprintf(stderr, "opendir: know nothing about %s\n", path);
        return -ENOENT;
    }

#ifdef _ISOFS
    if (!ISO_FLAGS_DIR(inode->record->flags))
    {
        fprintf(stderr, "opendir: %s not a dir\n", path);
        return -ENOTDIR;
    }
#else
    if (!S_ISDIR(SW16(inode->mode)))
    {
        fprintf(stderr, "opendir: %s not a dir\n", path);
        return -ENOTDIR;
    }
#endif

    return 0;
}

int fs_real_readdir(int real, const char *path, void *filler_buf, fs_dir_fill_t filler)
{
    char ppath[FS_MAXPATH];
    char v_chemin[2048];
    char *newpath=ppath;
    struct dirent *v_dirent;
    struct stat v_stbuf;
    int retval1, retval2;
    DIR *v_dir;
    char *buf = NULL;
    if (path[0] == '\0')
    {
        fprintf(stderr,"readdir: attempt to read empty path name\n");
        return -EINVAL;
    }

    if (TRACE) printf("****** READDIR: %s (%d)\n", path, real);
#ifdef _ISOFS
    isofs_inode *current_inode = fs_lookup(path);
    isofs_inode *inode2 = NULL;
#else
    struct cramfs_inode *current_inode = NULL;
    struct cramfs_inode *inode2 = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    cramfs_inode_context *ncontext2;
    if (ncontext) current_inode=ncontext->inode;
#endif

    if (!_union_mounting && current_inode==NULL)
    {
        fprintf(stderr, "readdir: know nothing about %s\n", path);
        return -ENOENT;
    }
    retval2 = fs_home(path,&newpath);
    if (current_inode == NULL)
    {
        if (real && retval2!=1) goto out;
        if (!real && retval2!=1)
        {
            g_hash_table_insert(real_lookup_table, g_strdup(path), "real");
            return 0;
        }
        if (_union_mounting)
        {
            retval1 = stat(path, &v_stbuf);
            if (real && retval1==0) goto out;
            if (!real && retval1==0)
            {
                g_hash_table_insert(real_lookup_table, g_strdup(path), "real");
                return 0;
            }
        }
        if (!real) fprintf(stderr, "readdir: know nothing about %s\n", path);
        return -ENOENT;
    }

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
    // printf("path %s, current_size %d, block %d\n", path, current_size, block);

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

            // fprintf(stderr, "%d -- %s\n\n", count, entry);

            if (filler)
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
            }

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
    if (retval2!=1 && real)
    {
        v_dir = opendir(newpath);
        if (v_dir)
        {
            for (;;)
            {
                v_dirent = readdir(v_dir);
                if (v_dirent == NULL) break;
                v_chemin[0] = '\0';
                if (!strcmp(newpath,"/")) sprintf(v_chemin, "/%s", v_dirent->d_name);
                else sprintf(v_chemin, "%s/%s", newpath, v_dirent->d_name);
#ifdef _ISOFS
                inode2 = fs_lookup(v_chemin);
#else
                inode2 = NULL;
                ncontext2 = fs_lookup(v_chemin);
                if (ncontext2) inode2=ncontext2->inode;
#endif
                if (inode2 == NULL)
                {
                    memset(&v_stbuf, '\0', sizeof(struct stat));
                    if (filler && !lstat(v_chemin,&v_stbuf))
                    {
                        filler(filler_buf, v_dirent->d_name, &v_stbuf, 0);
                        if (TRACE) printf("FILLER [%s]\n", v_dirent->d_name);
                    }
                }
            }
            closedir(v_dir);
        }
    }

    if (real && _union_mounting)
    {
        // Read the real path
        v_dir = opendir(path);
        if (v_dir)
        {
            for (;;)
            {
                retval1 = 1;
                v_dirent = readdir(v_dir);
                if (v_dirent == NULL) break;
                if (retval2 != 1)
                {
                    v_chemin[0] = '\0';
                    if (!strcmp(newpath,"/")) sprintf(v_chemin, "/%s", v_dirent->d_name);
                    else sprintf(v_chemin, "%s/%s", newpath, v_dirent->d_name);
                    retval1 = stat(v_chemin, &v_stbuf);
                }
                v_chemin[0] = '\0';
                if (!strcmp(path,"/")) sprintf(v_chemin, "/%s", v_dirent->d_name);
                else sprintf(v_chemin, "%s/%s", path, v_dirent->d_name);
#ifdef _ISOFS
                inode2 = fs_lookup(v_chemin);
#else
                inode2 = NULL;
                ncontext2 = fs_lookup(v_chemin);
                if (ncontext2) inode2=ncontext2->inode;
#endif
                if (inode2==NULL && retval1!=0)
                {
                    memset(&v_stbuf, '\0', sizeof(struct stat));
                    if (filler && !lstat(v_chemin,&v_stbuf))
                    {
                        filler(filler_buf, v_dirent->d_name, &v_stbuf, 0);
                        if (TRACE) printf("FILLER [%s]\n", v_dirent->d_name);
                    }
                }
            }
            closedir(v_dir);
        }
    }

    if (buf != NULL) free(buf);
    return 0;
}

int fs_real_getattr(const char *path, struct stat *stbuf)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** GETATTR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode == NULL)
    {
        // We forbid access to the /tmp/klik directory
        if (_union_mounting && strncmp(path,"/tmp/klik",9) == 0) return -ENOENT;

        // we maintains a list of apprun pids
        if (_union_mounting && strcmp(path,"/pid")==0)
        {
            GList* v_item;
            int v_size = 0;
            memset(stbuf, 0, sizeof(struct stat));
            stbuf->st_ino = 0;
            stbuf->st_mode = S_IFREG|S_IRUSR|S_IWUSR;
            stbuf->st_uid = getuid();
            stbuf->st_gid = getgid();
            v_item = g_list_first(pid_lookup_table);
            while (v_item != NULL)
            {
                v_size += strlen(v_item->data);
                v_item = g_list_next(v_item);
            }
            stbuf->st_size = v_size;
            stbuf->st_blksize = PAGE_CACHE_SIZE;
            stbuf->st_blocks = (stbuf->st_size  - 1) / PAGE_CACHE_SIZE + 1;
            stbuf->st_nlink = 1;
            return 0;
        }

        if (fs_home_stat(path,&newpath,stbuf) == 0) return 0;
        if (_union_mounting && lstat(path,stbuf)==0)
        {
            // If a file was previously removed, we need to take care
            if (g_hash_table_lookup(removed_lookup_table, path))
            {
                if (TRACE) printf("****** LOOKUP removed_lookup_table -> %s\n", path);
                return -ENOENT;
            }
            return 0;
        }
        // this error occur too often when browsing mounted tree with konqueror --
        // it check existence of .directory file with stat()
        // fprintf(stderr, "getattr: know nothing about %s\n", path);
        return -ENOENT;
    }

#ifdef _ISOFS
    printf("getattr: found %s, size %d\n", path, isonum_733(inode->record->size));
    memset(stbuf, 0, sizeof(struct stat));
    isofs_direntry2stat(stbuf, inode);
    /*    if (ISO_FLAGS_DIR(inode->record->flags)) {
                        printf("%s %i %i\n", path, (int) stbuf->st_size, stbuf->st_mode);
                    }*/
#else
    printf("getattr: found %s, size %d\n", path, inode->size);
    memset(stbuf, 0, sizeof(struct stat));
    /// @todo may be it is not to follow kernel cramfs and set ino to value of file offset?
    stbuf->st_ino = ncontext->ino;
    stbuf->st_mode = SW16(ncontext->inode->mode);
    /// @todo may be set it to current user uid/gid?
    stbuf->st_uid = SW16(ncontext->inode->uid);
    stbuf->st_gid = ncontext->inode->gid;
    stbuf->st_size = SWSIZE(ncontext->inode->size);
    stbuf->st_blksize = PAGE_CACHE_SIZE;
    stbuf->st_blocks = (stbuf->st_size  - 1) / PAGE_CACHE_SIZE + 1;
    stbuf->st_nlink = 1;
    /// @todo may be set atime/mtime/ctime to current time?
    //     stbuf->st_atime = ncontext->inode->st_atime;
    //     stbuf->st_mtime = time(NULL);
    //     stbuf->st_ctime = time(NULL);
#endif

    return 0;
}

int fs_real_readlink(const char *path, char *target, size_t size)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    int retval;
    if (TRACE) printf("****** READLINK: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode == NULL)
    {
        struct stat v_stbuf;

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISLNK(v_stbuf.st_mode)) return -EINVAL;
            retval = readlink(newpath, target, size-1);
            if (retval == -1) return -errno;
            target[retval] = '\0';
            return 0;
        }

        if (_union_mounting && lstat(path,&v_stbuf)==0)
        {
            if (!S_ISLNK(v_stbuf.st_mode)) return -EINVAL;
            retval = readlink(path, target, size-1);
            if (retval == -1) return -errno;
            target[retval] = '\0';
            return 0;
        }

        fprintf(stderr, "readlink: know nothing about %s\n", path);
        return -ENOENT;
    }

#ifdef _ISOFS
    if (!inode->PX || !inode->SL || !S_ISLNK(inode->st.st_mode))
    {
        fprintf(stderr, "readlink: %s not a link\n", path);
        return -EINVAL;
    }
    strncpy(target, inode->sl, size - 1);
    target[size - 1] = '\0';
#else
    if (!S_ISLNK(SW16(inode->mode)))
    {
        fprintf(stderr, "readlink: %s not a link\n", path);
        return -EINVAL;
    }
    size_t fsize = SWSIZE(inode->size);
    char *obuf = (char *) malloc(PAGE_CACHE_SIZE * 2);
    int nblocks = (fsize - 1) / PAGE_CACHE_SIZE + 1;
    int *bbuf = (int *) malloc(nblocks * 4);
    if (pthread_mutex_lock(& fd_mutex))
    {
        int err = errno;
        perror("readlink: can`l lock fd_mutex");
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
        fprintf(stderr, "readdir: can`t read full block table, errno %d, message %s\n",
            errno, strerror(errno));
        pthread_mutex_unlock(& fd_mutex);
        return -EIO;
    }
    pthread_mutex_unlock(& fd_mutex);
    int i;
    off_t offset = SWOFFSET(inode) * 4 + nblocks * 4;
    off_t ooff = 0;
    for (i=0; i<nblocks; i++)
    {
        int block = SW32(bbuf[i]);
        size_t bsize = block - offset;
        if (bsize > PAGE_CACHE_SIZE * 2)
        {
            free(bbuf);
            free(obuf);
            fprintf(stderr, "read: block size bigger than PAGE_CACHE_SIZE * 2 while reading block %i from symlink %s, bsize %i, offset %i, block %i\n",
                i, path, bsize, (int) offset, block);
            return -EIO;
        }
        size_t osize = PAGE_CACHE_SIZE;
        int rc = cramfs_read_block(offset, bsize, obuf + ooff, & osize);
        if (rc != Z_OK)
        {
            free(bbuf);
            free(obuf);
            fprintf(stderr, "readlink: read block error %i: %s\n", rc, strerror(rc));
            return -rc;
        }
        offset = block;
        ooff += osize;
        obuf[ooff] = '\0';
    }
    strncpy(target, obuf, size - 1);
    target[size - 1] = '\0';
    //     printf("readlink: %s -> %s\n", path, target);
    free(bbuf);
    free(obuf);
#endif
    return 0;
}

int fs_real_open(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** OPEN: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode == NULL)
    {
        struct stat v_stbuf;

        // we maintains a list of apprun pids
        if (_union_mounting && strcmp(path,"/pid")==0) return 0;

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            return 0;
        }

        if (_union_mounting && stat(path,&v_stbuf)==0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            return 0;
        }

        fprintf(stderr, "open: know nothing about %s\n", path);
        return -ENOENT;
    }
#ifdef _ISOFS
    if (ISO_FLAGS_DIR(inode->record->flags))
    {
        fprintf(stderr, "open: %s not a file\n", path);
        return -EINVAL;
    }
    if (ISO_FLAGS_HIDDEN(inode->record->flags))
    {
        fprintf(stderr, "open: %s is a hidden file\n", path);
        // return -EPERM;
    }
#else
    if (!S_ISREG(SW16(inode->mode)))
    {
        fprintf(stderr, "read: %s not a file\n", path);
        return -EINVAL;
    }
#endif
    return 0;
}

int fs_real_read(const char *path, char *out_buf, size_t size, off_t offset)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (TRACE) printf("****** READ: %s\n", path);
    if (inode == NULL)
    {
        struct stat v_stbuf;
        int v_file, v_count;

        // we maintains a list of apprun pids
        if (_union_mounting && strcmp(path,"/pid")==0)
        {
            int v_size = 0;
            GList *v_newlist=NULL, *v_item;

            if (offset != 0) return -ENOENT;

            v_item = g_list_first(pid_lookup_table);
            while (v_item != NULL)
            {
                // check if the pid is still alive
                if (getsid(atoi(v_item->data)) == -1)
                {
                    v_item = g_list_next(v_item);
                    continue;
                }

                // creation of a new list to replace the old one
                v_newlist = g_list_append(v_newlist, g_strdup(v_item->data));

                // we add the value to the out_buf buffer
                memcpy(&(out_buf[v_size]), v_item->data, strlen(v_item->data));
                v_size += strlen(v_item->data);
                free(v_item->data);
                v_item = g_list_next(v_item);
                if (v_item == NULL) break;
                out_buf[v_size] = ' ';
                v_size++;
            }
            g_list_free(pid_lookup_table);
            pid_lookup_table = v_newlist;
            size = v_size;
            return v_size;
        }

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            v_file = open(newpath, O_RDONLY);
            if (v_file == -1) return -EACCES;
            lseek(v_file, offset, SEEK_SET);
            v_count = read(v_file, out_buf, size);
            close(v_file);
            return size;
        }

        if (_union_mounting && stat(path,&v_stbuf)==0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            v_file = open(path, O_RDONLY);
            if (v_file == -1) return -EACCES;
            lseek(v_file, offset, SEEK_SET);
            v_count = read(v_file, out_buf, size);
            close(v_file);
            return size;
        }

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

int fs_real_statfs(const char *path, struct statfs *stbuf)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** STATFS: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode=NULL;
    if (_union_mounting) inode=fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    if (_union_mounting)
    {
        cramfs_inode_context *ncontext = fs_lookup(path);
        if (ncontext) inode=ncontext->inode;
    }
#endif
    if (!_union_mounting || inode)
    {
#ifdef _ISOFS
        stbuf->f_type = ISOFS_SUPER_MAGIC;
        stbuf->f_bsize = context.data_size;                       // or PAGE_CACHE_SIZE?
        stbuf->f_blocks = 0;                                      // while it is possible to calculate this, i see no reasons to do so
        stbuf->f_bfree = 0;
        stbuf->f_bavail = 0;
        stbuf->f_files = 0;
        stbuf->f_ffree = 0;
        stbuf->f_namelen = NAME_MAX - 1;                          // ? not sure..
#else
        stbuf->f_type = CRAMFS_MAGIC;
        stbuf->f_bsize = PAGE_CACHE_SIZE;
        stbuf->f_blocks = SW32(context.super.fsid.blocks);
        stbuf->f_bfree = 0;
        stbuf->f_bavail = 0;
        stbuf->f_files = SW32(context.super.fsid.files);
        stbuf->f_ffree = 0;
        stbuf->f_namelen = CRAMFS_MAXPATHLEN;
#endif
        return 0;
    }

    fs_home(path, &newpath);
    return statfs(newpath, stbuf);
}

//
// union mounting feature
//

int fs_real_write(const char *path, const char *out_buf, size_t size, off_t offset)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    int v_file, v_count, v_pid=-1, retval;
    char *newpath=ppath;
    if (TRACE) printf("****** WRITE: %s (%d octets, %d offset)\n", path, size, (int)offset);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "write: you don't have permission on %s\n", path);
        return -EPERM;
    }

    // we maintains a list of apprun pids
    if (strcmp(path,"/pid") == 0)
    {
	// You can only add a pid
        if (offset != 0) return -EINVAL;

        sscanf(out_buf, "%d", &v_pid);
        if (v_pid != -1)
        {
            sprintf(ppath, "%d", v_pid);
            if (strncmp(out_buf,ppath,strlen(ppath)) != 0) return -EINVAL;
            pid_lookup_table = g_list_append(pid_lookup_table, g_strdup(ppath));
            return size;
        }
    }

    retval = fs_home_stat(path, &newpath, &v_stbuf);
    if (retval != 1)
    {
        if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
        if (retval == -1) fs_file_dup(path,newpath);
        v_file = open(newpath, O_RDWR|O_APPEND);
        if (v_file == -1) return -EACCES;
        ftruncate(v_file, offset);
        v_count = write(v_file, out_buf, size);
        close(v_file);
        return size;
    }

    if (_union_mounting && stat(path,&v_stbuf)==0)
    {
        if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
        v_file = open(path, O_RDWR|O_APPEND);
        if (v_file == -1) return -EACCES;
        ftruncate(v_file, offset);
        v_count = write(v_file, out_buf, size);
        close(v_file);
        return size;
    }
    return -ENOENT;
}

int fs_real_access(const char *path, int amode)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    if (TRACE) printf("****** ACCESS: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode) return 0;
    if (fs_home_stat(path,&newpath,&v_stbuf) == 0) return access(newpath,amode);
    return access(path,amode);
}

int fs_real_mknod(const char *path, mode_t mode, dev_t dev)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** MKNOD: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "mknod: you don't have permission on %s\n", path);
        return -EPERM;
    }
    if (fs_home(path,&newpath) == 0) fs_path_mkdir(newpath,1);
    return mknod(newpath, mode, dev);
}

int fs_real_unlink(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    int retval;
    if (TRACE) printf("****** UNLINK: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "unlink: you don't have permission on %s\n", path);
        return -EPERM;
    }

    switch(fs_home_stat(path,&newpath,&v_stbuf))
    {
        case 0:
            if (TRACE) printf("****** #1 unlink(%s)\n", newpath);
            retval = unlink(newpath);
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (TRACE) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
            return retval;
        case -1:
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (TRACE) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
    }
    if (TRACE) printf("****** #2 unlink(%s)\n", newpath);
    return unlink(newpath);
}

int fs_real_rmdir(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    int retval;
    if (TRACE) printf("****** RMDIR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "rmdir: you don't have permission on %s\n", path);
        return -EPERM;
    }

    switch(fs_home_stat(path,&newpath,&v_stbuf))
    {
        case 0:
            if (TRACE) printf("****** #1 rmdir(%s)\n", newpath);
            retval = rmdir(newpath);
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (TRACE) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
            return retval;
        case -1:
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (TRACE) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
    }
    if (TRACE) printf("****** #2 rmdir(%s)\n", newpath);
    return rmdir(newpath);
}

int fs_real_mkdir(const char *path, mode_t mode)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (TRACE) printf("****** MKDIR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "mkdir: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home(path,&newpath) == 0) fs_path_mkdir(newpath,0);
    if (TRACE) printf("****** mkdir: %s %d\n", newpath, mode);
    return mkdir(newpath, mode);
}

int fs_real_symlink(const char *path1, const char *path2)
{
    char sppath1[FS_MAXPATH],sppath2[FS_MAXPATH];
    char *spath1=sppath1;
    char *spath2=sppath2;
    int retint;

    if (TRACE) printf("****** SYMLINK: %s -> %s\n", path1, path2);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path2);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path2);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "symlink: you don't have permission on %s\n", path2);
        return -EPERM;
    }

    retint = fs_home(path1,&spath1);
    if (fs_home(path2,&spath2)==0 || retint==0)
    {
        fs_path_mkdir(spath2, 1);
        if (TRACE) printf("****** symlink #1 %s -> %s\n", spath1, spath2);
        return symlink(spath1, spath2);
    }

    if (TRACE) printf("****** symlink #2 %s -> %s\n", path1, path2);
    return symlink(path1, path2);
}

int fs_real_rename(const char *oldpath, const char *newpath)
{
    char ppath1[FS_MAXPATH],ppath2[FS_MAXPATH];
    char *path1=ppath1;
    char *path2=ppath2;
    if (TRACE) printf("****** RENAME: %s -> %s\n", oldpath, newpath);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(oldpath);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(oldpath);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "rename: you don't have permission on %s\n", oldpath);
        return -EPERM;
    }
    if (fs_home(oldpath,&path1)==0 && fs_home(newpath,&path2)==0)
    {
        fs_path_mkdir(path2, 1);
        return rename(path1, path2);
    }
    return rename(oldpath, newpath);
}

int fs_real_link(const char *oldpath, const char *newpath)
{
    char ppath1[FS_MAXPATH],ppath2[FS_MAXPATH];
    char *path1=ppath1;
    char *path2=ppath2;
    if (TRACE) printf("****** LINK: %s -> %s\n", oldpath, newpath);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(oldpath);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(oldpath);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "link: you don't have permission on %s\n", oldpath);
        return -EPERM;
    }

    fs_home(oldpath,&path1);
    if (fs_home(newpath,&path2) == 0)
    {
        fs_path_mkdir(path2, 1);
        return link(path1, path2);
    }
    return link(oldpath, newpath);
}

int fs_real_chmod(const char *path, mode_t mode)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** CHMOD: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "chmod: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return chmod(newpath, mode);
}

int fs_real_chown(const char *path, uid_t owner, gid_t group)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** CHOWN: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "chown: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return chown(newpath, owner, group);
}

int fs_real_truncate(const char *path, off_t length)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** TRUNCATE: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "truncate: you don't have permission on %s\n", path);
        return -EPERM;
    }

    // we maintains a list of apprun pids
    if (length==0 && strcmp(path,"/pid")==0) return 0;

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return truncate(newpath, length);
}

int fs_real_utime(const char *path, struct utimbuf *times)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** UTIME: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "utime: you don't have permission on %s\n", path);
        return -ENOENT;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return utime(newpath, times);
}

int fs_real_setxattr(const char *path, const char *name, const void *value,
    size_t size, int options)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** SETXATTR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "setxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return setxattr(newpath, name, value, size, options);
}

int fs_real_getxattr(const char *path, const char *name, void *value, size_t size)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    int retval;
    if (TRACE) printf("****** GETXATTR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "getxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
    {
        retval = getxattr(newpath, name, value, size);
        if (retval == -1) return -ENOTSUP;
        return retval;
    }
    retval = getxattr(path, name, value, size);
    if (retval == -1) return -ENOTSUP;
    return retval;
}

int fs_real_listxattr(const char *path, char *list, size_t size)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** LISTXATTR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "listxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == 0) return listxattr(newpath,list,size);
    return listxattr(path,list,size);
}

int fs_real_removexattr(const char *path, const char *name)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (TRACE) printf("****** REMOVEXATTR: %s\n", path);
#ifdef _ISOFS
    isofs_inode *inode = fs_lookup(path);
#else
    struct cramfs_inode *inode = NULL;
    cramfs_inode_context *ncontext = fs_lookup(path);
    if (ncontext) inode=ncontext->inode;
#endif
    if (inode)
    {
        fprintf(stderr, "removexattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    return removexattr(newpath, name);
}
