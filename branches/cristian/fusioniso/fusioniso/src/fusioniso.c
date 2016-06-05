/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2005-2006 by Dmitry Morozhnikov <dmiceman@mail.ru>
 *   Original version (http://ubiz.ru/dm/)
 *
 *   Copyright (C) 2006-2008 by Lionel Tricon <lionel.tricon@free.fr>
 *   Klik2 support - union-fuse + sandbox feature
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
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * -----------------------------------------------------------------------------
 */

/*
 *  FAKECHROOT_EXCLUDE_PATH : list of directories to exclude from the jail (used by -bBd)
 *  FAKECHROOT_BASE_ROOT : location where all mount points are located
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mntent.h> // (set|add|end)mntent
#include <pwd.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/param.h>
#include <sys/types.h>

#include "fusioniso_debug.h"
#include "fusioniso.h"

#ifdef __GNUC__
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif


char* normalize_name(const char* fname)
{
    char* abs_fname = (char *) malloc(PATH_MAX);
    realpath(fname, abs_fname);
    // ignore errors from realpath()
    return abs_fname;
}

#if 0
int check_mount_point()
{
    struct stat st;
    int rc = lstat(extern_mount_point, &st);
    if (rc == -1 && errno == ENOENT)
    {
        // directory does not exists, createcontext
        rc = mkdir(extern_mount_point, 0777);                         // let`s underlying filesystem manage permissions
        if (rc != 0)
        {
            FSWARNING("can\'t create mount point %s: %s",extern_mount_point,strerror(errno));
            return -EIO;
        }
    }
    else if (rc == -1)
    {
        FSWARNING("can\'t check mount point %s: %s",extern_mount_point,strerror(errno));
        return -1;
    }
    return 0;
}

void del_mount_point()
{
    int rc = rmdir(extern_mount_point);
    if (rc != 0) FSWARNING("can\'t delete mount point %s: %s",extern_mount_point,strerror(errno));
}

char* get_mtab_path()
{
    char* mtab_path = (char*) malloc(PATH_MAX);
    uid_t uid = getuid();
    struct passwd* passwd = getpwuid(uid);
    if (!passwd)
    {
        FSWARNING("can\'t get home directory for user %d: %s",uid,strerror(errno));
        return NULL;
    }
    mtab_path[0] = 0;
    if (passwd->pw_dir)                                               // may be NULL, who know..
    {
        strncpy(mtab_path, passwd->pw_dir, PATH_MAX - 16);
        mtab_path[PATH_MAX - 1] = 0;
    }
    strcat(mtab_path, "/.mtab.fusioniso");
    return mtab_path;
}

int add_mtab_record()
{
    int rc;
    char* mtab_path = get_mtab_path();
    if (!mtab_path)
    {
        return -EIO;
    }
    int fd = open(mtab_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        FSWARNING("can\'t open mtab %s: %s",mtab_path,strerror(errno));
        return -EIO;
    }
    rc = lockf(fd, F_LOCK, 0);
    if (rc != 0)
    {
        FSWARNING("can\'t lock mtab %s: %s",mtab_path,strerror(errno));
        return -EIO;
    }
    FILE* mtab = setmntent(mtab_path, "a");
    if (!mtab)
    {
        FSWARNING("can\'t open mtab %s",mtab_path);
        return -EIO;
    }
    struct mntent ent;
    ent.mnt_fsname = imagefile;
    ent.mnt_dir = extern_mount_point;
    ent.mnt_type = "fusioniso";
    ent.mnt_opts = "defaults";
    ent.mnt_freq = 0;
    ent.mnt_passno = 0;
    rc = addmntent(mtab, &ent);
    if (rc != 0)
    {
        FSWARNING("can\'t add mtab entry to %s",mtab_path);
        return -EIO;
    }
    endmntent(mtab);
    rc = lockf(fd, F_ULOCK, 0);
    if (rc != 0)
    {
        FSWARNING("can\'t unlock mtab %s: %s",mtab_path,strerror(errno));
        return -EIO;
    }
    close(fd);
    free(mtab_path);
    return 0;
}

int del_mtab_record()
{
    int rc;
    char* mtab_path = get_mtab_path();
    char new_mtab_path[PATH_MAX];
    if (!mtab_path)
    {
        return -EIO;
    }
    int fd = open(mtab_path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        FSWARNING("can\'t open mtab %s: %s",mtab_path,strerror(errno));
        return -EIO;
    }
    rc = lockf(fd, F_LOCK, 0);
    if (rc != 0)
    {
        FSWARNING("can\'t lock mtab %s: %s",mtab_path,strerror(errno));
        return -EIO;
    }
    strncpy(new_mtab_path, mtab_path, PATH_MAX - 16);
    new_mtab_path[PATH_MAX - 1] = 0;
    strcat(new_mtab_path, ".new");
    FILE* mtab = setmntent(mtab_path, "r");
    if (!mtab)
    {
        FSWARNING("can\'t open mtab %s",mtab_path);
        return -EIO;
    }
    FILE* new_mtab = setmntent(new_mtab_path, "a+");
    if (!new_mtab)
    {
        FSWARNING("can\'t open new mtab %s",new_mtab_path);
        return -EIO;
    }
    struct mntent* ent;
    while ((ent = getmntent(mtab)) != NULL)
    {
        if (strcmp(ent->mnt_fsname, imagefile) == 0 &&
            strcmp(ent->mnt_dir, extern_mount_point) == 0 &&
            strcmp(ent->mnt_type, "fusioniso") == 0)
        {
            // skip
        }
        else
        {
            rc = addmntent(new_mtab, ent);
            if (rc != 0)
            {
                FSWARNING("can\'t add mtab entry to %s",new_mtab_path);
                return -EIO;
            }
        }
    }
    endmntent(mtab);
    endmntent(new_mtab);
    rc = rename(new_mtab_path, mtab_path);
    if (rc != 0)
    {
        FSWARNING("can\'t rewrite mtab from %s to %s: %s",new_mtab_path,mtab_path,strerror(errno));
        return -EIO;
    }
    rc = lockf(fd, F_ULOCK, 0);
    if (rc != 0)
    {
        FSWARNING("can\'t unlock mtab %s",mtab_path);
        return -EIO;
    }
    close(fd);
    free(mtab_path);
    return 0;
}
#endif

static int fs_getattr(const char *path, struct stat *stbuf)
{
    return fs_real_getattr(path, stbuf);
}


static int fs_readlink(const char *path, char *target, size_t size)
{
    return fs_real_readlink(path, target, size);
}

static int fs_open(const char *path, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_open(path);
}


static int fs_read(const char *path, char *buf, size_t size,
off_t offset, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_read(path, buf, size, offset);
}


static int fs_write(const char *path, const char *buf, size_t size,
off_t offset, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_write(path, buf, size, offset);
}

#ifdef HAVE_SETXATTR
static int fs_setxattr(const char *path, const char *name,
const char *value, size_t size, int options)
{
    return fs_real_setxattr(path, name, value, size, options);
}


static int fs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return fs_real_getxattr(path, name, value, size);
}


static int fs_listxattr(const char *path, char *list, size_t size)
{
    return fs_real_listxattr(path, list, size);
}


static int fs_removexattr(const char *path, const char *name)
{
    return fs_real_removexattr(path, name);
}
#endif

static int fs_access(const char *path, int amode)
{
    return fs_real_access(path, amode);
}


static int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    return fs_real_mknod(path, mode, dev);
}


static int fs_unlink(const char *path)
{
    return fs_real_unlink(path);
}


static int fs_rmdir(const char *path)
{
    return fs_real_rmdir(path);
}


static int fs_mkdir(const char *path, mode_t mode)
{
    return fs_real_mkdir(path, mode);
}


static int fs_symlink(const char *path1, const char *path2)
{
    return fs_real_symlink(path1, path2);
}


static int fs_rename(const char *oldpath, const char *newpath)
{
    return fs_real_rename(oldpath, newpath);
}


static int fs_link(const char *oldpath, const char *newpath)
{
    return fs_real_link(oldpath, newpath);
}


static int fs_chmod(const char *path, mode_t mode)
{
    return fs_real_chmod(path, mode);
}


static int fs_chown(const char *path, uid_t owner, gid_t group)
{
    return fs_real_chown(path, owner, group);
}


static int fs_truncate(const char *path, off_t length)
{
    return fs_real_truncate(path, length);
}


static int fs_utime(const char *path, struct utimbuf *times)
{
    return fs_real_utime(path, times);
}


static int fs_flush(const char *UNUSED(path), struct fuse_file_info *UNUSED(fi))
{
    return 0;
}

#if 0
static void* fs_init()
{
    int rc;
    if (maintain_mtab)
    {
        rc = add_mtab_record();
        if (rc != 0)
        {
            exit(EXIT_FAILURE);
        }
    }
    return isofs_real_init();
}

static void fs_destroy(void* param)
{
    if (maintain_mtab) del_mtab_record();
    return;
}
#endif

static int fs_opendir(const char *path, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_opendir(path);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t UNUSED(offset),
struct fuse_file_info *UNUSED(fi))
{
    return fs_real_readdir(1, path, buf, filler);
}

static int fs_statfs(const char *path, struct statfs *stbuf)
{
    return fs_real_statfs(path, stbuf);
}


static void *fs_init(struct fuse_conn_info *conn)
{
	struct fuse_context *mycontext=fuse_get_context();
	return mycontext->private_data;
}


static struct fuse_operations fs_oper = {
    .getattr        = fs_getattr,
    .readlink       = fs_readlink,
    .open           = fs_open,
    .read           = fs_read,
    .flush          = fs_flush,
    .init           = fs_init,
    //.destroy        = fs_destroy,
    .opendir        = fs_opendir,
    .readdir        = fs_readdir,
    .statfs         = fs_statfs,
};

static struct fuse_operations fs_oper_union = {
    .getattr		= fs_getattr,
    .readlink		= fs_readlink,
    .open		= fs_open,
    .read		= fs_read,
    .flush		= fs_flush,
    .init		= fs_init,
    //.destroy		= fs_destroy,
    .opendir		= fs_opendir,
    .readdir		= fs_readdir,
    .statfs		= fs_statfs,
    .write		= fs_write,
    .mknod		= fs_mknod,
    .unlink		= fs_unlink,
    .rmdir		= fs_rmdir,
    .mkdir		= fs_mkdir,
    .symlink		= fs_symlink,
    .rename		= fs_rename,
    .link		= fs_link,
    .chmod		= fs_chmod,
    .chown		= fs_chown,
    .truncate		= fs_truncate,
    .utime		= fs_utime,
#ifdef HAVE_SETXATTR
    .setxattr		= fs_setxattr,
    .getxattr		= fs_getxattr,
    .listxattr		= fs_listxattr,
    .removexattr	= fs_removexattr,
#endif
    .access		= fs_access,
};

void usage(const char* prog)
{
    printf("Version: %s\nUsage: %s [-nputgh] [-b|d|B <sandbox_path>] [-c <iocharset>] <isofs_image_file> <mount_point> [<FUSE library options>]\n"
   "Where options are:\n"
    "    -n                       -- do NOT maintain file $HOME/.mtab.fusioniso\n"
    "    -p                       -- maintain mount point; \n"
    "                                create it if it does not exists and delete it on exit\n"
    "    -c <iocharset>           -- specify iocharset for Joliet filesystem\n"
    "    -h                       -- print this screen\n"
    "    -t                       -- debug mode\n"
    "    -g                       -- change the loader on the fly to ld-klik2.so.2 [Experimental]\n"
    "    -u                       -- union mounting feature for klik2 (forced if -b, -d or -B)\n"
    "    -b <path>                -- sandbox feature for klik2 (redirect all modification from $HOME user to <path>)\n"
    "    -d <path>                -- sandbox feature for klik2 (redirect all modification from $HOME user dot files and folders to <path>)\n"
    "    -B <path>                -- sandbox feature for klik2 (redirect all modification to <path>) [Experimental]\n"
    "\nCommon FUSE library options are:\n"
    "    -f     -- run in foreground, do not daemonize\n"
    "    -d     -- run in foreground and print debug information\n"
    "    -s     -- run single-threaded\n"
    "\nPlease consult with FUSE documentation for more information\n",
    VERSION,
    prog);
}

int main(int argc, char *argv[])
{
    int i, j, rc;
    char c, *imagefile, *env, *pw_dir=NULL;
    struct passwd* passwd=NULL;
    fusioniso_data *data;
    isofs_inode *root_inode;

    setlocale(LC_ALL, "");  // set current locale for proper iocharset

    data = malloc(sizeof(fusioniso_data));
    if (!data) {
        FSWARNING("can\'t malloc: %s",strerror(errno));
	exit(EXIT_FAILURE);
    }

    // defaults
    data->maintain_mountpoint=0;
    data->maintain_mtab=1;
    data->iocharset=NULL;
    data->union_mounting=false;
    data->sandbox_mounting=sandbox_none;
    data->glibc=false;
    data->lookup_table          = g_hash_table_new(g_str_hash, g_str_equal);
    data->real_lookup_table     = g_hash_table_new(g_str_hash, g_str_equal);
    data->removed_lookup_table  = g_hash_table_new(g_str_hash, g_str_equal);
    data->negative_lookup_table = g_hash_table_new(g_str_hash, g_str_equal);

    // Home directory
    passwd = getpwuid(getuid());
    if (passwd && passwd->pw_dir)
    {
        pw_dir = strdup(passwd->pw_dir);
    }

    while ((c = getopt(argc, argv, "+npuht:gc:b:B:d:")) > 0)
    {
        switch (c)
        {
            case 'g':
                data->glibc = true;
                break;
#ifdef _DEBUG
            case 't':
		if (optarg)
			if (debug_outfile(optarg)<0)
				exit(EXIT_FAILURE);
                break;
#endif
            case 'n':
                data->maintain_mtab = 0;
                break;

            case 'p':
                data->maintain_mountpoint = 1;
                break;

            case 'c':
                if (optarg) data->iocharset=optarg;
                break;

            case 'h':
                usage(argv[0]);
                exit(0);
                break;

            case 'u':
                data->union_mounting = true;
                break;

            case 'b':
                data->union_mounting = true;
                data->sandbox_mounting = sandbox_home;
                if (optarg)
                {
                    data->sandbox_path = optarg;
                    data->sandbox_path_size = strlen(data->sandbox_path);
                    if (pw_dir != NULL)
                    {
                        data->redirect_path_size = strlen(pw_dir);
                        data->redirect_path = malloc(data->redirect_path_size+1);
                        strcpy(data->redirect_path, pw_dir);
                    }
                }
                break;

            case 'd':
                data->union_mounting = true;
                data->sandbox_mounting = sandbox_portable;
                if (optarg)
                {
                    data->sandbox_path=optarg;
                    data->sandbox_path_size = strlen(data->sandbox_path);
                    if (pw_dir != NULL)
                    {
                        data->redirect_path_size = strlen(pw_dir);
                        data->redirect_path = malloc(data->redirect_path_size+3);
                        strcpy(data->redirect_path, pw_dir);
                        strcat(data->redirect_path,  "/.");
                    }
                }
                break;

            case 'B':
                data->union_mounting = true;
                data->sandbox_mounting = sandbox_data;
                if (optarg)
                {
                    data->sandbox_path=optarg;
                    data->sandbox_path_size = strlen(data->sandbox_path);
                    data->redirect_path = malloc(2);
                    data->redirect_path_size = 1;
                    strcpy(data->redirect_path, "/");
                }
                break;
            case '?':
            case ':':
                usage(argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }

    if ((argc-optind) < 2)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    imagefile = normalize_name(argv[optind]);
    data->mountpoint = normalize_name(argv[optind + 1]);

    // with space for possible -o use_ino arguments
    char **nargv = (char **) malloc((argc + 2) * sizeof(char *));
    int nargc = argc - optind;

    nargv[0] = argv[0];

    int next_opt = 0;
    int use_ino_found = 0;
    for (i = 0; i < nargc - 1; ++i)
    {
        if (next_opt && !use_ino_found)
        {
            if (strstr(argv[i + optind + 1], "use_ino"))              // ok, already there
            {
                use_ino_found = 1;
                nargv[i + 1] = argv[i + optind + 1];
            }                                                         // add it
            else
            {
                char* str = (char*) malloc(strlen(argv[i + optind + 1]) + 10);
                strcpy(str, argv[i + optind + 1]);
                strcat(str, ",use_ino");
                nargv[i + 1] = str;
                use_ino_found = 1;
            }
        }
        else
        {
            nargv[i + 1] = argv[i + optind + 1];
        }
        // check if this is -o string mean that next argument should be options string
        if (i > 1 && nargv[i + 1][0] == '-' && nargv[i + 1][1] == 'o') next_opt=1;
    }
    if (!use_ino_found)
    {
        nargv[nargc] = "-o";
        nargc++;
        nargv[nargc] = "use_ino";
        nargc++;
    }

    if (!data->iocharset)
    {
        char *nlcharset = nl_langinfo(CODESET);
        if (nlcharset)
        {
            data->iocharset = (char *) malloc(strlen(nlcharset) + 9);
            strcpy(data->iocharset, nlcharset);
            strcat(data->iocharset, "//IGNORE");
        }
        if (!data->iocharset) data->iocharset="UTF-8//IGNORE";
    }

#if 0
    int rc;
    if (maintain_mount_point)
    {
        rc = check_mount_point();
        if (rc != 0) exit(EXIT_FAILURE);
    }
    if (maintain_mount_point)
    {
        rc = atexit(del_mount_point);
        if (rc != 0)
        {
            FSWARNING("can\'t set exit function");
            exit(EXIT_FAILURE);
        }
    }
#endif

    // will exit in case of failure
    rc=isofs_init(imagefile,&data->isofs);
    if (rc<0)
        exit(EXIT_FAILURE);

    root_inode=(isofs_inode *) malloc(sizeof(isofs_inode));
    if (!root_inode) {
        FSWARNING("can\'t malloc: %s",strerror(errno));
	exit(EXIT_FAILURE);
    }

    memset(root_inode,0,sizeof(isofs_inode));
    root_inode->record=data->isofs->root;
    root_inode->st_ino=1;
    g_hash_table_insert(data->lookup_table,(char*) "/",root_inode);
    data->isofs->last_ino=2;

    // We get a list of directories or files
    env = getenv("FAKECHROOT_EXCLUDE_PATH");
    if (env)
    {
        for (data->exclude_list_max=0,i=0; data->exclude_list_max<32; )
        {
            for (j=i; env[j]!=':'&&env[j]!='\0'; j++);
            data->exclude_list[data->exclude_list_max] = malloc(j-i+2);
            memset(data->exclude_list[data->exclude_list_max], '\0', j-i+2);
            strncpy(data->exclude_list[data->exclude_list_max], &(env[i]), j-i);
            data->exclude_list_size[data->exclude_list_max] = strlen(data->exclude_list[data->exclude_list_max]);
            data->exclude_list_max++;
            if (env[j] != ':') break;
            i=j+1;
        }
    }

    // We don't want to loop into ourself
    data->base_root = getenv("FAKECHROOT_BASE_ROOT");
    if (data->base_root == NULL) {
        data->base_root = (char*)malloc(10);
        strcpy(data->base_root, "/tmp/klik");
    }
    data->base_root_size = strlen(data->base_root);

    if (data->union_mounting) return fuse_main(nargc, nargv, &fs_oper_union, data);
    return fuse_main(nargc, nargv, &fs_oper, data);
}
