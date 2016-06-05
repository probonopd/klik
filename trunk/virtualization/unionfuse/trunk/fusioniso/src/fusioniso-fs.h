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

#ifndef _FS_H
#define _FS_H

#include "fusioniso-isofs.h"

#define PAGE_CACHE_SIZE (4096)

#if defined(PATH_MAX)
#define FS_MAXPATH PATH_MAX
#elif defined(_POSIX_PATH_MAX)
#define FS_MAXPATH _POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define FS_MAXPATH MAXPATHLEN
#else
#define FS_MAXPATH 4096
#endif

typedef struct statfs struct_statfs;

typedef int u32; // needed on kubuntu 7.04 ...

typedef int bool;
#define true 1
#define false 0

typedef enum sandboxing { sandbox_none, sandbox_home, sandbox_portable, sandbox_data } sandbox_type;

typedef int (*fs_dir_fill_t) (void *buf, const char *name,
const struct stat *stat, off_t off);

int fs_real_opendir(const char *path);
int fs_real_readdir(int real, const char *path, void *filler_buf, fs_dir_fill_t filler);
int fs_real_getattr(const char *path, struct stat *stbuf);
int fs_real_readlink(const char *path, char *target, size_t size);
int fs_real_open(const char *path);
int fs_real_read(const char *path, char *out_buf, size_t size, off_t offset);
int fs_real_statfs(const char *path, struct_statfs *stbuf);

int fs_real_access(const char *path, int amode);
int fs_real_mknod(const char *path, mode_t mode, dev_t dev);
int fs_real_write(const char *path, const char *out_buf, size_t size, off_t offset);
int fs_real_unlink(const char *path);
int fs_real_rmdir(const char *path);
int fs_real_mkdir(const char *path, mode_t mode);
int fs_real_symlink(const char *path1, const char *path2);
int fs_real_rename(const char *oldpath, const char *newpath);
int fs_real_link(const char *oldpath, const char *newpath);
int fs_real_chmod(const char *path, mode_t mode);
int fs_real_chown(const char *path, uid_t owner, gid_t group);
int fs_real_truncate(const char *path, off_t length);
int fs_real_utime(const char *path, struct utimbuf *times);
int fs_real_setxattr(const char *path, const char *name, const void *value, size_t size, int options);
int fs_real_getxattr(const char *path, const char *name, void *value, size_t size);
int fs_real_listxattr(const char *path, char *list, size_t size);
int fs_real_removexattr(const char *path, const char *list);

#endif	/* _FS_H */
