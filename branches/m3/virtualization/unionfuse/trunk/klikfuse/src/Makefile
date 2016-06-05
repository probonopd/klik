##
## Targeted for Kubuntu 7.04
##

all: fuseiso fusecram

fusioniso:
	make -f Makefile_iso clean
	make -f Makefile_iso
	gcc -D_FILE_OFFSET_BITS=64 -Wall -g -O3 -o fusioniso fusemain.o fs.o isofs.o -L/usr/lib `pkg-config --libs fuse` -lpthread -lrt `pkg-config --libs glib-2.0` -lz

fuseiso:
	make -f Makefile_iso clean
	make -f Makefile_iso
	gcc -D_FILE_OFFSET_BITS=64 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -g -O3 -o fuseiso fusemain.o fs.o isofs.o -L/usr/lib -lfuse -lpthread -lrt -lglib-2.0 -lz
	# gcc -D_FILE_OFFSET_BITS=64 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -g -O3 -o fuseiso fusemain.o fs.o isofs.o -L/usr/lib /usr/lib/libfuse.a -lpthread -lrt /usr/lib/libglib-2.0.a /usr/lib/libz.a

fusecram:
	make -f Makefile_cram clean
	make -f Makefile_cram
	gcc -D_FILE_OFFSET_BITS=64 -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -g -O3 -o fusecram fusemain.o fs.o cramfs.o -L/usr/lib -lfuse -lpthread -lrt -lglib-2.0 -lz
	# gcc -D_FILE_OFFSET_BITS=64 -D_FILE_OFFSET_BITS=64 -I/usr /include/fuse -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -Wall -g -O3 -o fusecram fusemain.o fs.o cramfs.o -L/usr/lib /usr/lib/libfuse.a -lpthread -lrt /usr/lib/libglib-2.0.a /usr/lib/libz.a

clean:
	make -f Makefile_iso clean
	make -f Makefile_cram clean

distclean: clean
	[ ! -d .deps ] || rm -rf .deps
	[ ! -f *.o ] || rm -f *.o
	[ ! -f Makefile_iso ] || rm -f Makefile_iso
	[ ! -f Makefile_cram ] || rm -f Makefile_cram

install:
	mkdir -p $(DESTDIR)/usr/bin
	cp fuseiso $(DESTDIR)/usr/bin/
	cp fusecram $(DESTDIR)/usr/bin/

