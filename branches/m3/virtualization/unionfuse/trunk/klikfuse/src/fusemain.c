/*
 * -----------------------------------------------------------------------------
 *
 *   Copyright (C) 2005, 2006 by Dmitry Morozhnikov <dmiceman@mail.ru>
 *   Original version (http://ubiz.ru/dm/)
 *
 *   Copyright (C) 2006, 2007 by Lionel Tricon <lionel.tricon@free.fr>
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
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * -----------------------------------------------------------------------------
 */

/*
 * ENVIRONMENT VARIABLE
 *
 *  FAKECHROOT_EXCLUDE_PATHS -> list of directories to exclude from the jail (used by -bB)
 *
 */

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

#define FUSE_USE_VERSION 25
#include <fuse.h>

#include <zlib.h>
#include <locale.h>
#include <langinfo.h>
#include <linux/iso_fs.h>

#ifdef _ISOFS
#include "fs.h"
#include "isofs.h"
#else
#include "fs.h"
#include "cramfs.h"
#endif

#ifdef __GNUC__
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

typedef struct fuse_file_info struct_file_info;

static char *imagefile = NULL;
static char *mount_point = NULL;
static int image_fd = -1;

int maintain_mount_point;
int maintain_mtab;
char *iocharset;

bool _union_mounting;                                                 // if true, we want to union-mount the data
bool _sandbox_mounting;                                               // if true, we want to sandbox the application
char* _redirect_path;                                                 // ~$HOME directory
int   _size_redirect_path;                                            // Size of the ~$HOME directory
char* _sandbox_path;                                                  // Directory where to redirect write (sandboxing)
int   _size_sandbox_path;                                             // Size of the directory where to redirect write
char* _exclude_list[32];                                              // We want to exclude some directories in case of sandboxing
int   _exclude_length[32];                                            // Size of each directory to exclude (exclude list)
int   _list_max=0;                                                    // Number of directories (exclude list)

char* normalize_name(const char* fname)
{
    char* abs_fname = (char *) malloc(PATH_MAX);
    realpath(fname, abs_fname);
    // ignore errors from realpath()
    return abs_fname;
}

int check_mount_point()
{
    struct stat st;
    int rc = lstat(mount_point, &st);
    if (rc == -1 && errno == ENOENT)
    {
        // directory does not exists, createcontext
        rc = mkdir(mount_point, 0777);                                // let`s underlying filesystem manage permissions
        if (rc != 0)
        {
            perror("Can`t create mount point");
            return -EIO;
        }
    }
    else if (rc == -1)
    {
        perror("Can`t check mount point");
        return -1;
    }
    return 0;
}

void del_mount_point()
{
    int rc = rmdir(mount_point);
    if (rc != 0) perror("Can`t delete mount point");
}

char* get_mtab_path()
{
    char* mtab_path = (char*) malloc(PATH_MAX);
    uid_t uid = getuid();
    struct passwd* passwd = getpwuid(uid);
    if (!passwd)
    {
        fprintf(stderr, "Can`t get home directory for user %d: %s\n", uid, strerror(errno));
        return NULL;
    }
    mtab_path[0] = 0;
    if (passwd->pw_dir)                                               // may be NULL, who know..
    {
        strncpy(mtab_path, passwd->pw_dir, PATH_MAX - 16);
        mtab_path[PATH_MAX - 1] = 0;
    }
#ifdef _ISOFS
    strcat(mtab_path, "/.mtab.fuseiso");
#else
    strcat(mtab_path, "/.mtab.fusecram");
#endif
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
        perror("Can`t open mtab");
        return -EIO;
    }
    rc = lockf(fd, F_LOCK, 0);
    if (rc != 0)
    {
        perror("Can`t lock mtab");
        return -EIO;
    }
    FILE* mtab = setmntent(mtab_path, "a");
    if (!mtab)
    {
        perror("Can`t open mtab");
        return -EIO;
    }
    struct mntent ent;
    ent.mnt_fsname = imagefile;
    ent.mnt_dir = mount_point;
#ifdef _ISOFS
    ent.mnt_type = "fuseiso";
#else
    ent.mnt_type = "fusecram";
#endif
    ent.mnt_opts = "defaults";
    ent.mnt_freq = 0;
    ent.mnt_passno = 0;
    rc = addmntent(mtab, &ent);
    if (rc != 0)
    {
        perror("Can`t add mtab entry");
        return -EIO;
    }
    endmntent(mtab);
    rc = lockf(fd, F_ULOCK, 0);
    if (rc != 0)
    {
        perror("Can`t unlock mtab");
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
        perror("Can`t open mtab");
        return -EIO;
    }
    rc = lockf(fd, F_LOCK, 0);
    if (rc != 0)
    {
        perror("Can`t lock mtab");
        return -EIO;
    }
    strncpy(new_mtab_path, mtab_path, PATH_MAX - 16);
    new_mtab_path[PATH_MAX - 1] = 0;
    strcat(new_mtab_path, ".new");
    FILE* mtab = setmntent(mtab_path, "r");
    if (!mtab)
    {
        perror("Can`t open mtab");
        return -EIO;
    }
    FILE* new_mtab = setmntent(new_mtab_path, "a+");
    if (!new_mtab)
    {
        perror("Can`t open new mtab");
        return -EIO;
    }
    struct mntent* ent;
    while ((ent = getmntent(mtab)) != NULL)
    {
        if (strcmp(ent->mnt_fsname, imagefile) == 0 &&
            strcmp(ent->mnt_dir, mount_point) == 0 &&
#ifdef _ISOFS
        strcmp(ent->mnt_type, "fuseiso") == 0)
        {
#else
        strcmp(ent->mnt_type, "fusecram") == 0)
        {
#endif
            // skip
        }
        else
        {
            rc = addmntent(new_mtab, ent);
            if (rc != 0)
            {
                perror("Can`t add mtab entry");
                return -EIO;
            }
        }
    }
    endmntent(mtab);
    endmntent(new_mtab);
    rc = rename(new_mtab_path, mtab_path);
    if (rc != 0)
    {
        perror("Can`t rewrite mtab");
        return -EIO;
    }
    rc = lockf(fd, F_ULOCK, 0);
    if (rc != 0)
    {
        perror("Can`t unlock mtab");
        return -EIO;
    }
    close(fd);
    free(mtab_path);
    return 0;
}

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


static int fs_setxattr(const char *path, const char *name,
const char *value, size_t size, int options,
struct fuse_file_info *UNUSED(fi))
{
    return fs_real_setxattr(path, name, value, size, options);
}


static int fs_getxattr(const char *path, const char *name,
char *value, size_t size, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_getxattr(path, name, value, size);
}


static int fs_listxattr(const char *path, char *list,
size_t size, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_listxattr(path, list, size);
}


static int fs_removexattr(const char *path, const char *name, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_removexattr(path, name);
}


static int fs_access(const char *path, int amode, struct fuse_file_info *UNUSED(fi))
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

#ifdef _ISOFS
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
#else
static void* fs_init()
{
    return cramfs_real_init(imagefile);
}
#endif

static void fs_destroy(void* param)
{
    if (maintain_mtab) del_mtab_record();
    return;
}

static int fs_opendir(const char *path, struct fuse_file_info *UNUSED(fi))
{
    return fs_real_opendir(path);
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t UNUSED(offset),
struct fuse_file_info *UNUSED(fi))
{
    return fs_real_readdir( 1, path, buf, filler);
}

static int fs_statfs(const char *path, struct statfs *stbuf)
{
    return fs_real_statfs( path, stbuf );
}

static struct fuse_operations fs_oper = {
    .getattr        = fs_getattr,
    .readlink       = fs_readlink,
    .open           = fs_open,
    .read           = fs_read,
    .flush          = fs_flush,
    .init           = fs_init,
    .destroy        = fs_destroy,
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
    .destroy		= fs_destroy,
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
    .setxattr		= fs_setxattr,
    .getxattr		= fs_getxattr,
    .listxattr		= fs_listxattr,
    .removexattr	= fs_removexattr,
    .access		= fs_access,
};

void usage(const char* prog)
{
#ifdef _ISOFS
    printf("Version: %s\nUsage: %s [-n] [-p] [-u] [-b|d|B <sandbox_path>] [-c <iocharset>] [-h] <isofs_image_file> <mount_point> [<FUSE library options>]\n"
#else
    printf("Version: %s\nUsage: %s [-n] [-p] [-u] [-b|d|B <sandbox_path>] [-c <iocharset>] [-h] <cramfs_image_file> <mount_point> [<FUSE library options>]\n"
#endif
   "Where options are:\n"
    "    -n                       -- do NOT maintain file $HOME/.mtab.fuseiso\n"
    "    -p                       -- maintain mount point; \n"
    "                                create it if it does not exists and delete it on exit\n"
    "    -c <iocharset>           -- specify iocharset for Joliet filesystem\n"
    "    -h                       -- print this screen\n"
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
    char *env;
    int i,j;
    struct passwd* passwd = NULL;
    setlocale(LC_ALL, "");                                        // set current locale for proper iocharset

    // defaults
    maintain_mount_point = 0;
    maintain_mtab = 1;
    _union_mounting = false;
    _sandbox_mounting = false; 
    iocharset = NULL;

    char c;
    while ((c = getopt(argc, argv, "+npuc:b:B:h:d")) > 0)
    {
        switch (c)
        {
            case 'n':
                maintain_mtab = 0;
                break;

            case 'p':
                maintain_mount_point = 1;
                break;

            case 'c':
                if (optarg) iocharset=optarg;
                break;

            case 'h':
                usage(argv[0]);
                exit(0);
                break;

            case 'u':
                _union_mounting = true;
                break;

            case 'b':
                _union_mounting = true;
                _sandbox_mounting = true;
                if (optarg)
                {
                    _sandbox_path=optarg;
                    _size_sandbox_path = strlen( _sandbox_path );
                    passwd = getpwuid( getuid() );
                    if (passwd && passwd->pw_dir)
                    {
                        _size_redirect_path = strlen( passwd->pw_dir );
                        _redirect_path = malloc( _size_redirect_path+1 );
                        strcpy( _redirect_path, passwd->pw_dir );
                    }
                }
                break;

            case 'd':
                _union_mounting = true;
                _sandbox_mounting = true;
                if (optarg)
                {
                    _sandbox_path=optarg;
                    _size_sandbox_path = strlen( _sandbox_path );
                    passwd = getpwuid( getuid() );
                    if (passwd && passwd->pw_dir)
                    {
                        _size_redirect_path = strlen( passwd->pw_dir );
                        _redirect_path = malloc( _size_redirect_path+3 );
                         strcpy( _redirect_path, passwd->pw_dir );
                         strcat( _redirect_path,  "/." );
                    }
                }
                break;

            case 'B':
                _union_mounting = true;
                _sandbox_mounting = true;
                if (optarg)
                {
                    _sandbox_path=optarg;
                    _size_sandbox_path = strlen( _sandbox_path );
                    _redirect_path = malloc( 2 );
                    _size_redirect_path = 1;
                    strcpy( _redirect_path, "/" );
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

    image_fd = open(imagefile, O_RDONLY);
    if (image_fd == -1)
    {
        fprintf(stderr, "Supplied image file name: \"%s\"\n", imagefile);
        perror("Can`t open image file");
        exit(EXIT_FAILURE);
    }

    mount_point = normalize_name(argv[optind + 1]);

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

    if (!iocharset)
    {
        char *nlcharset = nl_langinfo(CODESET);
        if (nlcharset)
        {
            iocharset = (char *) malloc(strlen(nlcharset) + 9);
            strcpy(iocharset, nlcharset);
            strcat(iocharset, "//IGNORE");
        }
        if (!iocharset) iocharset="UTF-8//IGNORE";
    }

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
            fprintf(stderr, "Can`t set exit function\n");
            exit(EXIT_FAILURE);
        }
    }

#ifdef _ISOFS
    // will exit in case of failure
    rc = isofs_real_preinit(imagefile, image_fd);
#endif

    // We get a list of directories or files
    env = getenv("FAKECHROOT_EXCLUDE_PATHS");
    if (env)
    {
        for (i=0; _list_max<32;)
        {
            for (j=i; env[j]!=':'&&env[j]!='\0'; j++);
            _exclude_list[_list_max] = malloc(j-i+2);
            memset(_exclude_list[_list_max], '\0', j-i+2);
            strncpy(_exclude_list[_list_max], &(env[i]), j-i);
            _exclude_length[_list_max] = strlen(_exclude_list[_list_max]);
            _list_max++;
            if (env[j] != ':') break;
            i=j+1;
        }
    }

    if (_union_mounting) return fuse_main(nargc, nargv, &fs_oper_union);
    return fuse_main(nargc, nargv, &fs_oper);
}
