#ifndef _LIB_CMGUTL_HH
#define _LIB_CMGUTL_HH

#include <fcntl.h>
#include <linux/stat.h>
#include <byteswap.h>
#include <pthread.h>
#include <sys/stat.h>
#include <utime.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <iconv.h>

#include <zlib.h>
#include <locale.h>
#include <langinfo.h>

#include<stdio.h>
#include<stdlib.h>
#include<limits.h>
#include<string>
#include<cstring>
#include<vector>
#include<map>

#include "iso_fs.h"
#include "iso_utl.h"
#include "rock.h"
#include "link_utl.hh"

using namespace std;

/*
 *  CmgUtl class declaration
 */

class CmgUtl : public CLinkUtl
{
    public:

        CmgUtl(string, bool=false, string = "UTF-8//IGNORE");
        ~CmgUtl(void);
        string GetLinkInfo(string, bool=false);
        bool GetFileInfo(string&, mode_t&, long&);
        int ExtractFile(string, FILE*);
        int ReadFile(string, char*, size_t, size_t);
        bool ReadDir(string, vector<string>&);
        bool IsDir(string);
        bool IsLink(string);

    private:

        isofs_inode* findLookupTable(string);
        void insertLookupTable(string, isofs_inode*);
        void searchFileIsoImage(char*);
        void searchFileIsoImage(char*,string&);
        int readDirIsoImage(char*);
        int readDirIsoImage(char*, vector<string>&);
        int readDirIsoImage(char*, bool, vector<string>&);
        int isofs_real_preinit(char*, int);
        char* dstr(char*, const char*, int);
        void* isofs_real_init(void);
        int isofs_check_rr(struct iso_directory_record*);
        int isofs_read_raw_block(int, char*);
        time_t isofs_date(char*, int);
        int isofs_direntry2stat(struct stat*, isofs_inode*);
        char* isofs_fix_entry(char*, size_t);
        void isofs_free_inode(isofs_inode*);
        int isofs_parse_zisofs_header(isofs_inode*);
        int isofs_parse_sa(isofs_inode*, char*, size_t);
        int isofs_real_read_zf(isofs_inode*, char*, size_t, off_t);

        bool _debug;                                                  // debug mode
        string _iocharset;                                            // file encoding
        isofs_context _context;                                       // context of the iso image
        int _filedesc;                                                // file descriptor
        map<string, isofs_inode*> _lookup_table;                      // we maintains a list of known files into the iso image
        pthread_mutex_t _fd_mutex;
        pthread_mutex_t _lookup_table_mutex;
};

#endif 	/* _LIB_CMGUTL_HH */
