#include "config.h"

#include <list>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>

#include "inotify-cxx.h"
#include "klikd.h"
#include "KlikdConfig.h"

using namespace std;

Inotify notifier;
KlikdConfig klikdConfig;
string pidFileName = string( getenv("HOME") ) + "/.klik/klikd.pid";


enum disp_type_t { REGISTER, UNREGISTER };
string disp_packages[2];
string disp_command[] = { "klik register ", "klik unregister " };
time_t disp_timeout = 1; // seconds
pthread_t dispatcher[2];
pthread_mutex_t condition_mutex[] = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER };
pthread_cond_t  condition_cond[]  = { PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER };


int syncDirectory(string directories) {
  return system(("klik sync " + directories).data());
}

void *dispatch(void*);
void *call(void*);

void processPackages(string packages, disp_type_t disp_type) {
  disp_packages[disp_type] += packages;
  if ( dispatcher[disp_type] && !pthread_kill(dispatcher[disp_type], 0) )
    // Thread is running, send thread a signal to reset the timeout
    // note: pthread_kill returns 0 if the thread is alive
    pthread_cond_signal( &condition_cond[disp_type] );
  else
    // Thread is not running, create it
    pthread_create( &dispatcher[disp_type], NULL, dispatch, (void*) &disp_type );
}

void *dispatch(void *d_type) {
  
  /* Threaded function.
   * Each register and unregister actions have their own thread.
   * If a package to be (un)registered is found:
   *  - if the action thread was not created, create it
   *  - if the action thread is already running, reset the timeout
   * When the timeout is over, the command to (un)register packages
   * is executed, and the thread dies. */ 

  disp_type_t disp_type = *((disp_type_t*) d_type);
  
  struct timespec timeout;

  clock_gettime(CLOCK_REALTIME, &timeout);
  timeout.tv_sec += disp_timeout;

  /* If the timewait got a timeout (ret!=0), we are ready to
   * commit the new packages.
   * Otherwise, the timewait got interuppted by new packages,
   * so reset the timeout and start listening again. */ 

  int ret = 0;

  pthread_mutex_lock( &condition_mutex[disp_type] );
  while( !ret )
    ret = pthread_cond_timedwait( &condition_cond[disp_type], &condition_mutex[disp_type], &timeout );
  pthread_mutex_unlock( &condition_mutex[disp_type] );

  // Execute command
  pthread_t caller;
  pthread_create( &caller, NULL, call, (void*) (disp_command[disp_type] + disp_packages[disp_type]).data() );
  disp_packages[disp_type].clear();

  return NULL;
  
}

void *call(void *cmd) {
  system((char*) cmd);
  return NULL;
}

void initialize(KlikdConfig& klikdConfig, Inotify& notifier) {
  
  string paths;
  
  for (list<string>::iterator path = klikdConfig.application_folders.begin();
       path != klikdConfig.application_folders.end();
       ++path) {
    
    debug(string("Watching path ") + *path);
    notifier.Add(new InotifyWatch(*path, IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM, true));
    paths += *path + " ";
  }
  
  if ( paths.length() )
    syncDirectory(paths);
  
}

void checkAlredyRunning() {
  // Check if PID file exists
  ifstream pidFile( pidFileName.data() );
  
  int pid = 0;
  if ( pidFile.is_open() ) {
    pidFile >> pid;
    ostringstream pidDir;
    pidDir << "/proc/" << pid;
    if ( !isDirectory(pidDir.str().data()) ) {
      // PID file exists but there is no such process
      // Overwrite file with new PID
      pid = 0;
      debug("Overwriting invalid PID file");
    }
  }
  
  // If after this pid==0, we understand the file doesn't exist or it's invalid.
  
  if ( pid ) {
    
    cerr << "!! Klikd is already running with pid " << pid << endl
	 << "!! If you are sure it is not, please remove " << pidFileName << endl;
    exit(1);
    
  } else {

    // PID file doesn't exist or not valid, create it
    pidFile.close();
    ofstream pidFile( pidFileName.data() );
    pidFile << getpid();
    pidFile.close();

  }
}

void quit() {
  debug("klikd exiting...");
  
  // Clean inotify
  notifier.Close();
  
  // Clean PID file
  if (isFile( pidFileName.data() ))
    remove( pidFileName.data() );
}

void sig_exit(int signal) {
  exit(0);
}

int main(int argc, char **argv) {

  // Make sure klikd is not already running
  checkAlredyRunning();

  signal(SIGINT, sig_exit);
  signal(SIGTERM, sig_exit);
  atexit(quit);
  
  /* If any config file could be read, or all the config files had no valid paths, abort daemon */
  if (klikdConfig.application_folders.empty()) {
    cerr << "!! No folders to watch" << endl
	 << "!! Aborting" << endl;
    exit(1);
  }

  // Initialize daemon: add folders to watch and define behaviour
  
  initialize(klikdConfig, notifier);

  // Wait for events on watched folders

  InotifyEvent event;
  string packages_to_add;
  string packages_to_del;

  while(1) {
    debug("Waiting for events");
    notifier.WaitForEvents();

    // Process each event, either installing or uninstalling a package
    
    int n_events = notifier.GetEventCount();
    
    for (int i=0; i<n_events; i++) {
      
      notifier.GetEvent(event);
      
      switch(event.GetMask()) {
      case IN_CREATE:
      case IN_MOVED_TO:
	packages_to_add += "\"" + (*event.GetWatch()).GetPath() + "/" + event.GetName() + "\" ";
	break;
      case IN_DELETE:
      case IN_MOVED_FROM:
	packages_to_del += "\"" + event.GetName() + "\" ";
	break;
      default:
	break;
      }

    }
    
    if ( packages_to_add.length() ) {
      processPackages( packages_to_add, REGISTER );
      packages_to_add.clear();
    }

    if ( packages_to_del.length() ) {
      processPackages( packages_to_del, UNREGISTER );
      packages_to_del.clear();
    }
    
  }

}
