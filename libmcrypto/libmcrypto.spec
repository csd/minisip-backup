%define name		libmcrypto
%define version		0.8.0
%define release		1

%define major		0
%define lib_name	%{name}%{major}

Summary: 		A C++ library providing various network utilities
Name:			%{name}
Version:		%{version}
Release:		%{release}
Packager:		Erik Eliasson <ere@kth.se>
License:		GPL
URL:			http://www.minisip.org/
Group:			System/Libraries
Source:			http://www.minisip.org/source/%{name}-%{version}.tar.gz
BuildRoot:		%_tmppath/%name-%version-%release-root

%description
Libmcrypto is a library providing C++ cryptography related utilities.
It is used by the Minisip project.

%package -n %{lib_name}
Summary: 		A C++ library providing various utilities
Group:			System/Libraries
Provides:		%{name}
Requires:       	openssl >= 0.9.6
Requires:       	libmnetutil0 >= 0.8.0


%description -n %{lib_name}
Libmcrypto is a library providing C++ cryptography related utilities.
It is used by the Minisip project.



%package -n %{lib_name}-devel
Summary: 		Development files for the libmcrypto library.
Group:          	Development/C
Provides:       	%name-devel
Requires:       	%{lib_name} = %{version}


%description -n %{lib_name}-devel
Libmcrypto is a library providing C++ cryptography related utilities.
It is used by the Minisip project.

This package includes the development files (headers and static library)

%prep
%setup -q


%build
%configure
make

%install
%makeinstall

%clean
rm -rf %buildroot

%post -n %{lib_name} -p /sbin/ldconfig

%postun -n %{lib_name} -p /sbin/ldconfig
										

%files -n %{lib_name}
%defattr(-,root,root,-)
%doc AUTHORS README COPYING ChangeLog
%{_libdir}/*.so.*

%files -n %{lib_name}-devel
%defattr(-,root,root)
%doc COPYING
%{_libdir}/*a
%{_libdir}/*.so
%{_includedir}/*


%changelog
* Thu Oct 11 2008 Erik Eliasson <ere@kth.se>
- initial release
