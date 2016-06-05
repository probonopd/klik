bin_PROGRAMS = klikd
klikd_SOURCES = KlikdConfig.cpp  inotify-cxx.cpp  klikd.cpp
AM_CPPFLAGS = -g3 -Wall -Werror
klikd_LDFLAGS = -lpthread -lrt

run: all
	./klikd
