#ifndef _LIBFAKECHROOT_H
#define _LIBFAKECHROOT_H

#if defined(PATH_MAX)
#define FAKECHROOT_MAXPATH PATH_MAX
#elif defined(_POSIX_PATH_MAX)
#define FAKECHROOT_MAXPATH _POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define FAKECHROOT_MAXPATH MAXPATHLEN
#else
#define FAKECHROOT_MAXPATH 2048
#endif

#ifndef INLINE
# if __GNUC__
#  define INLINE extern inline
# else
#  define INLINE inline
# endif
#endif

#define narrow_chroot_path(path, fakechroot_path, fakechroot_ptr) \
    { \
        if ((path) != NULL && *((char *)(path)) != '\0') { \
            fakechroot_path = getenv("FAKECHROOT_BASE"); \
            if (fakechroot_path != NULL) { \
                fakechroot_ptr = strstr((path), fakechroot_path); \
                if (fakechroot_ptr == (path)) { \
                    if (strlen((path)) == strlen(fakechroot_path)) { \
                        ((char *)(path))[0] = '/'; \
                        ((char *)(path))[1] = '\0'; \
                    } else { \
                        memmove((void*)(path), (path)+strlen(fakechroot_path), 1+strlen((path))-strlen(fakechroot_path)); \
                    } \
                } \
            } \
        } \
    }

#define expand_chroot_path(path, fakechroot_path, fakechroot_ptr, fakechroot_buf) \
    { \
        if (!fakechroot_localdir(path)) { \
            if ((path) != NULL && *((char *)(path)) == '/') { \
                fakechroot_path = getenv("FAKECHROOT_BASE"); \
                if (fakechroot_path != NULL) { \
                    fakechroot_ptr = strstr((path), fakechroot_path); \
                    if (fakechroot_ptr != (path)) { \
                        strcpy(fakechroot_buf, fakechroot_path); \
                        strcat(fakechroot_buf, (path)); \
                        (path) = fakechroot_buf; \
                    } \
                } \
            } \
        } \
    }

#define expand_chroot_path_malloc(path, fakechroot_path, fakechroot_ptr, fakechroot_buf) \
    { \
        if (!fakechroot_localdir(path)) { \
            if ((path) != NULL && *((char *)(path)) == '/') { \
                fakechroot_path = getenv("FAKECHROOT_BASE"); \
                if (fakechroot_path != NULL) { \
                    fakechroot_ptr = strstr((path), fakechroot_path); \
                    if (fakechroot_ptr != (path)) { \
                        if ((fakechroot_buf = malloc(strlen(fakechroot_path)+strlen(path)+1)) == NULL) { \
                            errno = ENOMEM; \
                            return NULL; \
                        } \
                        strcpy(fakechroot_buf, fakechroot_path); \
                        strcat(fakechroot_buf, (path)); \
                        (path) = fakechroot_buf; \
                    } \
                } \
            } \
        } \
    }

#define nextsym(function, name) \
    { \
        char *msg; \
        if (next_##function == NULL) { \
            *(void **)(&next_##function) = dlsym(RTLD_NEXT, name); \
            if ((msg = dlerror()) != NULL) { \
                 fprintf (stderr, "%s: dlsym(%s): %s\n", PACKAGE, name, msg); \
            } \
        } \
    }

INLINE char* fakechroot_strings_cat(char*, int, const char*, ...);    // Used to prevent buffer overruns
INLINE void  fakechroot_expand_path(const char*, const char*);        // Expand a path to be absolute
INLINE int   fakechroot_localdir(const char*);                        // Check if path is part of the exclude list

/*
 *    ----------------------------
 *    List of overloaded functions
 *    ----------------------------
 *
 *    __lxstat                      fopen                         mkstemp                     getuid
 *    __lxstat64                    fopen64                       mkstemp64                   getgid
 *    __open64                      freopen                       mktemp                      geteuid
 *    __opendir2                    freopen64                     nftw                        getegid
 *    __xmknod                      fts_open                      nftw64                      
 *    __xstat                       ftw                           open                        
 *    __xstat64                     ftw64                         open64                      
 *    _xftw                         get_current_dir_name          opendir                     
 *    _xftw64                       getcwd                        pathconf                    
 *    access                        getwd                         readlink                    
 *    eaccess                       getxattr                      realpath                    
 *    acct                          glob                          remove                      
 *    canonicalize_file_name        glob64                        removexattr                 
 *    chdir                         glob_pattern_p                rename                      
 *    chmod                         lchmod                        revoke                      
 *    chown                         lchown                        rmdir                       
 *    fchown                        lckpwdf                       scandir                     
 *    chroot                        lgetxattr                     scandir64                   
 *    creat                         link                          setxattr                    
 *    creat64                       listxattr                     stat                        
 *    dlmopen                       llistxattr                    stat64                      
 *    dlopen                        lremovexattr                  symlink                     
 *    euidaccess                    lsetxattr                     tempnam                     
 *    euideaccess                   lstat                         tmpnam                      
 *    execl                         lstat64                       truncate                    
 *    execle                        lutimes                       truncate64                  
 *    execlp                        mkdir                         ulckpwdf                    
 *    execv                         mkdtemp                       unlink                      
 *    execve                        mkfifo                        utime                       
 *    execvp                        mknod                         utimes                      
 */

#endif	/* _LIBFAKECHROOT_H */
