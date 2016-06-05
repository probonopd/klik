#ifndef __KLIKDCONFIG_H__
#define __KLIKDCONFIG_H__

#include <string>
#include <list>
#include "inotify-cxx.h"

using namespace std;

class KlikdConfig {
  
 public:
  list<string> application_folders;
  bool debug_mode;
  
  KlikdConfig();
  ~KlikdConfig() {};

};

#endif /* __KLIKDCONFIG_H__ */
