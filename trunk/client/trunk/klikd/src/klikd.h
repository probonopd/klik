#ifndef __KLIKD_H__
#define __KLIKD_H__

#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "KlikdConfig.h"

using namespace std;
extern KlikdConfig klikdConfig;

static struct stat stbuf;

inline bool isDirectory(const char *path)
{ return ((stat(path, &stbuf) != -1) && (S_ISDIR(stbuf.st_mode))); }

inline bool isFile(const char *path)
{ return ((stat(path, &stbuf) != -1) && (S_ISREG(stbuf.st_mode))); }

inline void debug(string msg)
{ if ( klikdConfig.debug_mode ) cout << "## " << msg << endl; }



#endif /* __KLIKD_H__ */
