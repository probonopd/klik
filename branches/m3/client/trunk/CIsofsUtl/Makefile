CPP=g++
CPFLAGS= -O2 -g -Wall
LDFLAGS= # nothing

all : libCIsofsUtl.a cmginfo

libCIsofsUtl.a: lib_CIsoUtl.o lib_CLinkUtl.o
	ar crv libCIsofsUtl.a lib_CIsoUtl.o lib_CLinkUtl.o

lib_CIsoUtl.o: lib_CIsoUtl.cpp lib_CIsoUtl.hh iso_fs.h iso_utl.h rock.h
	$(CPP) -c -DHAVE_CONFIG_H -I. $(CPFLAGS) lib_CIsoUtl.cpp

lib_CLinkUtl.o: lib_CLinkUtl.cpp lib_CLinkUtl.hh
	$(CPP) -c -DHAVE_CONFIG_H -I. $(CPFLAGS) lib_CLinkUtl.cpp

main.o: main.cpp
	$(CPP) -c -DHAVE_CONFIG_H -I. $(CPFLAGS) main.cpp

cmginfo: libCIsofsUtl.a main.o
	 $(CPP) -DHAVE_CONFIG_H -I. $(CPFLAGS) -lz -o cmginfo main.o libCIsofsUtl.a

clean:
	rm -f *.o libCIsofsUtl.a cmginfo
