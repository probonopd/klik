#include <stdlib.h>
#include <string.h>

#include <string>
#include <list>
#include <iostream>
#include <fstream>

#include "klikd.h"
#include "KlikdConfig.h"

/* To get the configuration we just call the settings.py module */

KlikdConfig::KlikdConfig() {

  string executable ( "klik settings" );
  string data       ( "debug application_folders" );
  
  FILE *output = popen( (executable + " " + data).data(), "r" );
  char *line = NULL;
  size_t n = 0;
  int line_id = 1;
  
  while ( getline( &line, &n, output ) != -1 ) {

    if ( strlen(line) <= 1 )
      ++line_id;

    else {

      switch( line_id ) {
      case 1:
	this->debug_mode = (bool) atoi( line );
	debug("Debug is ON");
	break;
      case 2:
	line[strlen(line)-1] = '\0';
	if ( isDirectory(line) )
	  this->application_folders.push_back( line );
	else
	  debug(string("Dropping path ") + line);
	break;
      default:
	cerr << "!! Extra configuration line: " << line << endl;
	break;

      }
    }
      
    if ( line )
      free( line );

    line = NULL;
  
  }

  pclose( output );

}
