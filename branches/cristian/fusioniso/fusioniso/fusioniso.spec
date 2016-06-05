%define reldate 20080515
Name:           fusioniso
Version:        1.0
Release:        0.%{reldate}%{?dist}
Summary:        Mount ISO filesystem images as a non-root user for the klik project

Group:          System Environment/Base
License:        GPL
URL:            http://code.google.com/p/klikclient/
Source0:        %{name}-%{reldate}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  fuse-devel >= 2.2
BuildRequires:  glib2-devel >= 2.2
Requires:       fuse >= 2.2

%description
Mount ISO filesystem images as a non-root user. Currently supports
plain ISO9660 Level 1 and 2, Rock Ridge, Joliet, zisofs. 
Supported image types: ISO, BIN (single track only), NRG, MDF, IMG (CCD).
Add some dedicated features like union mounting or sandboxing useful for the klik project

%prep
%setup -q -n %{name}-%{reldate}


%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING INSTALL NEWS README TODO ChangeLog
%{_bindir}/*


%changelog
* Sun Mar 16 2008 Lionel Tricon <lionel.tricon@gmail.com> - 1.0-0.20080515
- Initial packaging.
