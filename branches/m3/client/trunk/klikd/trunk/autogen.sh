# http://www.developingprogrammers.com/index.php/2006/01/05/autotools-tutorial/

#autoreconf --install
aclocal         # aclocal.m4
automake -ac    # Makefile.in
autoheader      # config.h.in
autoconf        # configure
