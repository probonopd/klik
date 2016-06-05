Name: isofsthumbnail 
Version: 1.0
Release: 1
Summary: ISOFS-Thumbnail-Plugin for Konqueror
Group: System/GUI/KDE
License: GPL
Vendor: None
Packager: lionel.tricon@free.fr
Requires: kdelibs3 >= %( echo `rpm -q --queryformat '%{VERSION}' kdelibs3`)
URL: 
Source: isofsthumbnail-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-build
Distribution: Kubuntu 7.04

%description
Plugin for Konqueror to preview KLIK2-Icons as Thumbnails.

%prep
%setup 
./configure --prefix=`kde-config --prefix` 
gmake CPPFLAGS='${RPM_OPT_FLAGS}' 

%install
. /etc/opt/kde3/common_options
gmake DESTDIR=$RPM_BUILD_ROOT $INSTALL_TARGET

%files
%doc COPYING TODO README
/opt

%clean
rm -rf %{buildroot}
