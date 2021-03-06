/*
 *  libfakechroot -- fake chroot environment
 *  (c) 2003-2005 Piotr Roszatycki <dexter@debian.org>, LGPL
 *
 *  klik2 support -- allow direct access to a list of directories
 *  (c) 2006-2007 Lionel Tricon <lionel.tricon@free.fr>, LGPL
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
*/

#define DEBUG 0

/*
 * ENVIRONMENT VARIABLE
 *
 *  FAKECHROOT_BASE -> path of the jail
 *  FAKECHROOT_WORKING_DIRECTORY -> working directory
 *  FAKECHROOT_EXCLUDE_PATHS -> list of directories to exclude from the jail
 *  KLIK_APPRUN -> name of the application running into the jail
 *
 */

#include <config.h>

#define _GNU_SOURCE
#define __BSD_VISIBLE

#include <pwd.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <glob.h>
#include <utime.h>
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#ifdef HAVE_FTW_H
#include <ftw.h>
#endif
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#if defined(PATH_MAX)
#define FAKECHROOT_MAXPATH PATH_MAX
#elif defined(_POSIX_PATH_MAX)
#define FAKECHROOT_MAXPATH _POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define FAKECHROOT_MAXPATH MAXPATHLEN
#else
#define FAKECHROOT_MAXPATH 4096
#endif

#define narrow_chroot_path(path, fakechroot_path, fakechroot_ptr) \
{ \
    if ((path) != NULL && *((char *)(path)) != '\0') \
    { \
        fakechroot_path = getenv("FAKECHROOT_BASE"); \
        if (fakechroot_path != NULL) \
        { \
            fakechroot_ptr = strstr((path), fakechroot_path); \
            if (fakechroot_ptr == (path)) \
            { \
                if (strlen((path)) == strlen(fakechroot_path)) \
                { \
                    ((char *)(path))[0] = '/'; \
                    ((char *)(path))[1] = '\0'; \
                } \
                else \
                { \
                    (path) = ((path) + strlen(fakechroot_path)); \
                } \
            } \
        } \
    } \
}

#define expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf) \
{ \
    if ((path) != NULL && *((char *)(path)) == '/') \
    { \
        fakechroot_path = getenv("FAKECHROOT_BASE"); \
        if (fakechroot_path != NULL) \
        { \
            fakechroot_ptr = strstr((path), fakechroot_path); \
            if (fakechroot_ptr != (path)) \
            { \
                strcpy(fakechroot_buf, fakechroot_path); \
                strcat(fakechroot_buf, (path)); \
                (path) = fakechroot_buf; \
            } \
        } \
    } \
}

//                fakechroot_ptr = malloc(FAKECHROOT_MAXPATH); \
//                strcpy((char*)fakechroot_ptr, fakechroot_buf); \
//                (path) = fakechroot_ptr; \

#define expand_chroot_path_malloc(path, fakechroot_path, fakechroot_ptr, fakechroot_buf) \
{ \
    if ((path) != NULL && *((char *)(path)) == '/') \
    { \
        fakechroot_path = getenv("FAKECHROOT_BASE"); \
        if (fakechroot_path != NULL) \
        { \
            fakechroot_ptr = strstr((path), fakechroot_path); \
            if (fakechroot_ptr != (path)) \
            { \
                if ((fakechroot_buf = malloc(strlen(fakechroot_path)+strlen(path)+1)) == NULL) \
                { \
                    errno = ENOMEM; \
                    return NULL; \
                } \
                strcpy(fakechroot_buf, fakechroot_path); \
                strcat(fakechroot_buf, (path)); \
                (path) = fakechroot_buf; \
            } \
        } \
    } \
}

#define nextsym(function, name) \
{ \
    char *msg; \
    if (next_##function == NULL) \
    { \
        *(void **)(&next_##function) = dlsym(RTLD_NEXT, name); \
        if ((msg = dlerror()) != NULL) \
        { \
            fprintf(stderr, "%s: dlsym(%s): %s\n", PACKAGE, name, msg); \
        } \
    } \
}

#ifndef __GLIBC__
                                                        extern char **environ;
#endif

// Usefull to exclude a list of directories or files
char *_exclude_list[32];
int _exclude_length[32];
int _list_max = 0;
int _first = 0;
char *_home_path = NULL;

#ifndef HAVE_STRCHRNUL
/* Find the first occurrence of C in S or the final NUL byte.  */
static char *strchrnul(const char *s, int c_in)
{
    const unsigned char *char_ptr;
    const unsigned long int *longword_ptr;
    unsigned long int longword, magic_bits, charmask;
    unsigned char c;

    c = (unsigned char) c_in;

    /* Handle the first few characters by reading one character at a time.
                   Do this until CHAR_PTR is aligned on a longword boundary.  */
    for (char_ptr=s; ((unsigned long int) char_ptr
        & (sizeof(longword) - 1)) != 0; ++char_ptr)
        if (*char_ptr == c || *char_ptr == '\0')
            return (void *) char_ptr;

    /* All these elucidatory comments refer to 4-byte longwords,
                   but the theory applies equally well to 8-byte longwords.  */

    longword_ptr = (unsigned long int *) char_ptr;

    /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
                   the "holes."  Note that there is a hole just to the left of
                   each byte, with an extra at the end:

                   bits:  01111110 11111110 11111110 11111111
                   bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

                   The 1-bits make sure that carries propagate to the next 0-bit.
                   The 0-bits provide holes for carries to fall into.  */
    switch (sizeof(longword))
    {
        case 4:
            magic_bits = 0x7efefeffL;
            break;
        case 8:
            magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL;
            break;
        default:
            abort();
    }

    /* Set up a longword, each of whose bytes is C.  */
    charmask = c | (c << 8);
    charmask |= charmask << 16;
    if (sizeof(longword) > 4)
        /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
        charmask |= (charmask << 16) << 16;
    if (sizeof(longword) > 8)
        abort();

    /* Instead of the traditional loop which tests each character,
                   we will test a longword at a time.  The tricky part is testing
                   if *any of the four* bytes in the longword in question are zero.  */
    for (;;)
    {
        /* We tentatively exit the loop if adding MAGIC_BITS to
                               LONGWORD fails to change any of the hole bits of LONGWORD.

                               1) Is this safe?  Will it catch all the zero bytes?
                               Suppose there is a byte with all zeros.  Any carry bits
                               propagating from its left will fall into the hole at its
                               least significant bit and stop.  Since there will be no
                               carry from its most significant bit, the LSB of the
                               byte to the left will be unchanged, and the zero will be
                               detected.

                               2) Is this worthwhile?  Will it ignore everything except
                               zero bytes?  Suppose every byte of LONGWORD has a bit set
                               somewhere.  There will be a carry into bit 8.  If bit 8
                               is set, this will carry into bit 16.  If bit 8 is clear,
                               one of bits 9-15 must be set, so there will be a carry
                               into bit 16.  Similarly, there will be a carry into bit
                               24.  If one of bits 24-30 is set, there will be a carry
                               into bit 31, so all of the hole bits will be changed.

                               The one misfire occurs when bits 24-30 are clear and bit
                               31 is set; in this case, the hole at bit 31 is not
                               changed.  If we had access to the processor carry flag,
                               we could close this loophole by putting the fourth hole
                               at bit 32!

                               So it ignores everything except 128's, when they're aligned
                               properly.

                               3) But wait!  Aren't we looking for C as well as zero?
                               Good point.  So what we do is XOR LONGWORD with a longword,
                               each of whose bytes is C.  This turns each byte that is C
                               into a zero.  */

        longword = *longword_ptr++;

        /* Add MAGIC_BITS to LONGWORD.  */
        if ((((longword + magic_bits)

            /* Set those bits that were unchanged by the addition.  */
            ^ ~longword)

            /* Look at only the hole bits.  If any of the hole bits
                                                are unchanged, most likely one of the bytes was a
                                                zero.  */
            & ~magic_bits) != 0 ||
            /* That caught zeroes.  Now test for C.  */
            ((((longword ^ charmask) +
            magic_bits) ^ ~(longword ^ charmask))
            & ~magic_bits) != 0)
        {
            /* Which of the bytes was C or zero?
                                               If none of them were, it was a misfire; continue the search.  */

            const unsigned char *cp =
                (const unsigned char *) (longword_ptr - 1);

            if (*cp == c || *cp == '\0')
                return (char *) cp;
            if (*++cp == c || *cp == '\0')
                return (char *) cp;
            if (*++cp == c || *cp == '\0')
                return (char *) cp;
            if (*++cp == c || *cp == '\0')
                return (char *) cp;
            if (sizeof(longword) > 4)
            {
                if (*++cp == c || *cp == '\0')
                    return (char *) cp;
                if (*++cp == c || *cp == '\0')
                    return (char *) cp;
                if (*++cp == c || *cp == '\0')
                    return (char *) cp;
                if (*++cp == c || *cp == '\0')
                    return (char *) cp;
            }
        }
    }

    /* This should never happen.  */
    return NULL;
}
#endif

#ifdef HAVE___LXSTAT
static int     (*next___lxstat) (int ver, const char *filename, struct stat *buf) = NULL;
#endif
#ifdef HAVE___LXSTAT64
static int     (*next___lxstat64) (int ver, const char *filename, struct stat64 *buf) = NULL;
#endif
#ifdef HAVE___OPEN
static int     (*next___open) (const char *pathname, int flags, ...) = NULL;
#endif
#ifdef HAVE___OPEN64
static int     (*next___open64) (const char *pathname, int flags, ...) = NULL;
#endif
#ifdef HAVE___OPENDIR2
static DIR *   (*next___opendir2) (const char *name, int flags) = NULL;
#endif
#ifdef HAVE___XMKNOD
static int     (*next___xmknod) (int ver, const char *path, mode_t mode, dev_t *dev) = NULL;
#endif
#ifdef HAVE___XSTAT
static int     (*next___xstat) (int ver, const char *filename, struct stat *buf) = NULL;
#endif
#ifdef HAVE___XSTAT64
static int     (*next___xstat64) (int ver, const char *filename, struct stat64 *buf) = NULL;
#endif
#ifdef HAVE__XFTW
static int     (*next__xftw) (int mode, const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd) = NULL;
#endif
#ifdef HAVE__XFTW64
static int     (*next__xftw64) (int mode, const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd) = NULL;
#endif
static int     (*next_access) (const char *pathname, int mode) = NULL;
static int     (*next_acct) (const char *filename) = NULL;
#ifdef HAVE_CANONICALIZE_FILE_NAME
static char *  (*next_canonicalize_file_name) (const char *name) = NULL;
#endif
static int     (*next_chdir) (const char *path) = NULL;
static int     (*next_chmod) (const char *path, mode_t mode) = NULL;
static int     (*next_chown) (const char *path, uid_t owner, gid_t group) = NULL;
static int     (*next_chroot) (const char *path) = NULL;
static int     (*next_creat) (const char *pathname, mode_t mode) = NULL;
static int     (*next_creat64) (const char *pathname, mode_t mode) = NULL;
#ifdef HAVE_DLMOPEN
static void *  (*next_dlmopen) (Lmid_t nsid, const char *filename, int flag) = NULL;
#endif
static void *  (*next_dlopen) (const char *filename, int flag) = NULL;
#ifdef HAVE_EUIDACCESS
static int     (*next_euidaccess) (const char *pathname, int mode) = NULL;
#endif
/* static int     (*next_execl) (const char *path, const char *arg, ...) = NULL; */
/* static int     (*next_execle) (const char *path, const char *arg, ...) = NULL; */
/* static int     (*next_execlp) (const char *file, const char *arg, ...) = NULL; */
/* static int     (*next_execv) (const char *path, char *const argv []) = NULL; */
static int     (*next_execve) (const char *filename, char *const argv [], char *const envp[]) = NULL;
static int     (*next_execvp) (const char *file, char *const argv []) = NULL;
static FILE *  (*next_fopen) (const char *path, const char *mode) = NULL;
static FILE *  (*next_fopen64) (const char *path, const char *mode) = NULL;
static FILE *  (*next_freopen) (const char *path, const char *mode, FILE *stream) = NULL;
static FILE *  (*next_freopen64) (const char *path, const char *mode, FILE *stream) = NULL;
#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
static FTS *   (*next_fts_open) (char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **)) = NULL;
#endif
#endif
#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
static int     (*next_ftw) (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd) = NULL;
#endif
#endif
#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
static int     (*next_ftw64) (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd) = NULL;
#endif
#endif
#ifdef HAVE_GET_CURRENT_DIR_NAME
static char *  (*next_get_current_dir_name) (void) = NULL;
#endif
static char *  (*next_getcwd) (char *buf, size_t size) = NULL;
static char *  (*next_getwd) (char *buf) = NULL;
#ifdef HAVE_GETXATTR
static ssize_t (*next_getxattr) (const char *path, const char *name, void *value, size_t size) = NULL;
#endif
static int     (*next_glob) (const char *pattern, int flags, int (*errfunc) (const char *, int), glob_t *pglob) = NULL;
#ifdef HAVE_GLOB64
static int     (*next_glob64) (const char *pattern, int flags, int (*errfunc) (const char *, int), glob64_t *pglob) = NULL;
#endif
#ifdef HAVE_GLOB_PATTERN_P
static int     (*next_glob_pattern_p) (const char *pattern, int quote) = NULL;
#endif
#ifdef HAVE_LCHMOD
static int     (*next_lchmod) (const char *path, mode_t mode) = NULL;
#endif
static int     (*next_lchown) (const char *path, uid_t owner, gid_t group) = NULL;
#ifdef HAVE_LCKPWDF
/* static int     (*next_lckpwdf) (void) = NULL; */
#endif
#ifdef HAVE_LGETXATTR
static ssize_t (*next_lgetxattr) (const char *path, const char *name, void *value, size_t size) = NULL;
#endif
static int     (*next_link) (const char *oldpath, const char *newpath) = NULL;
#ifdef HAVE_LISTXATTR
static ssize_t (*next_listxattr) (const char *path, char *list, size_t size) = NULL;
#endif
#ifdef HAVE_LLISTXATTR
static ssize_t (*next_llistxattr) (const char *path, char *list, size_t size) = NULL;
#endif
#ifdef HAVE_LREMOVEXATTR
static int     (*next_lremovexattr) (const char *path, const char *name) = NULL;
#endif
#ifdef HAVE_LSETXATTR
static int     (*next_lsetxattr) (const char *path, const char *name, const void *value, size_t size, int flags) = NULL;
#endif
#if !defined(HAVE___LXSTAT)
static int     (*next_lstat) (const char *file_name, struct stat *buf) = NULL;
#endif
#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
static int     (*next_lstat64) (const char *file_name, struct stat64 *buf) = NULL;
#endif
#endif
#ifdef HAVE_LUTIMES
static int     (*next_lutimes) (const char *filename, const struct timeval tv[2]) = NULL;
#endif
static int     (*next_mkdir) (const char *pathname, mode_t mode) = NULL;
#ifdef HAVE_MKDTEMP
static char *  (*next_mkdtemp) (char *template) = NULL;
#endif
static int     (*next_mknod) (const char *pathname, mode_t mode, dev_t dev) = NULL;
static int     (*next_mkfifo) (const char *pathname, mode_t mode) = NULL;
static int     (*next_mkstemp) (char *template) = NULL;
static int     (*next_mkstemp64) (char *template) = NULL;
static char *  (*next_mktemp) (char *template) = NULL;
#ifdef HAVE_NFTW
static int     (*next_nftw) (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s), int nopenfd, int flags) = NULL;
#endif
#ifdef HAVE_NFTW64
static int     (*next_nftw64) (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag, struct FTW *s), int nopenfd, int flags) = NULL;
#endif
static int     (*next_open) (const char *pathname, int flags, ...) = NULL;
static int     (*next_open64) (const char *pathname, int flags, ...) = NULL;
#if !defined(HAVE___OPENDIR2)
static DIR *   (*next_opendir) (const char *name) = NULL;
#endif
static long    (*next_pathconf) (const char *path, int name) = NULL;
static int     (*next_readlink) (const char *path, char *buf, READLINK_TYPE_ARG3) = NULL;
static char *  (*next_realpath) (const char *name, char *resolved) = NULL;
static int     (*next_remove) (const char *pathname) = NULL;
#ifdef HAVE_REMOVEXATTR
static int     (*next_removexattr) (const char *path, const char *name) = NULL;
#endif
static int     (*next_rename) (const char *oldpath, const char *newpath) = NULL;
#ifdef HAVE_REVOKE
static int     (*next_revoke) (const char *file) = NULL;
#endif
static int     (*next_rmdir) (const char *pathname) = NULL;
#ifdef HAVE_SCANDIR
static int     (*next_scandir) (const char *dir, struct dirent ***namelist, SCANDIR_TYPE_ARG3, int(*compar)(const void *, const void *)) = NULL;
#endif
#ifdef HAVE_SCANDIR64
static int     (*next_scandir64) (const char *dir, struct dirent64 ***namelist, int(*filter)(const struct dirent64 *), int(*compar)(const void *, const void *)) = NULL;
#endif
#ifdef HAVE_SETXATTR
static int     (*next_setxattr) (const char *path, const char *name, const void *value, size_t size, int flags) = NULL;
#endif
#if !defined(HAVE___XSTAT)
static int     (*next_stat) (const char *file_name, struct stat *buf) = NULL;
#endif
#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
static int     (*next_stat64) (const char *file_name, struct stat64 *buf) = NULL;
#endif
#endif
static int     (*next_symlink) (const char *oldpath, const char *newpath) = NULL;
static char *  (*next_tempnam) (const char *dir, const char *pfx) = NULL;
static char *  (*next_tmpnam) (char *s) = NULL;
static int     (*next_truncate) (const char *path, off_t length) = NULL;
#ifdef HAVE_TRUNCATE64
static int     (*next_truncate64) (const char *path, off64_t length) = NULL;
#endif
static int     (*next_unlink) (const char *pathname) = NULL;
#ifdef HAVE_ULCKPWDF
/* static int     (*next_ulckpwdf) (void) = NULL; */
#endif
static int     (*next_utime) (const char *filename, const struct utimbuf *buf) = NULL;
static int     (*next_utimes) (const char *filename, const struct timeval tv[2]) = NULL;

/*
 * We retrieve a list of directories to exclude from the jail
 * thanks to FAKECHROOT_EXCLUDE_PATHS and and we deal with the process
 * name KLIK_APPRUN to save the all pids into the virtual /pid file 
 * (used to check if the application if still running)
 */
void fakechroot_init (void) __attribute((constructor));
void fakechroot_init (void)
{
    int i,j,retval;
    struct passwd* passwd = NULL;
    char file_pid[FAKECHROOT_MAXPATH];
    char file_path[FAKECHROOT_MAXPATH];
    char readlink_path[FAKECHROOT_MAXPATH];
    FILE *file_desc;
    char *env;

    if (!_first)
    {
        _first = 1;

        // we need to do it here in order to save the pid file
        nextsym(fopen, "fopen");

        // We get a list of directories or files
        env = getenv("FAKECHROOT_EXCLUDE_PATHS");
        if (env)
        {
            char *v_ptr, *v_buf;
            char *v_buffer = strdup(env);

            v_ptr = strtok_r(v_buffer, ":", &v_buf);
            for (;_list_max<32;)
            {
                if (v_ptr == NULL) break;
                _exclude_length[_list_max] = strlen(v_ptr);
                _exclude_list[_list_max] = malloc(_exclude_length[_list_max]+1);
                strcpy(_exclude_list[_list_max], v_ptr);
                _list_max++;
                v_ptr = strtok_r(NULL, ":", &v_buf);
            }

            free(v_buffer);
        }

        // We save the process id
        env = getenv("KLIK_APPRUN");
        if (env && getenv("FAKECHROOT_BASE"))
        {
            sprintf(file_pid, "/proc/%d/exe", (int)getpid());
            retval = readlink(file_pid,readlink_path,FAKECHROOT_MAXPATH);
            if (DEBUG) fprintf(stderr, "# compare (KLIK_APPRUN=%s, readlink=%s) on pid=%d\n", env, readlink_path, (int)getpid());
            if (retval!=-1 && strcmp(env,readlink_path)==0)
            {
                sprintf(file_pid, "%d", (int)getpid());
                sprintf(file_path, "%s/pid", getenv("FAKECHROOT_BASE"));
                file_desc = next_fopen(file_path, "w");
                if (file_desc != NULL)
                {
                    fwrite((void*)file_pid, 1, (size_t)strlen(file_pid), file_desc);
                    fclose(file_desc);
                }
            }
        }

        // We get the home of the user
        passwd = getpwuid(getuid());
        if (passwd && passwd->pw_dir)
        {
            _home_path = malloc(strlen(passwd->pw_dir)+2);
            strcpy(_home_path, passwd->pw_dir);
            strcat(_home_path, "/");
        }
    }

#ifdef HAVE___LXSTAT
    nextsym(__lxstat, "__lxstat");
#endif
#ifdef HAVE___LXSTAT64
    nextsym(__lxstat64, "__lxstat64");
#endif
#ifdef HAVE___OPEN
    nextsym(__open, "__open");
#endif
#ifdef HAVE___OPEN64
    nextsym(__open64, "__open64");
#endif
#ifdef HAVE___OPENDIR2
    nextsym(__opendir2, "__opendir2");
#endif
#ifdef HAVE___XMKNOD
    nextsym(__xmknod, "__xmknod");
#endif
#ifdef HAVE___XSTAT
    nextsym(__xstat, "__xstat");
#endif
#ifdef HAVE___XSTAT64
    nextsym(__xstat64, "__xstat64");
#endif
    nextsym(access, "access");
    nextsym(acct, "acct");
#ifdef HAVE_CANONICALIZE_FILE_NAME
    nextsym(canonicalize_file_name, "canonicalize_file_name");
#endif
    nextsym(chdir, "chdir");
    nextsym(chmod, "chmod");
    nextsym(chown, "chown");
    nextsym(chroot, "chroot");
    nextsym(creat, "creat");
    nextsym(creat64, "creat64");
#ifdef HAVE_DLMOPEN
    nextsym(dlmopen, "dlmopen");
#endif
    nextsym(dlopen, "dlopen");
#ifdef HAVE_EUIDACCESS
    nextsym(euidaccess, "euidaccess");
#endif
    /*    nextsym(execl, "execl"); */
    /*    nextsym(execle, "execle"); */
    /*    nextsym(execlp, "execlp"); */
    /*    nextsym(execv, "execv"); */
    nextsym(execve, "execve");
    nextsym(execvp, "execvp");
    //nextsym(fopen, "fopen");
    nextsym(fopen64, "fopen64");
    nextsym(freopen, "freopen");
    nextsym(freopen64, "freopen64");
#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
    nextsym(fts_open, "fts_open");
#endif
#endif
#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
    nextsym(ftw, "ftw");
#endif
#endif
#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
    nextsym(ftw64, "ftw64");
#endif
#endif
#ifdef HAVE_GET_CURRENT_DIR_NAME
    nextsym(get_current_dir_name, "get_current_dir_name");
#endif
    nextsym(getcwd, "getcwd");
    nextsym(getwd, "getwd");
#ifdef HAVE_GETXATTR
    nextsym(getxattr, "getxattr");
#endif
    nextsym(glob, "glob");
#ifdef HAVE_GLOB64
    nextsym(glob64, "glob64");
#endif
#ifdef HAVE_GLOB_PATTERN_P
    nextsym(glob_pattern_p, "glob_pattern_p");
#endif
#ifdef HAVE_LCHMOD
    nextsym(lchmod, "lchmod");
#endif
    nextsym(lchown, "lchown");
#ifdef HAVE_LCKPWDF
    /*    nextsym(lckpwdf, "lckpwdf"); */
#endif
#ifdef HAVE_LGETXATTR
    nextsym(lgetxattr, "lgetxattr");
#endif
    nextsym(link, "link");
#ifdef HAVE_LISTXATTR
    nextsym(listxattr, "listxattr");
#endif
#ifdef HAVE_LLISTXATTR
    nextsym(llistxattr, "llistxattr");
#endif
#ifdef HAVE_LREMOVEXATTR
    nextsym(lremovexattr, "lremovexattr");
#endif
#ifdef HAVE_LSETXATTR
    nextsym(lsetxattr, "lsetxattr");
#endif
#if !defined(HAVE___LXSTAT)
    nextsym(lstat, "lstat");
#endif
#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
    nextsym(lstat64, "lstat64");
#endif
#endif
#ifdef HAVE_LUTIMES
    nextsym(lutimes, "lutimes");
#endif
    nextsym(mkdir, "mkdir");
#ifdef HAVE_MKDTEMP
    nextsym(mkdtemp, "mkdtemp");
#endif
    nextsym(mknod, "mknod");
    nextsym(mkfifo, "mkfifo");
    nextsym(mkstemp, "mkstemp");
    nextsym(mkstemp64, "mkstemp64");
    nextsym(mktemp, "mktemp");
#ifdef HAVE_NFTW
    nextsym(nftw, "nftw");
#endif
#ifdef HAVE_NFTW64
    nextsym(nftw64, "nftw64");
#endif
    nextsym(open, "open");
    nextsym(open64, "open64");
#if !defined(HAVE___OPENDIR2)
    nextsym(opendir, "opendir");
#endif
    nextsym(pathconf, "pathconf");
    nextsym(readlink, "readlink");
    nextsym(realpath, "realpath");
    nextsym(remove, "remove");
#ifdef HAVE_REMOVEXATTR
    nextsym(removexattr, "removexattr");
#endif
    nextsym(rename, "rename");
#ifdef HAVE_REVOKE
    nextsym(revoke, "revoke");
#endif
    nextsym(rmdir, "rmdir");
#ifdef HAVE_SCANDIR
    nextsym(scandir, "scandir");
#endif
#ifdef HAVE_SCANDIR64
    nextsym(scandir64, "scandir64");
#endif
#ifdef HAVE_SETXATTR
    nextsym(setxattr, "setxattr");
#endif
#if !defined(HAVE___XSTAT)
    nextsym(stat, "stat");
#endif
#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
    nextsym(stat64, "stat64");
#endif
#endif
    nextsym(symlink, "symlink");
    nextsym(tempnam, "tempnam");
    nextsym(tmpnam, "tmpnam");
    nextsym(truncate, "truncate");
#ifdef HAVE_TRUNCATE64
    nextsym(truncate64, "truncate64");
#endif
    nextsym(unlink, "unlink");
#ifdef HAVE_ULCKPWDF
    /*    nextsym(ulckpwdf, "ulckpwdf"); */
#endif
    nextsym(utime, "utime");
    nextsym(utimes, "utimes");
}

/*
 *  KLIK2 : expand a path to be absolute
 */
void expand_path(const char *p_dest, const char *p_path)
{
    char *fakechroot_path, *fakechroot_ptr;
    char cwd_path[FAKECHROOT_MAXPATH];
    char v_path[FAKECHROOT_MAXPATH];
    char *ptr_path;

    if (_home_path == NULL)
    {
        strcpy((char*)p_dest, (char*)p_path);
        return;
    }

    strcpy(v_path, p_path);

    // we try to expand ~ path
    if (v_path[0] == '~')
    {
        strcpy(v_path, _home_path);
        strcat(v_path, &(p_path[1]));
    }

    // take care of double-slash
    if (strncmp(v_path,"//",2) ==0)
    {
        strcpy(cwd_path, v_path);
        strcpy(v_path, &(cwd_path[1]));
    }

    // We need to expand relative paths
    if (v_path[0] !=  '/')
    {
        if (next_getcwd == NULL) fakechroot_init();
        next_getcwd(cwd_path, FAKECHROOT_MAXPATH);
        ptr_path = cwd_path;
        narrow_chroot_path(ptr_path, fakechroot_path, fakechroot_ptr);
        strcat(ptr_path, "/");
        strcat(ptr_path, v_path);
        strcpy(v_path, ptr_path);
    }

    strcpy((char*)p_dest, (char*)v_path);
}

/*
 *  KLIK2
 *  0, expand the local path
 *  1, we allow direct access to files
 */
int localdir(const char *p_path)
{
    char *fakechroot_path, *fakechroot_ptr;
    char v_path[FAKECHROOT_MAXPATH];
    int i, len;

    // Fakechroot initialization
    if (next___lxstat == NULL) fakechroot_init();

    // We need to expand the path
    if (!p_path) return 0;
    expand_path(v_path, p_path);
if (DEBUG) fprintf(stderr, "# try to expand path [%s => %s]\n", p_path, v_path);

    // We do not want to give direct access to the /tmp/klik directory
    if (strncmp(v_path,"/tmp/klik",9) == 0) return 0;
    if (strncmp(v_path,"//tmp/klik",10) == 0) return 0;

    // We try to find if we need direct access to a file
    len = strlen(v_path);
    for (i=0; i<_list_max; i++)
    {
        if (_exclude_length[i]>len ||
            v_path[_exclude_length[i]-1]!=(_exclude_list[i])[_exclude_length[i]-1] ||
            strncmp(_exclude_list[i],v_path,_exclude_length[i])!=0) continue;
        if (_exclude_length[i]==len || v_path[_exclude_length[i]]=='/') return 1;
    }

    return 0;
}


#ifdef HAVE___LXSTAT
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __lxstat(int ver, const char *filename, struct stat *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __lxstat(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __lxstat(%s)\n", filename);
    return next___lxstat(ver, filename, buf);
}
#endif

#ifdef HAVE___LXSTAT64
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __lxstat64(int ver, const char *filename, struct stat64 *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __lxstat64(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __lxstat64(%s)\n", filename);
    return next___lxstat64(ver, filename, buf);
}
#endif

#ifdef HAVE___OPEN
/* Internal libc function */
int __open(const char *pathname, int flags, ...)
{
    int mode = 0;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __open(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __open(%s)\n", pathname);

    if (flags & O_CREAT)
    {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    return next___open(pathname, flags, mode);
}
#endif

#ifdef HAVE___OPEN64
/* Internal libc function */
int __open64(const char *pathname, int flags, ...)
{
    int mode = 0;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __open64(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __open64(%s)\n", pathname);

    if (flags & O_CREAT)
    {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    return next___open64(pathname, flags, mode);
}
#endif

#ifdef HAVE___OPENDIR2
/* Internal libc function */
/* #include <dirent.h> */
DIR *__opendir2(const char *name, int flags)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __opendir2(%s)\n", name);
    if (!localdir(name)) expand_chroot_path(name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __opendir2(%s)\n", name);
    return next___opendir2(name, flags);
}
#endif

#ifdef HAVE___XMKNOD
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xmknod(int ver, const char *path, mode_t mode, dev_t *dev)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __xmknod(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __xmknod(%s)\n", path);
    return next___xmknod(ver, path, mode, dev);
}
#endif

#ifdef HAVE___XSTAT
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xstat(int ver, const char *filename, struct stat *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __xstat(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __xstat(%s)\n", filename);
    return next___xstat(ver, filename, buf);
}
#endif

#ifdef HAVE___XSTAT64
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xstat64(int ver, const char *filename, struct stat64 *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# __xstat64(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path __xstat64(%s)\n", filename);
    return next___xstat64(ver, filename, buf);
}
#endif

#ifdef HAVE__XFTW
/* include <ftw.h> */
int _xftw(int mode, const char *dir, int(*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# _xftw(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path _xftw(%s)\n", dir);
    return next__xftw(mode, dir, fn, nopenfd);
}
#endif

#ifdef HAVE__XFTW64
/* include <ftw.h> */
int _xftw64(int mode, const char *dir, int(*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# _xftw64(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path _xftw64(%s)\n", dir);
    return next__xftw64(mode, dir, fn, nopenfd);
}
#endif

/* #include <unistd.h> */
int access(const char *pathname, int mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# access(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path access(%s)\n", pathname);
    return next_access(pathname, mode);
}


/* #include <unistd.h> */
int acct(const char *filename)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# acct(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path acct(%s)\n", filename);
    return next_acct(filename);
}


#ifdef HAVE_CANONICALIZE_FILE_NAME
/* #include <stdlib.h> */
char *canonicalize_file_name(const char *name)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# canonicalize_file_name(%s)\n", name);
    if (!localdir(name)) expand_chroot_path(name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path canonicalize_file_name(%s)\n", name);
    return next_canonicalize_file_name(name);
}
#endif

/* #include <unistd.h> */
int chdir(const char *path)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# chdir(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path chdir(%s)\n", path);
    return next_chdir(path);
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int chmod(const char *path, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# chmod(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path chmod(%s)\n", path);
    return next_chmod(path, mode);
}


/* #include <sys/types.h> */
/* #include <unistd.h> */
int chown(const char *path, uid_t owner, gid_t group)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# chown(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path chown(%s)\n", path);
    return next_chown(path, owner, group);
}


/* #include <unistd.h> */
int chroot(const char *path)
{
if (DEBUG) fprintf(stderr, "# chroot(%s)\n", path);

    if (getenv("FAKECHROOT_BASE") == NULL)
    {
#if defined(HAVE_SETENV)
        setenv("FAKECHROOT_BASE", path, 1);
#else
        envbuf = malloc(strlen(path)+16);
        snprintf(envbuf, strlen(path)+16, "FAKECHROOT_BASE=%s", path);
        putenv(envbuf);
#endif
    }
    return 0;
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int creat(const char *pathname, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# creat(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path creat(%s)\n", pathname);
    return next_creat(pathname, mode);
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int creat64(const char *pathname, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# creat64(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path creat64(%s)\n", pathname);
    return next_creat64(pathname, mode);
}


#ifdef HAVE_DLMOPEN
/* #include <dlfcn.h> */
void *dlmopen(Lmid_t nsid, const char *filename, int flag)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# dlmopen(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path dlmopen(%s)\n", filename);
    return next_dlmopen(nsid, filename, flag);
}
#endif

/* #include <dlfcn.h> */
void *dlopen(const char *filename, int flag)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# dlopen(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path dlopen(%s)\n", filename);
    return next_dlopen(filename, flag);
}


#ifdef HAVE_EUIDACCESS
/* #include <unistd.h> */
int euidaccess(const char *pathname, int mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# euidaccess(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path euidaccess(%s)\n", pathname);
    return next_euidaccess(pathname, mode);
}
#endif

/* #include <unistd.h> */
int execl(const char *path, const char *arg, ...)
{
    size_t argv_max = 1024;
    const char **argv = alloca(argv_max * sizeof(const char *));
    unsigned int i;
    va_list args;

if (DEBUG) fprintf(stderr, "# execl(%s)\n", path);

    argv[0] = arg;

    va_start(args, arg);
    i = 0;
    while (argv[i++] != NULL)
    {
        if (i == argv_max)
        {
            const char **nptr = alloca((argv_max *= 2) * sizeof(const char *));

            if ((char *) argv + i == (char *) nptr)
                /* Stack grows up.  */
                argv_max += i;
            else
                /* We have a hole in the stack.  */
                argv = (const char **) memcpy(nptr, argv, i*sizeof(const char *));
        }

        argv[i] = va_arg(args, const char *);
    }
    va_end(args);

    return execve(path, (char *const *) argv, environ);
}


/* #include <unistd.h> */
int execle(const char *path, const char *arg, ...)
{
    size_t argv_max = 1024;
    const char **argv = alloca(argv_max * sizeof(const char *));
    const char *const *envp;
    unsigned int i;
    va_list args;
    argv[0] = arg;

if (DEBUG) fprintf(stderr, "# execle(%s)\n", path);

    va_start(args, arg);
    i = 0;
    while (argv[i++] != NULL)
    {
        if (i == argv_max)
        {
            const char **nptr = alloca((argv_max *= 2) * sizeof(const char *));

            if ((char *) argv + i == (char *) nptr)
                /* Stack grows up.  */
                argv_max += i;
            else
                /* We have a hole in the stack.  */
                argv = (const char **) memcpy(nptr, argv, i * sizeof(const char *));
        }

        argv[i] = va_arg(args, const char *);
    }

    envp = va_arg(args, const char *const *);
    va_end(args);

    return execve(path, (char *const *) argv, (char *const *) envp);
}


/* #include <unistd.h> */
int execlp(const char *file, const char *arg, ...)
{
    size_t argv_max = 1024;
    const char **argv = alloca(argv_max*sizeof(const char *));
    unsigned int i;
    va_list args;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];

if (DEBUG) fprintf(stderr, "# execlp(%s)\n", file);

    argv[0] = arg;

    va_start(args, arg);
    i = 0;
    while (argv[i++] != NULL)
    {
        if (i == argv_max)
        {
            const char **nptr = alloca((argv_max *= 2) * sizeof(const char *));

            if ((char*) argv + i == (char*) nptr)
                /* Stack grows up.  */
                argv_max += i;
            else
                /* We have a hole in the stack.  */
                argv = (const char **) memcpy(nptr, argv, i*sizeof(const char *));
        }

        argv[i] = va_arg(args, const char *);
    }
    va_end (args);

    if (!localdir(file)) expand_chroot_path(file, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path execlp(%s)\n", file);
    return next_execvp(file, (char *const *) argv);
}


/* #include <unistd.h> */
int execv(const char *path, char *const argv [])
{
if (DEBUG) fprintf(stderr, "# execv(%s)\n", path);
    return execve(path, argv, environ);
}


/* #include <unistd.h> */
int execve(const char *filename, char *const argv [], char *const envp[])
{
    int file;
    char hashbang[FAKECHROOT_MAXPATH];
    char temporary[FAKECHROOT_MAXPATH];
    size_t argv_max = 1024;
    const char **newargv = alloca(argv_max * sizeof(const char *));
    char tmp[FAKECHROOT_MAXPATH], newfilename[FAKECHROOT_MAXPATH], argv0[FAKECHROOT_MAXPATH];
    char *envbuf = getenv("FAKECHROOT_WORKING_DIRECTORY");
    char *ptr;
    char **envp_v;
    unsigned int i, j, n, envp_c;
    char c;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# execve(%s)\n", filename);

    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
    strcpy(tmp, filename);
    filename = tmp;

    if ((file = open(filename, O_RDONLY)) == -1)
    {
        errno = ENOENT;
        return -1;
    }

    i = read(file, hashbang, FAKECHROOT_MAXPATH-2);
    close(file);
    if (i == -1)
    {
        errno = ENOENT;
        return -1;
    }

    if (next_execve == NULL) fakechroot_init();

    // Prefix all libraries from LD_LIBRARY_PATH with fuse mount point
    /*char *env = getenv("FAKECHROOT_BASE");
    if (env!=NULL && getenv("LD_LIBRARY_PATH")!=NULL)
    {
        char *v_ptr, *v_buf;
        char *v_buffer = strdup(getenv("LD_LIBRARY_PATH"));
        char *v_newlibrarypath = malloc(10*FAKECHROOT_MAXPATH);   // risk of buffer overflow ?

        sprintf(v_newlibrarypath, "LD_LIBRARY_PATH=");
        v_ptr = strtok_r(v_buffer, ":", &v_buf);

        for (;;)
        {
            if (v_ptr == NULL) break;
            if (strncmp(env, v_ptr, strlen(env)) == 0) sprintf(temporary, "%s:", v_ptr);
            else sprintf(temporary, "%s/%s:", env, v_ptr);
            strcat(v_newlibrarypath, temporary);
            v_ptr = strtok_r(NULL, ":", &v_buf);
        }

        free(v_buffer);

        for (envp_c=0; envp[envp_c++]!=NULL; envp_c++);
        envp_v = (char **)malloc(sizeof(char*) * (envp_c+1));
        for (i=0; envp[i]!=NULL; i++)
        {
            if (strncmp("LD_LIBRARY_PATH=",envp[i],16) == 0) envp_v[i] = strdup(v_newlibrarypath);
            else envp_v[i] = strdup(envp[i]);
        }
        envp_v[i] = NULL;
        free(v_newlibrarypath);
    }*/

    if (hashbang[0] != '#' || hashbang[1] != '!')
    {
if (DEBUG) fprintf(stderr, "# before next_execve (%s) #1\n", filename);
        if (envbuf != NULL) chdir(envbuf);  else chdir(_home_path);
        return next_execve(filename, argv, envp/*envp_v*/);
    }

    hashbang[i] = hashbang[i+1] = 0;
    for (i=j=2; (hashbang[i]==' ' || hashbang[i]=='\t') && i<FAKECHROOT_MAXPATH; i++, j++);
    for (n = 0; i < FAKECHROOT_MAXPATH; i++)
    {
        c = hashbang[i];
        if (hashbang[i] == 0 || hashbang[i] == ' ' || hashbang[i] == '\t' || hashbang[i] == '\n')
        {
            hashbang[i] = 0;
            if (i > j)
            {
                if (n == 0)
                {
                    ptr = &hashbang[j];
                    if (!localdir(ptr)) expand_chroot_path(ptr, fakechroot_path, fakechroot_ptr, fakechroot_buf);
                    strcpy(newfilename, ptr);
                    strcpy(argv0, &hashbang[j]);
                    newargv[n++] = argv0;
                }
                else
                {
                    newargv[n++] = &hashbang[j];
                }
            }
            j = i + 1;
        }
        if (c == '\n' || c == 0)
            break;
    }

    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
    newargv[n++] = filename;

    for (i=1; argv[i]!=NULL && i<argv_max;)
    {
        newargv[n++] = argv[i++];
    }

    newargv[n] = 0;

if (DEBUG) fprintf(stderr, "# before next_execve #2\n");
    if (envbuf != NULL) chdir(envbuf);  else chdir(_home_path);
    return next_execve(newfilename, (char *const *)newargv, envp/*envp_v*/);
}


/* #include <unistd.h> */
int execvp(const char *file, char *const argv [])
{
if (DEBUG) fprintf(stderr, "# execvp(%s)\n", file);

    if (*file == '\0')
    {
        /* We check the simple case first. */
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/') != NULL)
    {
        /* Don't search when it contains a slash.  */
        return execve(file, argv, environ);
    }
    else
    {
        int got_eacces = 0;
        char *path, *p, *name;
        size_t len;
        size_t pathlen;

        path = getenv("PATH");
        if (path == NULL)
        {
            /* There is no `PATH' in the environment.
                                                 The default search path is the current directory
                                                 followed by the path `confstr' returns for `_CS_PATH'.  */
            len = confstr(_CS_PATH,(char *) NULL, 0);
            path = (char *) alloca(1 + len);
            path[0] = ':';
            (void) confstr(_CS_PATH, path + 1, len);
        }

        len = strlen(file) + 1;
        pathlen = strlen(path);
        name = alloca(pathlen + len + 1);
        /* Copy the file name at the top.  */
        name = (char*) memcpy(name + pathlen + 1, file, len);
        /* And add the slash.  */
        *--name = '/';

        p = path;
        do
        {
            char *startp;

            path = p;
            p = strchrnul(path, ':');

            if (p == path)
                /* Two adjacent colons, or a colon at the beginning or the end
                                                               of `PATH' means to search the current directory.  */
                startp = name + 1;
            else
                startp = (char*) memcpy(name - (p - path), path, p - path);

            /* Try to execute this name.  If it works, execv will not return.  */
            execve(startp, argv, environ);

            switch(errno)
            {
                case EACCES:
                    /* Record the we got a `Permission denied' error.  If we end
                                                                             up finding no executable we can use, we want to diagnose
                                                                             that we did find one but were denied access.  */
                    got_eacces = 1;
                case ENOENT:
                case ESTALE:
                case ENOTDIR:
                    /* Those errors indicate the file is missing or not executable
                                                                             by us, in which case we want to just try the next path
                                                                             directory.  */
                    break;

                default:
                    /* Some other error means we found an executable file, but
                                                                             something went wrong executing it; return the error to our
                                                                             caller.  */
                    return -1;
            }
        }
        while (*p++ != '\0');

        /* We tried every element and none of them worked.  */
        if (got_eacces)
            /* At least one failure was due to permissions, so report that
                                               error.  */
            errno = EACCES;
    }

    /* Return the error from the last attempt (probably ENOENT).  */
    return -1;
}


/* #include <stdio.h> */
FILE *fopen(const char *path, const char *mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# fopen(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path fopen(%s)\n", path);
    return next_fopen(path, mode);
}


/* #include <stdio.h> */
FILE *fopen64(const char *path, const char *mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# fopen64(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path fopen64(%s)\n", path);
    return next_fopen64(path, mode);
}


/* #include <stdio.h> */
FILE *freopen(const char *path, const char *mode, FILE *stream)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# freopen(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path freopen(%s)\n", path);
    return next_freopen(path, mode, stream);
}


/* #include <stdio.h> */
FILE *freopen64(const char *path, const char *mode, FILE *stream)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# freopen64(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path freopen64(%s)\n", path);
    return next_freopen64(path, mode, stream);
}


#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
/* #include <fts.h> */
FTS * fts_open(char * const *path_argv, int options, int(*compar)(const FTSENT **, const FTSENT **))
{
    char *fakechroot_path, *fakechroot_ptr, *fakechroot_buf;
    char *path;
    char * const *p;
    char **new_path_argv;
    char **np;
    int n;

if (DEBUG) fprintf(stderr, "# fts_open(%s)\n", path_argv);

    for (n=0,p=path_argv; *p; n++,p++);
    if ((new_path_argv = malloc(n*(sizeof(char *)))) == NULL) return NULL;

    for (n=0,p=path_argv,np=new_path_argv; *p; n++,p++,np++)
    {
        path = *p;
if (DEBUG) fprintf(stderr, "# fts_open(path=%s)\n", path);
        if (!localdir(path)) expand_chroot_path_malloc(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path_malloc fts_open(path=%s)\n", path);
        *np = path;
    }

    if (next_fts_open == NULL) fakechroot_init();
    return next_fts_open(new_path_argv, options, compar);
}
#endif
#endif

#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
/* include <ftw.h> */
int ftw(const char *dir, int(*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# ftw(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path ftw(%s)\n", dir);
    return next_ftw(dir, fn, nopenfd);
}
#endif
#endif

#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW64)
/* include <ftw.h> */
int ftw64(const char *dir, int(*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# ftw64(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path ftw64(%s)\n", dir);
    return next_ftw64(dir, fn, nopenfd);
}
#endif
#endif

#ifdef HAVE_GET_CURRENT_DIR_NAME
/* #include <unistd.h> */
char * get_current_dir_name(void)
{
    char *cwd, *oldptr, *newptr;
    char *fakechroot_path, *fakechroot_ptr;

    if (next_get_current_dir_name == NULL) fakechroot_init();
    if ((cwd = next_get_current_dir_name()) == NULL) return NULL;
    oldptr = cwd;

if (DEBUG) fprintf(stderr, "# get_current_dir_name\n");
    narrow_chroot_path(cwd, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path get_current_dir_name\n");
    if (cwd == NULL) return NULL;
    if ((newptr = malloc(strlen(cwd)+1)) == NULL)
    {
        free(oldptr);
        return NULL;
    }
    strcpy(newptr, cwd);
    free(oldptr);
    return newptr;
}
#endif

/* #include <unistd.h> */
char * getcwd(char *buf, size_t size)
{
    char *cwd, *oldptr, *newptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];

    if (next_getcwd == NULL) fakechroot_init();

    if (buf != NULL)
    {
if (DEBUG) fprintf(stderr, "# getcwd() not null with size=%d) #1\n", size);
        if ((cwd = next_getcwd(fakechroot_buf,FAKECHROOT_MAXPATH)) == NULL) return NULL;
if (DEBUG) fprintf(stderr, "# getcwd(%s) #1\n", cwd);
        narrow_chroot_path(cwd, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path getcwd(%s)\n", cwd);
        strcpy(buf, cwd);
        return (char*)buf;
    }

    if ((cwd = next_getcwd(NULL, 0)) == NULL) return NULL;
    oldptr = cwd;
if (DEBUG) fprintf(stderr, "# getcwd(%s) #2\n", cwd);
    narrow_chroot_path(cwd, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path getcwd(%s)\n", cwd);
    if (cwd == NULL) return NULL;
    if ((newptr = malloc(strlen(cwd)+1)) == NULL)
    {
        free(oldptr);
        return NULL;
    }
    strcpy(newptr, cwd);
    free(oldptr);
    return newptr;
}


/* #include <unistd.h> */
char * getwd(char *buf)
{
    char *cwd;
    char *fakechroot_path, *fakechroot_ptr;

    if (next_getwd == NULL) fakechroot_init();
    if ((cwd = next_getwd(buf)) == NULL) return NULL;

if (DEBUG) fprintf(stderr, "# getwd(%s)\n", buf);
    narrow_chroot_path(cwd, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path getwd(%s)\n", buf);
    return cwd;
}


#ifdef HAVE_GETXATTR
/* #include <sys/xattr.h> */
ssize_t getxattr(const char *path, const char *name, void *value, size_t size)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# getxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path getxattr(%s)\n", path);
    return next_getxattr(path, name, value, size);
}
#endif

/* #include <glob.h> */
int glob(const char *pattern, int flags, int(*errfunc)(const char *, int), glob_t *pglob)
{
    int rc,i;
    char tmp[FAKECHROOT_MAXPATH], *tmpptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];

if (DEBUG) fprintf(stderr, "# glob(%s)\n", pattern);
    if (!localdir(pattern)) expand_chroot_path(pattern, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path glob(%s)\n", pattern);

    rc = next_glob(pattern, flags, errfunc, pglob);
    if (rc < 0)
        return rc;

    for (i=0; i<pglob->gl_pathc; i++)
    {
        strcpy(tmp,pglob->gl_pathv[i]);
        fakechroot_path = getenv("FAKECHROOT_BASE");
        if (fakechroot_path != NULL)
        {
            fakechroot_ptr = strstr(tmp, fakechroot_path);
            if (fakechroot_ptr != tmp) tmpptr = tmp;
            else tmpptr = tmp + strlen(fakechroot_path);
            strcpy(pglob->gl_pathv[i], tmpptr);
        }
    }
    return rc;
}


#ifdef HAVE_GLOB64
/* #include <glob.h> */
int glob64(const char *pattern, int flags, int(*errfunc)(const char *, int), glob64_t *pglob)
{
    int rc,i;
    char tmp[FAKECHROOT_MAXPATH], *tmpptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];

if (DEBUG) fprintf(stderr, "# glob64(%s)\n", pattern);
    if (!localdir(pattern)) expand_chroot_path(pattern, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path glob64(%s)\n", pattern);

    rc = next_glob64(pattern, flags, errfunc, pglob);
    if (rc < 0)
        return rc;

    for (i=0; i<pglob->gl_pathc; i++)
    {
        strcpy(tmp, pglob->gl_pathv[i]);
        fakechroot_path = getenv("FAKECHROOT_BASE");
        if (fakechroot_path != NULL)
        {
            fakechroot_ptr = strstr(tmp, fakechroot_path);
            if (fakechroot_ptr != tmp) tmpptr = tmp;
            else tmpptr = tmp + strlen(fakechroot_path);
            strcpy(pglob->gl_pathv[i], tmpptr);
        }
    }
    return rc;
}
#endif

#ifdef HAVE_GLOB_PATTERN_P
/* #include <glob.h> */
int glob_pattern_p(const char *pattern, int quote)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# glob_pattern_p(%s)\n", pattern);
    if (!localdir(pattern)) expand_chroot_path(pattern, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path glob_pattern_p(%s)\n", pattern);
    return next_glob_pattern_p(pattern, quote);
}
#endif

#ifdef HAVE_LCHMOD
/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int lchmod(const char *path, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lchmod(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lchmod(%s)\n", path);
    return next_lchmod(path, mode);
}
#endif

/* #include <sys/types.h> */
/* #include <unistd.h> */
int lchown(const char *path, uid_t owner, gid_t group)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lchown(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lchown(%s)\n", path);
    return next_lchown(path, owner, group);
}


#ifdef HAVE_LCKPWDF
/* #include <shadow.h> */
int lckpwdf(void)
{
    return 0;
}
#endif

#ifdef HAVE_LGETXATTR
/* #include <sys/xattr.h> */
ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lgetxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lgetxattr(%s)\n", path);
    return next_lgetxattr(path, name, value, size);
}
#endif

/* #include <unistd.h> */
int link(const char *oldpath, const char *newpath)
{
    char tmp[FAKECHROOT_MAXPATH];
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# link(%s:%s)\n", oldpath,newpath);
    if (!localdir(oldpath)) expand_chroot_path(oldpath, fakechroot_path, fakechroot_ptr, fakechroot_buf);
    strcpy(tmp, oldpath);
    oldpath = tmp;
    if (!localdir(newpath)) expand_chroot_path(newpath, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path link(%s:%s)\n", oldpath,newpath);
    return next_link(oldpath, newpath);
}


#ifdef HAVE_LISTXATTR
/* #include <sys/xattr.h> */
ssize_t listxattr(const char *path, char *list, size_t size)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# listxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path listxattr(%s)\n", path);
    return next_listxattr(path, list, size);
}
#endif

#ifdef HAVE_LLISTXATTR
/* #include <sys/xattr.h> */
ssize_t llistxattr(const char *path, char *list, size_t size)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# llistxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path llistxattr(%s)\n", path);
    return next_llistxattr(path, list, size);
}
#endif

#ifdef HAVE_LREMOVEXATTR
/* #include <sys/xattr.h> */
int lremovexattr(const char *path, const char *name)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lremovexattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lremovexattr(%s)\n", path);
    return next_lremovexattr(path, name);
}
#endif

#ifdef HAVE_LSETXATTR
/* #include <sys/xattr.h> */
int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lsetxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lsetxattr(%s)\n", path);
    return next_lsetxattr(path, name, value, size, flags);
}
#endif

#if !defined(HAVE___LXSTAT)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int lstat(const char *file_name, struct stat *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lstat(%s)\n", file_name);
    if (!localdir(file_name)) expand_chroot_path(file_name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lstat(%s)\n", file_name);
    return next_lstat(file_name, buf);
}
#endif

#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int lstat64(const char *file_name, struct stat64 *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lstat64(%s)\n", file_name);
    if (!localdir(file_name)) expand_chroot_path(file_name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lstat64(%s)\n", file_name);
    return next_lstat64(file_name, buf);
}
#endif
#endif

#ifdef HAVE_LUTIMES
/* #include <sys/time.h> */
int lutimes(const char *filename, const struct timeval tv[2])
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# lutimes(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path lutimes(%s)\n", filename);
    return next_lutimes(filename, tv);
}
#endif

/* #include <sys/stat.h> */
/* #include <sys/types.h> */
int mkdir(const char *pathname, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# mkdir(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mkdir(%s)\n", pathname);
    return next_mkdir(pathname, mode);
}


#ifdef HAVE_MKDTEMP
/* #include <stdlib.h> */
char *mkdtemp(char *template)
{
    char tmp[FAKECHROOT_MAXPATH], *oldtemplate = template, *ptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];

if (DEBUG) fprintf(stderr, "# mkdtemp(%s)\n", template);
    if (!localdir(template)) expand_chroot_path(template, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mkdtemp(%s)\n", template);

    if (next_mkdtemp(template) == NULL) return NULL;
    ptr = tmp;
    strcpy(ptr, template);
    narrow_chroot_path(ptr, fakechroot_path, fakechroot_ptr);
    if (ptr == NULL) return NULL;
    strcpy(oldtemplate, ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path mkdtemp(%s)\n", template);
    return oldtemplate;
}
#endif

/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int mkfifo(const char *pathname, mode_t mode)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# mkfifo(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mkfifo(%s)\n", pathname);
    return next_mkfifo(pathname, mode);
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
/* #include <unistd.h> */
int mknod(const char *pathname, mode_t mode, dev_t dev)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# mknod(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mknod(%s)\n", pathname);
    return next_mknod(pathname, mode, dev);
}


/* #include <stdlib.h> */
int mkstemp(char *template)
{
    char tmp[FAKECHROOT_MAXPATH], *oldtemplate = template, *ptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
    int fd;

if (DEBUG) fprintf(stderr, "# mkstemp(%s)\n", template);
    if (!localdir(template)) expand_chroot_path(template, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mkstemp(%s)\n", template);

    if ((fd = next_mkstemp(template)) == -1) return -1;
    ptr = tmp;
    strcpy(ptr, template);
    narrow_chroot_path(ptr, fakechroot_path, fakechroot_ptr);
    if (ptr != NULL)
    {
        strcpy(oldtemplate, ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path mkstemp(%s)\n", template);
    }
    return fd;
}


/* #include <stdlib.h> */
int mkstemp64(char *template)
{
    char tmp[FAKECHROOT_MAXPATH], *oldtemplate = template, *ptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
    int fd;

if (DEBUG) fprintf(stderr, "# mktemp64(%s)\n", template);
    if (!localdir(template)) expand_chroot_path(template, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mktemp64(%s)\n", template);

    if ((fd = next_mkstemp64(template)) == -1) return -1;
    ptr = tmp;
    strcpy(ptr, template);
    narrow_chroot_path(ptr, fakechroot_path, fakechroot_ptr);
    if (ptr != NULL)
    {
        strcpy(oldtemplate, ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path mkstemp64(%s)\n", template);
    }
    return fd;
}


/* #include <stdlib.h> */
char *mktemp(char *template)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# mktemp(%s)\n", template);
    if (!localdir(template)) expand_chroot_path(template, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path mktemp(%s)\n", template);
    return next_mktemp(template);
}


#ifdef HAVE_NFTW
/* #include <ftw.h> */
int nftw(const char *dir, int(*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s), int nopenfd, int flags)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# nftw(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path nftw(%s)\n", dir);
    return next_nftw(dir, fn, nopenfd, flags);
}
#endif

#ifdef HAVE_NFTW64
/* #include <ftw.h> */
int nftw64(const char *dir, int(*fn)(const char *file, const struct stat64 *sb, int flag, struct FTW *s), int nopenfd, int flags)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# nftw64(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path nftw64(%s)\n", dir);
    return next_nftw64(dir, fn, nopenfd, flags);
}
#endif


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int open(const char *pathname, int flags, ...)
{
    int mode = 0;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# open(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path open(%s)\n", pathname);

    if (flags & O_CREAT)
    {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    return next_open(pathname, flags, mode);
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int open64(const char *pathname, int flags, ...)
{
    int mode = 0;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# open64(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path open64(%s)\n", pathname);

    if (flags & O_CREAT)
    {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);
    }

    return next_open64(pathname, flags, mode);
}


#if !defined(HAVE___OPENDIR2)
/* #include <sys/types.h> */
/* #include <dirent.h> */
DIR *opendir(const char *name)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# opendir(%s)\n", name);
    if (!localdir(name)) expand_chroot_path(name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path opendir(%s)\n", name);
    return next_opendir(name);
}
#endif

/* #include <unistd.h> */
long pathconf(const char *path, int name)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
    int retval;
if (DEBUG) fprintf(stderr, "# pathconf(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path pathconf(%s)\n", path);
    retval = next_pathconf(path, name);
    if (retval==0 && name==3) retval=-1; // correct a fuse bug (else, crash qt4)
    return retval;
}

/* #include <unistd.h> */
int readlink(const char *path, char *buf, READLINK_TYPE_ARG3)
{
    int status;
    char tmp[FAKECHROOT_MAXPATH], *tmpptr;
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# readlink(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path readlink(%s)\n", path);

    if ((status = next_readlink(path, tmp, bufsiz)) == -1) return status;
    /* TODO: shouldn't end with \000 */
    tmp[status] = '\0';

    fakechroot_path = getenv("FAKECHROOT_BASE");
    if (fakechroot_path != NULL)
    {
        fakechroot_ptr = strstr(tmp, fakechroot_path);
        if (fakechroot_ptr != tmp) tmpptr = tmp;
        else tmpptr = tmp + strlen(fakechroot_path);
        strcpy(buf, tmpptr);
if (DEBUG) fprintf(stderr, "# readlink(buf=%s)\n", buf);
    }
    else
    {
        strcpy(buf, tmp);
        narrow_chroot_path(buf, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path readlink(buf=%s)\n", buf);
    }
    return strlen(buf);
}


/* #include <stdlib.h> */
char *realpath(const char *name, char *resolved)
{
    char *ptr, *nmalloc;
    char *fakechroot_path, *fakechroot_ptr;
    char fakechroot_buf[FAKECHROOT_MAXPATH], fakechroot_resolved[FAKECHROOT_MAXPATH];

if (DEBUG) fprintf(stderr, "# realpath(%s)\n", name);
if (DEBUG && resolved==NULL) printf("# realpath() request malloc\n");
    if (!localdir(name)) expand_chroot_path(name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path realpath(%s)\n", name);

    if (resolved == NULL)
    {
        if ((ptr = next_realpath(name, fakechroot_resolved)) != NULL)
        {
            narrow_chroot_path(ptr, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path realpath(%s) #1\n", ptr);
            nmalloc = malloc(strlen(ptr)+1);
            strcpy(nmalloc, fakechroot_resolved);
            return nmalloc;
        }
        return NULL;
    }

    if ((ptr = next_realpath(name, fakechroot_resolved)) != NULL)
    {
        narrow_chroot_path(ptr, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# narrow_chroot_path name=%s ptr=%s\n", name, ptr);
    }
    else
    {
        return NULL;
    }
    strcpy(resolved, ptr);
    return (char*)resolved;
}


/* #include <stdio.h> */
int remove(const char *pathname)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# remove(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path remove(%s)\n", pathname);
    return next_remove(pathname);
}


#ifdef HAVE_REMOVEXATTR
/* #include <sys/xattr.h> */
int removexattr(const char *path, const char *name)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# removexattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path removexattr(%s)\n", path);
    return next_removexattr(path, name);
}
#endif


/* #include <stdio.h> */
int rename(const char *oldpath, const char *newpath)
{
    char tmp[FAKECHROOT_MAXPATH];
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# rename(%s:%s)\n", oldpath,newpath);
    if (!localdir(oldpath)) expand_chroot_path(oldpath, fakechroot_path, fakechroot_ptr, fakechroot_buf);
    strcpy(tmp, oldpath);
    oldpath = tmp;
    if (!localdir(newpath)) expand_chroot_path(newpath, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path rename(%s:%s)\n", oldpath,newpath);
    return next_rename(oldpath, newpath);
}


#ifdef HAVE_REVOKE
/* #include <unistd.h> */
int revoke(const char *file)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# revoke(%s)\n", file);
    if (!localdir(file)) expand_chroot_path(file, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path revoke(%s)\n", file);
    return next_revoke(file);
}
#endif


/* #include <unistd.h> */
int rmdir(const char *pathname)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# rmdir(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path rmdir(%s)\n", pathname);
    return next_rmdir(pathname);
}

int getdents(unsigned int fd, struct dirent *dirp, unsigned int count)
{
    int retval, i;
    retval = next_getdents(fd, dirp, count);
}


#ifdef HAVE_SCANDIR
/* #include <dirent.h> */
int scandir(const char *dir, struct dirent ***namelist, SCANDIR_TYPE_ARG3, int(*compar)(const void *, const void *))
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# scandir(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path scandir(%s)\n", dir);
    return next_scandir(dir, namelist, filter, compar);
}
#endif


#ifdef HAVE_SCANDIR64
/* #include <dirent.h> */
int scandir64(const char *dir, struct dirent64 ***namelist, int(*filter)(const struct dirent64 *), int(*compar)(const void *, const void *))
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# scandir64(%s)\n", dir);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path scandir64(%s)\n", dir);
    return next_scandir64(dir, namelist, filter, compar);
}
#endif


#ifdef HAVE_SETXATTR
/* #include <sys/xattr.h> */
int setxattr(const char *path, const char *name, const void *value, size_t size, int flags)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# setxattr(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path setxattr(%s)\n", path);
    return next_setxattr(path, name, value, size, flags);
}
#endif


#if !defined(HAVE___XSTAT)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int stat(const char *file_name, struct stat *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# stat(%s)\n", file_name);
    if (!localdir(file_name)) expand_chroot_path(file_name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path stat(%s)\n", file_name);
    return next_stat(file_name, buf);
}
#endif


#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int stat64(const char *file_name, struct stat64 *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# stat64(%s)\n", file_name);
    if (!localdir(file_name)) expand_chroot_path(file_name, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path stat64(%s)\n", file_name);
    return next_stat64(file_name, buf);
}
#endif
#endif


/* #include <unistd.h> */
int symlink(const char *oldpath, const char *newpath)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# symlink(%s:%s)\n", oldpath, newpath);
     if (!localdir(newpath)) expand_chroot_path(newpath, fakechroot_path, fakechroot_ptr, fakechroot_buf);
     narrow_chroot_path(oldpath, fakechroot_path, fakechroot_ptr);
if (DEBUG) fprintf(stderr, "# [expand|narrow]_chroot_path symlink(%s:%s)\n", oldpath, newpath);
    return next_symlink(oldpath, newpath);
}


/* #include <stdio.h> */
char *tempnam(const char *dir, const char *pfx)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# tempnam(%s:%s)\n", dir, pfx);
    if (!localdir(dir)) expand_chroot_path(dir, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path tempnam(%s:%s)\n", dir, pfx);
    return next_tempnam(dir, pfx);
}


/* #include <stdio.h> */
char *tmpnam(char *s)
{
    char *ptr;
    char *fakechroot_path, *fakechroot_ptr, *fakechroot_buf;
if (DEBUG) fprintf(stderr, "# tmpnam(%s)\n", s);

    if (next_tmpnam == NULL) fakechroot_init();
    if (s != NULL) return next_tmpnam(s);

    ptr = next_tmpnam(NULL);
    if (!localdir(ptr)) expand_chroot_path_malloc(ptr, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path tmpnam(%s)\n", s);
    return ptr;
}


/* #include <unistd.h> */
/* #include <sys/types.h> */
int truncate(const char *path, off_t length)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# truncate(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path truncate(%s)\n", path);
    return next_truncate(path, length);
}


#ifdef HAVE_TRUNCATE64
/* #include <unistd.h> */
/* #include <sys/types.h> */
int truncate64(const char *path, off64_t length)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# truncate64(%s)\n", path);
    if (!localdir(path)) expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path truncate64(%s)\n", path);
    return next_truncate64(path, length);
}
#endif

#ifdef HAVE_ULCKPWDF
/* #include <shadow.h> */
int ulckpwdf (void)
{
    return 0;
}
#endif

/* #include <unistd.h> */
int unlink(const char *pathname)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# unlink(%s)\n", pathname);
    if (!localdir(pathname)) expand_chroot_path(pathname, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path unlink(%s)\n", pathname);
    return next_unlink(pathname);
}


/* #include <sys/types.h> */
/* #include <utime.h> */
int utime(const char *filename, const struct utimbuf *buf)
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# utime(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path utime(%s)\n", filename);
    return next_utime(filename, buf);
}


/* #include <sys/time.h> */
int utimes(const char *filename, const struct timeval tv[2])
{
    char *fakechroot_path, *fakechroot_ptr, fakechroot_buf[FAKECHROOT_MAXPATH];
if (DEBUG) fprintf(stderr, "# utimes(%s)\n", filename);
    if (!localdir(filename)) expand_chroot_path(filename, fakechroot_path, fakechroot_ptr, fakechroot_buf);
if (DEBUG) fprintf(stderr, "# expand_chroot_path utimes(%s)\n", filename);
    return next_utimes(filename, tv);
}
