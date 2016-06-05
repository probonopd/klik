/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2005-2006 by Dmitry Morozhnikov <dmiceman AT mail.ru>
 *   Original version (http://ubiz.ru/dm/)
 *
 *   Copyright (C) 2006-2008 by Lionel Tricon <lionel.tricon AT free.fr>
 *   Klik2 support - union-fuse + sandbox feature
 *
 *   Copyright (C) 2008 by Jason Taylor <killerkiwi2005 AT gmail.com>
 *   Klik2 support - add the support of the -d option
 *
 *   Copyright (C) 2007 by Chandan Dutta Chowdhury
 *   Locally implement g_strv_length, this is missing in glib2 for rhel3/rhel4
 *
 *   Copyright (C) 2005 by Renzo Davoli <renzo AT cs.unibo.it>
 *   Auto-endianess conversion
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

/*
 * Specific functions to the klik2 runtime
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
 *  fs_rewrite_loader
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
#include <errno.h>
#include <elf.h>

#include "fusioniso-fs.h"
#include "fusioniso-isofs.h"

extern char *extern_mount_point;
extern bool extern_union_mounting;
extern sandbox_type extern_sandbox_mounting;
extern char* extern_redirect_path;
extern int   extern_size_redirect_path;
extern char* extern_sandbox_path;
extern int   extern_size_sandbox_path;
extern char* extern_exclude_list[];
extern int   extern_exclude_length[];
extern int   extern_list_max;
extern char* extern_base_root;
extern int   extern_base_root_length;
extern bool  extern_debug;
extern bool  extern_glibc;

isofs_context extern_context;

GHashTable *lookup_table;
GHashTable *removed_lookup_table;
GHashTable *real_lookup_table;
GHashTable *negative_lookup_table;
static char *negative_value = "not exists";
static char *removed_value = "removed";

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
    if (extern_debug) printf("****** fs_path_mkdir %s\n", p_path);

    strcpy(v_path, p_path);
    retval = stat(v_path, &v_stbuf);
    if (!retval && !S_ISDIR(v_stbuf.st_mode))
    {
        retval = stat(dirname((char*)v_path), &v_stbuf);
        if (!retval && S_ISDIR(v_stbuf.st_mode)) return;
    }

    if (extern_debug) printf("****** g_strsplit %s -> %s\n", v_path, &(v_path[extern_size_sandbox_path]));
    parts = g_strsplit(&(v_path[extern_size_sandbox_path]), "/", -1);
    strcpy(source, extern_redirect_path);
    strcpy(dest, extern_sandbox_path);
    for (i=0; i<local_g_strv_length(parts)-p_value; i++)
    {
        strcat(source, "/");
        strcat(source, parts[i]);
        strcat(dest, "/");
        strcat(dest, parts[i]);
        retval = stat(source, &v_stbuf);
        if (extern_debug) printf("****** ISDIR PATH_MKDIR: %s\n", source);
        if (!retval && !S_ISDIR(v_stbuf.st_mode)) break;
        if (extern_debug) printf("****** PATH_MKDIR: %s\n", dest);
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
    if (extern_debug) printf("****** fs_file_dup %s -> %s\n", p_path, p_newpath);

    retval = stat(p_path, &v_stbuf);
    if (!retval && S_ISDIR(v_stbuf.st_mode))
    {
        fs_path_mkdir(p_newpath, 0);
        return;
    }

    fs_path_mkdir(p_newpath, 1);
    file_in = fopen(p_path, "r");
    if (file_in==NULL && extern_sandbox_mounting!=sandbox_data) return;
    file_out = fopen(p_newpath, "w");
    if (file_out == NULL)
    {
        if (file_in != NULL) fclose(file_in);
        return;
    }
    while (file_in != NULL)
    {
        number = fread(buffer, 1, FS_MAXPATH, file_in);
        fwrite(buffer, 1, number, file_out);
        if (feof(file_in)) break;
    }
    fclose(file_out);
    if (file_in != NULL) fclose(file_in);
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

    if (extern_sandbox_mounting==sandbox_none || v_size<extern_size_redirect_path)
    {
        *p_newpath = (char*)p_path;
        return 1;
    }

    // We try to find if we need direct access to a file
    for (i=0; i<extern_list_max; i++)
    {
        if (extern_exclude_length[i]>v_size ||
            p_path[extern_exclude_length[i]-1]!=(extern_exclude_list[i])[extern_exclude_length[i]-1] ||
            strncmp(extern_exclude_list[i],p_path,extern_exclude_length[i])!=0) continue;
        if (extern_exclude_length[i]==v_size || p_path[extern_exclude_length[i]]=='/')
        {
            *p_newpath = (char*)p_path;
            return 1;
        }
    }

    // -B support
    if (extern_sandbox_mounting==sandbox_data &&
        extern_size_redirect_path==1 &&
        p_path[0]=='/')
    {
        strcpy(*p_newpath, extern_sandbox_path);
        strcat(*p_newpath, p_path);
        return 0;
    }

    // -d support
    if (extern_sandbox_mounting==sandbox_portable &&
        p_path[extern_size_redirect_path-2]=='/' &&
        p_path[extern_size_redirect_path-1]=='.' &&
        !strncmp(p_path,extern_redirect_path,extern_size_redirect_path))
    {
        strcpy(*p_newpath, extern_sandbox_path);
        strcat(*p_newpath, &(p_path[extern_size_redirect_path]));
        return 0;
    }

    // -b support
    if (extern_sandbox_mounting==sandbox_home &&
        (v_size==extern_size_redirect_path || p_path[extern_size_redirect_path]=='/') &&
        !strncmp(p_path,extern_redirect_path,extern_size_redirect_path))
    {
        strcpy(*p_newpath, extern_sandbox_path);
        strcat(*p_newpath, &(p_path[extern_size_redirect_path]));
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
 * Change the loader from an elf binary
 */
void fs_rewrite_loader(char *p_path, int p_size)
{
    int i;
    for (i=0; i<p_size-18; i++)
    {
        if (p_path[i] != '/') continue;
        if (memcmp(&(p_path[i]), "/lib/ld-linux.so.2", 18) == 0)
        {
            memcpy(&(p_path[i+8]), "klik2", 5);
            break;
        }
        if (memcmp(&(p_path[i]), "/lib/ld-lsb.so.", 15) == 0)
        {
            memcpy(&(p_path[i+8]), "lsk", 3);
            break;
        }
    }
}


/*
 * Important function
 * Check if the path is found inside the cmg file
 *
 */
isofs_inode* fs_lookup(const char *p_path)
{
    if (p_path[0] == '\0') return NULL;
    isofs_inode* inode = g_hash_table_lookup(lookup_table, p_path);
    if (inode != NULL) return inode;
    if (g_hash_table_lookup(negative_lookup_table, p_path)) return NULL;
    if (extern_union_mounting && g_hash_table_lookup(real_lookup_table, p_path)) return NULL;
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
        if (extern_union_mounting && g_hash_table_lookup(real_lookup_table, rpath1)) return NULL;
        inode = g_hash_table_lookup(lookup_table, rpath1);
        if (inode == NULL)
        {
            //          printf("trying to load %s...\n", rpath);
            int rc = fs_real_readdir(0, rpath, NULL, NULL);
            if (!extern_union_mounting && rc)
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
        if (extern_union_mounting && stat(p_path,&v_stbuf)!=0)
        {
            g_hash_table_insert(negative_lookup_table, g_strdup(p_path), negative_value);
            return NULL;
        }
        if (!extern_union_mounting) g_hash_table_insert(negative_lookup_table, g_strdup(p_path), negative_value);
        return NULL;
    }
    return inode;
}

int fs_real_opendir(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (extern_debug) printf("****** OPENDIR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode==NULL)
    {
        struct stat v_stbuf;

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISDIR(v_stbuf.st_mode)) return -ENOENT;
            return 0;
        }

        if (extern_union_mounting && lstat(path,&v_stbuf)==0)
        {
            if (!S_ISDIR(v_stbuf.st_mode)) return -ENOENT;
            return 0;
        }

        fprintf(stderr, "opendir: know nothing about %s\n", path);
        return -ENOENT;
    }

    if (!ISO_FLAGS_DIR(inode->record->flags))
    {
        fprintf(stderr, "opendir: %s not a dir\n", path);
        return -ENOTDIR;
    }

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

    if (extern_debug) printf("****** READDIR: (%s) flag=%d\n", path, real);
    isofs_inode *current_inode = fs_lookup(path);
    isofs_inode *inode2 = NULL;

    if (!extern_union_mounting && current_inode==NULL)
    {
        fprintf(stderr, "readdir: know nothing about %s\n", path);
        return -ENOENT;
    }

    retval2 = fs_home(path,&newpath);
    if (current_inode == NULL)
    {
        if (real && retval2!=1) goto out_readdir;
        if (!real && retval2!=1)
        {
            g_hash_table_insert(real_lookup_table, g_strdup(path), "real");
            return 0;
        }
        if (extern_union_mounting)
        {
            retval1 = stat(path, &v_stbuf);
            if (real && retval1==0) goto out_readdir;
            if (!real && retval1==0)
            {
                g_hash_table_insert(real_lookup_table, g_strdup(path), "real");
                return 0;
            }
        }
        if (!real) fprintf(stderr, "readdir: know nothing about %s\n", path);
        return -ENOENT;
    }

    struct iso_directory_record *current = current_inode->record;
    if (!ISO_FLAGS_DIR(current->flags))
    {
        fprintf(stderr, "readdir: %s not a dir\n", path);
        return -ENOTDIR;
    }

    size_t current_size = isonum_733(current->size);

    int block = isonum_733(current->extent);
    buf = (char *) malloc(extern_context.data_size);
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
        if (rc != extern_context.data_size)
        {
            // can`t be allowed
            fprintf(stderr, "readdir: can`t read whole block, read only %d bytes, block %d\n", rc, block);
            free(buf);
            return -EIO;
        }
        block_count++;

        if (boff > 0)
        {
            total_size += (extern_context.data_size - boff);
            boff = 0;
        }

        while (boff + sizeof(struct iso_directory_record) <= extern_context.data_size &&
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
                total_size += (extern_context.data_size - boff);
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
                    total_size += (extern_context.data_size - boff);
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
                goto out_readdir;
            }

            isofs_inode *inode = (isofs_inode *) malloc(sizeof(isofs_inode));
            if (inode == NULL)
            {
                perror("Can`t malloc: ");
                return -ENOMEM;
            }
            memset(inode, 0, sizeof(isofs_inode));
            inode->st_ino = extern_context.last_ino;
            extern_context.last_ino++;

            struct iso_directory_record *n_rec =
                (struct iso_directory_record *) malloc (sizeof(struct iso_directory_record));
            if (!n_rec)
            {
                perror("Can`t malloc: ");
                return -ENOMEM;
            }
            memcpy(n_rec, record, sizeof(struct iso_directory_record));
            inode->record = n_rec;

            if (extern_context.susp || path[1] == '\0')               // if susp is known to be present or this is a root dir ("/")
            {
                /*                printf("sa offset %d, sa_len %d\n",
                                                                        sizeof(struct iso_directory_record) + name_len + pad_len, sa_len);*/
                rc = isofs_parse_sa(inode,
                    ((char *) record) + sizeof(struct iso_directory_record) + name_len + pad_len + extern_context.susp_skip,
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
                if (extern_debug) printf("REGISTER [%s] IN LOOKUP_TABLE\n", absolute_entry);
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

    out_readdir:
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
                inode2 = fs_lookup(v_chemin);
                if (inode2 == NULL)
                {
                    memset(&v_stbuf, '\0', sizeof(struct stat));
                    if (filler && !lstat(v_chemin,&v_stbuf))
                    {
                        filler(filler_buf, v_dirent->d_name, &v_stbuf, 0);
                        if (extern_debug) printf("FILLER #1 [%s]\n", v_dirent->d_name);
                    }
                }
            }
            closedir(v_dir);
        }
    }

    if (real && extern_union_mounting)
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
                inode2 = fs_lookup(v_chemin);
                if (inode2==NULL && retval1!=0)
                {
                    memset(&v_stbuf, '\0', sizeof(struct stat));
                    if (filler && !lstat(v_chemin,&v_stbuf))
                    if (g_hash_table_lookup(removed_lookup_table,v_chemin) == NULL)
                    {
                        filler(filler_buf, v_dirent->d_name, &v_stbuf, 0);
                        if (extern_debug) printf("FILLER #2 [%s]\n", v_dirent->d_name);
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
    char ppath[FS_MAXPATH], target[FS_MAXPATH];
    char *newpath=ppath;
    int i, j;

    if (extern_debug) printf("****** GETATTR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode == NULL)
    {
        if (extern_glibc && strcmp(path,"/lib/ld-linux.so.2")==0)
        {
            if (lstat("/lib/ld-klik2.so.2",stbuf) == 0) return 0;
            return -ENOENT;
        }

        // we forbid access to the location where all mount points are located
        if (extern_union_mounting)
        {
            for (i=0,j=0; i<extern_base_root_length && path[j]!='\0';) {
                if (extern_base_root[i] != path[j]) break;
                if (path[j] == '/') for (;path[j]=='/';j++);
                else j++;
                i++;
                }
            if (extern_base_root[i] == '\0') return -ENOENT;
        }

        if (fs_home_stat(path,&newpath,stbuf) == 0) return 0;
        if (extern_union_mounting && lstat(path,stbuf)==0)
        {
            int retval;

            // If a file was previously removed, we need to take care
            if (g_hash_table_lookup(removed_lookup_table, path))
            {
                if (extern_debug) printf("****** LOOKUP removed_lookup_table -> %s\n", path);
                return -ENOENT;
            }

            // Point to a file located into the iso image
            retval = readlink(path, target, FS_MAXPATH-1);
            if (retval != -1)
            {
                target[retval] = '\0';
                stbuf->st_size += strlen(extern_mount_point);
            }

            return 0;
        }
        // this error occur too often when browsing mounted tree with konqueror --
        // it check existence of .directory file with stat()
        // fprintf(stderr, "getattr: know nothing about %s\n", path);
        return -ENOENT;
    }

    fprintf(stderr, "getattr: found %s, size %d\n", path, isonum_733(inode->record->size));
    memset(stbuf, 0, sizeof(struct stat));
    isofs_direntry2stat(stbuf, inode);
    /*    if (ISO_FLAGS_DIR(inode->record->flags)) {
                        printf("%s %i %i\n", path, (int) stbuf->st_size, stbuf->st_mode);
                    }*/

    return 0;
}

int fs_real_readlink(const char *path, char *target, size_t size)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    int retval;
    if (extern_debug) printf("****** READLINK: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode == NULL)
    {
        struct stat v_stbuf;

        // Use our own loader
        if (extern_glibc && strcmp(path,"/lib/ld-linux.so.2")==0)
        {
            retval = readlink("/lib/ld-klik2.so.2", target, size-1);
            if (retval == -1) return -errno;
            target[retval] = '\0';
            return 0;
        }

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISLNK(v_stbuf.st_mode)) return -EINVAL;
            retval = readlink(newpath, target, size-1);
            if (retval == -1) return -errno;
            target[retval] = '\0';
            strcpy(newpath, extern_sandbox_path);
            strcat(newpath, target);
            if (extern_sandbox_mounting!=sandbox_none && stat(newpath,&v_stbuf)==0)
            {
                strcpy(target, newpath);
                return 0;
            }
            return 0;
        }

        if (extern_union_mounting && lstat(path,&v_stbuf)==0)
        {
            if (!S_ISLNK(v_stbuf.st_mode)) return -EINVAL;
            retval = readlink(path, target, size-1);
            if (retval == -1) return -errno;
            target[retval] = '\0';
            if (lstat(target,&v_stbuf)==-1 && fs_lookup(target))
            {
                sprintf(ppath,"%s%s", extern_mount_point, target);
                strcpy(target, ppath);
            }
            return 0;
        }

        fprintf(stderr, "readlink: know nothing about %s\n", path);
        return -ENOENT;
    }

    if (!inode->PX || !inode->SL || !S_ISLNK(inode->st.st_mode))
    {
        fprintf(stderr, "readlink: %s not a link\n", path);
        return -EINVAL;
    }
    strncpy(target, inode->sl, size - 1);
    target[size - 1] = '\0';
    return 0;
}

int fs_real_open(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (extern_debug) printf("****** OPEN: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode == NULL)
    {
        struct stat v_stbuf;

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            return 0;
        }

        if (extern_union_mounting && stat(path,&v_stbuf)==0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            return 0;
        }

        fprintf(stderr, "open: know nothing about %s\n", path);
        return -ENOENT;
    }
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
    return 0;
}

int fs_real_read(const char *path, char *out_buf, size_t size, off_t offset)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (extern_debug) printf("****** READ: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode == NULL)
    {
        struct stat v_stbuf;
        int v_file, v_count;

        // Use our own loader
        if (extern_glibc && strcmp(path,"/lib/ld-linux.so.2")==0)
        {
            v_file = open("/lib/ld-klik2.so.2", O_RDONLY);
            if (v_file == -1) return -EACCES;
            lseek(v_file, offset, SEEK_SET);
            v_count = read(v_file, out_buf, size);
            close(v_file);
            return v_count;
        }

        if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            v_file = open(newpath, O_RDONLY);
            if (v_file == -1) return -EACCES;
            lseek(v_file, offset, SEEK_SET);
            v_count = read(v_file, out_buf, size);
            close(v_file);
            if (extern_glibc && offset==0 && v_count>18 && memcmp(out_buf, ELFMAG, SELFMAG)==0 && out_buf[16]==ET_EXEC)
            {
                fs_rewrite_loader(out_buf, v_count);
            }
            return v_count;
        }

        if (extern_union_mounting && stat(path,&v_stbuf)==0)
        {
            if (!S_ISREG(v_stbuf.st_mode)) return -EINVAL;
            v_file = open(path, O_RDONLY);
            if (v_file == -1) return -EACCES;
            lseek(v_file, offset, SEEK_SET);
            v_count = read(v_file, out_buf, size);
            close(v_file);
            if (extern_glibc && offset==0 && v_count>18 && out_buf[0]==127 && memcmp(out_buf, ELFMAG, SELFMAG)==0 && out_buf[16]==ET_EXEC)
            {
                fs_rewrite_loader(out_buf, v_count);
            }
            return v_count;
        }

        fprintf(stderr, "read: know nothing about %s\n", path);
        return -ENOENT;
    }

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
        int v_count = isofs_real_read_zf(inode, out_buf, size, offset);
        if (extern_glibc && offset==0 && v_count>18 && out_buf[0]==127 && memcmp(out_buf, ELFMAG, SELFMAG)==0 && out_buf[16]==ET_EXEC)
        {
            fs_rewrite_loader(out_buf, v_count);
        }
        return v_count;
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

    int start = offset / extern_context.data_size;
    int end = (offset + size) / extern_context.data_size;
    int shift = offset % extern_context.data_size;
    int block = start_block + start;
    //     printf("read: path %s, size %d, offset %d, fsize %d, start %d, end %d, shift %d\n",
    //         path, size, (int) offset, fsize, start, end, shift);

    char *buf = (char *) malloc(extern_context.data_size);
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

        memcpy(out_buf + count * extern_context.data_size, buf + shift, len - shift);

        count++;
        shift = 0;
        size_left -= len;
        block++;
    }

    free(buf);
    if (extern_glibc && total_size>18 && offset==0 && out_buf[0]==127 && memcmp(out_buf, ELFMAG, SELFMAG)==0 && out_buf[16]==ET_EXEC)
    {
        fs_rewrite_loader(out_buf, total_size);
    }
    return total_size;
}

int fs_real_statfs(const char *path, struct statfs *stbuf)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (extern_debug) printf("****** STATFS: (%s)\n", path);
    isofs_inode *inode=NULL;
    if (extern_glibc && strcmp(path,"/lib/ld-linux.so.2")==0)
    {
        if (statfs("/lib/ld-klik2.so.2",stbuf) == 0) return 0;
        return -errno;
    }
    if (extern_union_mounting) inode=fs_lookup(path);
    if (!extern_union_mounting || inode)
    {
        stbuf->f_type = ISOFS_SUPER_MAGIC;
        stbuf->f_bsize = extern_context.data_size;                // or PAGE_CACHE_SIZE?
        stbuf->f_blocks = 0;                                      // while it is possible to calculate this, i see no reasons to do so
        stbuf->f_bfree = 0;
        stbuf->f_bavail = 0;
        stbuf->f_files = 0;
        stbuf->f_ffree = 0;
        stbuf->f_namelen = NAME_MAX - 1;                          // ? not sure..
        return 0;
    }

    fs_home(path, &newpath);
    if (statfs(newpath,stbuf) == 0) return 0;
    return -errno;
}

//
// union mounting feature
//

int fs_real_write(const char *path, const char *out_buf, size_t size, off_t offset)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    int v_file, v_count, retval;
    char *newpath=ppath;
    if (extern_debug) printf("****** WRITE: (%s) (%d octets, %d offset)\n", path, size, (int)offset);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "write: you don't have permission on %s\n", path);
        return -EPERM;
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

    if (extern_union_mounting && stat(path,&v_stbuf)==0)
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
    char ppath[2*FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    if (extern_debug) printf("****** ACCESS: (%s) mode=%d\n", path, amode);
    isofs_inode *inode = fs_lookup(path);
    if (inode) return 0;
    if (extern_sandbox_mounting==sandbox_data && (amode&W_OK)) return 0;
    if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
    {
        if (access(newpath,amode) == 0) return 0;
        return -errno;
    }
    if (access(path,amode) == 0) return 0;
    return -errno;
}

int fs_real_mknod(const char *path, mode_t mode, dev_t dev)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    mode_t mode2 = mode;
    if (extern_debug) printf("****** MKNOD: (%s) mode=%d dev=%ld\n", path, mode, (long)dev);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "mknod: you don't have permission on %s\n", path);
        return -EPERM;
    }
    if (fs_home(path,&newpath) == 0) fs_path_mkdir(newpath,1);
    if (mode == 32768) mode2=33188; /* temporary fix */
    if (mknod(newpath,mode2,dev) == 0) return 0;
    return -errno;
}

int fs_real_unlink(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    int retval;
    if (extern_debug) printf("****** UNLINK: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "unlink: you don't have permission on %s\n", path);
        return -EPERM;
    }

    switch(fs_home_stat(path,&newpath,&v_stbuf))
    {
        case 0:
            if (extern_debug) printf("****** #1 unlink(%s)\n", newpath);
            retval = unlink(newpath);
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (extern_debug) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
            return retval;
        case -1:
            if (extern_debug) printf("****** #2 unlink(%s)\n", newpath);
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (extern_debug) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
    }
    if (extern_debug) printf("****** #3 unlink(%s)\n", newpath);
    if (unlink(newpath) == 0) return 0;
    return -errno;
}

int fs_real_rmdir(const char *path)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    struct stat v_stbuf;
    int retval, errno2;
    if (extern_debug) printf("****** RMDIR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "rmdir: you don't have permission on %s\n", path);
        return -EPERM;
    }

    switch(fs_home_stat(path,&newpath,&v_stbuf))
    {
        case 0:
            if (extern_debug) printf("****** #1 rmdir(%s)\n", newpath);
            retval = rmdir(newpath);
            errno2 = errno;
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (extern_debug) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
            if (retval == 0) return 0;
            return -errno2;
        case -1:
            // If the file exist on disk,
            // we keep the file name in a list
            if (lstat(path,&v_stbuf) == 0)
            {
                if (extern_debug) printf("****** INSERT removed_lookup_table -> %s\n", path);
                g_hash_table_insert(removed_lookup_table, g_strdup(path), removed_value);
                return 0;
            }
    }
    if (extern_debug) printf("****** #2 rmdir(%s)\n", newpath);
    if (rmdir(newpath) == 0) return 0;
    return -errno;
}

int fs_real_mkdir(const char *path, mode_t mode)
{
    char ppath[FS_MAXPATH];
    char *newpath=ppath;
    if (extern_debug) printf("****** MKDIR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "mkdir: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home(path,&newpath) == 0) fs_path_mkdir(newpath,1);
    if (extern_debug) printf("****** mkdir: %s %d\n", newpath, mode);

    if (mkdir(newpath,mode) == 0) return 0;
    return -errno;
}

int fs_real_symlink(const char *path1, const char *path2)
{
    char sppath2[FS_MAXPATH];
    char *spath2=sppath2;

    if (extern_debug) printf("****** SYMLINK: (%s) -> (%s)\n", path1, path2);
    isofs_inode *inode = fs_lookup(path2);
    if (inode)
    {
        fprintf(stderr, "symlink: you don't have permission on %s\n", path2);
        return -EPERM;
    }

    if (fs_home(path2,&spath2) == 0)
    {
        fs_path_mkdir(spath2, 1);
        if (extern_debug) printf("****** symlink #1 %s -> %s\n", path1, spath2);
        if (symlink(path1,spath2) == 0) return 0;
        return -errno;
    }

    if (extern_debug) printf("****** symlink #2 %s -> %s\n", path1, path2);
    if (symlink(path1,path2) == 0) return 0;
    return -errno;
}

int fs_real_rename(const char *oldpath, const char *newpath)
{
    char ppath1[FS_MAXPATH],ppath2[FS_MAXPATH];
    struct stat v_stbuf;
    char *path1=ppath1;
    char *path2=ppath2;
    int retval;
    if (extern_debug) printf("****** RENAME: (%s) -> (%s)\n", oldpath, newpath);
    isofs_inode *inode = fs_lookup(oldpath);
    if (inode)
    {
        fprintf(stderr, "rename: you don't have permission on %s\n", oldpath);
        return -EPERM;
    }
    retval = fs_home_stat(oldpath,&path1,&v_stbuf);
    if (fs_home(newpath,&path2)==0 || retval!=1)
    {
        if (retval == -1)
        {
            retval = stat(oldpath, &v_stbuf);
            if (!retval && S_ISDIR(v_stbuf.st_mode))
            {
                printf("TODO\n");
                fflush(stdout);
            }
            else fs_file_dup(oldpath,path1);
            if (extern_debug) printf("****** INSERT removed_lookup_table -> %s\n", oldpath);
            g_hash_table_insert(removed_lookup_table, g_strdup(oldpath), removed_value);
        }
        fs_path_mkdir(path2, 1);

        if (extern_debug) printf("****** rename #1 %s -> %s\n", path1, path2);
        if (rename(path1,path2) == 0) return 0;
        return -errno;
    }
    if (extern_debug) printf("****** rename #2 %s -> %s\n", oldpath, newpath);
    if (rename(oldpath,newpath) == 0) return 0;
    return -errno;
}

int fs_real_link(const char *oldpath, const char *newpath)
{
    char ppath2[FS_MAXPATH];
    char *path2=ppath2;
    if (extern_debug) printf("****** LINK: (%s) -> (%s)\n", oldpath, newpath);
    isofs_inode *inode = fs_lookup(oldpath);
    if (inode)
    {
        fprintf(stderr, "link: you don't have permission on %s\n", oldpath);
        return -EPERM;
    }

    if (fs_home(newpath,&path2) == 0)
    {
        fs_path_mkdir(path2, 1);
        if (extern_debug) printf("****** link #1: %s -> %s\n", oldpath, path2);
        if (link(oldpath,path2) == 0) return 0;
        return -errno;
    }
    if (extern_debug) printf("****** link #2: %s -> %s\n", oldpath, oldpath);
    if (link(oldpath,newpath) == 0) return 0;
    return -errno;
}

int fs_real_chmod(const char *path, mode_t mode)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** CHMOD: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "chmod: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (chmod(newpath,mode) == 0) return 0;
    return -errno;
}

int fs_real_chown(const char *path, uid_t owner, gid_t group)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** CHOWN: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "chown: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (chown(newpath,owner,group) == 0) return 0;
    return -errno;
}

int fs_real_truncate(const char *path, off_t length)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** TRUNCATE: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "truncate: you don't have permission on %s\n", path);
        return -EPERM;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (truncate(newpath,length) == 0) return 0;
    return -errno;
}

int fs_real_utime(const char *path, struct utimbuf *times)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** UTIME: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "utime: you don't have permission on %s\n", path);
        return -ENOENT;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (utime(newpath,times) == 0) return 0;
    return -errno;
}

int fs_real_setxattr(const char *path, const char *name, const void *value,
    size_t size, int options)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** SETXATTR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "setxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (setxattr(newpath,name,value,size,options) == 0) return 0;
    return -errno;
}

int fs_real_getxattr(const char *path, const char *name, void *value, size_t size)
{
    char ppath[2*FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    int retval;
    if (extern_debug) printf("****** GETXATTR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        //fprintf(stderr, "getxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
    {
        retval = getxattr(newpath, name, value, size);
        if (retval == -1) return -errno;
        return retval;
    }

    retval = getxattr(path, name, value, size);
    if (retval == -1) return -errno;
    return retval;
}

int fs_real_listxattr(const char *path, char *list, size_t size)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** LISTXATTR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "listxattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == 0)
    {
        if (listxattr(newpath,list,size) == 0) return 0;
        return -errno;
    }
    if (listxattr(path,list,size) == 0) return 0;
    return -errno;
}

int fs_real_removexattr(const char *path, const char *name)
{
    char ppath[FS_MAXPATH];
    struct stat v_stbuf;
    char *newpath=ppath;
    if (extern_debug) printf("****** REMOVEXATTR: (%s)\n", path);
    isofs_inode *inode = fs_lookup(path);
    if (inode)
    {
        fprintf(stderr, "removexattr: you don't have permission on %s\n", path);
        return -ENOTSUP;
    }

    if (fs_home_stat(path,&newpath,&v_stbuf) == -1) fs_file_dup(path,newpath);
    if (removexattr(newpath,name) == 0) return 0;
    return -errno;
}
