#ifndef _LIB_LINKUTL_HH
#define _LIB_LINKUTL_HH

#include<sstream>
#include<iostream>
#include<string>
#include<vector>

using namespace std;

/*
 *  CLinkUtl class declaration
 */

class CLinkUtl
{
    public:

        CLinkUtl(void) {};
        ~CLinkUtl(void) {};
        string GetRealPath(string, string);

    private:

        vector<string> _path;                                         // contains the original path
        vector<string> _link;                                         // contains the symbolic link
};

#endif 	/* _LIB_LINKUTL_HH */
