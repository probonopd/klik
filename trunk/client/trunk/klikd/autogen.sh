# http://www.developingprogrammers.com/index.php/2006/01/05/autotools-tutorial/

#autoreconf --install
aclocal         # aclocal.m4
autoheader      # config.h.in
automake -ac    # Makefile.in
autoconf        # configure
