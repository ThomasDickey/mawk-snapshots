Summary: mawk - pattern scanning and text processing language
%global AppProgram mawk
%global AppVersion 1.3.4
%global AppPatched 20260109
%global MySite https://invisible-island.net
# $MawkId: mawk.spec,v 1.143 2026/01/10 00:12:00 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppPatched}
License: GPLv2
Group: Applications/Development
URL: %{MySite}/%{AppProgram}
Source0: %{MySite}/archives/%{AppProgram}-%{AppVersion}-%{AppPatched}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
mawk is an interpreter for the AWK Programming Language.  The AWK language is
useful for manipulation of data files, text retrieval and processing, and for
prototyping and experimenting with algorithms.

%prep

%define debug_package %{nil}
%setup -q -n %{AppProgram}-%{AppVersion}-%{AppPatched}

%build

INSTALL_PROGRAM='${INSTALL}' \
	%configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir}

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

strip $RPM_BUILD_ROOT%{_bindir}/%{AppProgram}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_prefix}/bin/%{AppProgram}
%{_mandir}/man1/%{AppProgram}.*
%{_mandir}/man7/%{AppProgram}-*.*

%changelog
# each patch should add its ChangeLog entries here

* Fri Jan 09 2026 Thomas E. Dickey
- testing mawk 1.3.4-20260109

* Wed Aug 02 2023 Thomas Dickey
- add man7-pages for array/code

* Thu Dec 29 2022 Thomas Dickey
- update URLs

* Mon Jan 06 2020 Thomas Dickey
- use hardening flags

* Sat Oct 27 2012 Thomas Dickey
- cancel any debug-rpm

* Sun Jun 20 2010 Thomas Dickey
- initial version
