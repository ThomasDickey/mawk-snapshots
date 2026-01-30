Summary: mawk - pattern scanning and text processing language
%global AppProgram mawk
%global AppVersion 1.3.4
%global AppPatched 20260129
%global MySite https://invisible-island.net
# $MawkId: mawk.spec,v 1.149 2026/01/29 11:08:35 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppPatched}
License: GPL-2.0-only
Group: Applications/Development
URL: %{MySite}/%{AppProgram}
Source0: %{MySite}/archives/%{AppProgram}-%{AppVersion}-%{AppPatched}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
mawk is an interpreter for the AWK programming language.  The AWK language is
useful for manipulation of data files, text retrieval and processing, and for
prototyping and experimenting with algorithms.

%prep

%define debug_package %{nil}
%setup -q -n %{AppProgram}-%{AppVersion}-%{AppPatched}
chmod 644 examples/*

%build

INSTALL_PROGRAM='${INSTALL} -p' \
	%configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir}

make

%check
make check

%install
make install DESTDIR=%{buildroot} INSTALL='install -p'

strip $RPM_BUILD_ROOT%{_bindir}/%{AppProgram}


%files
%doc COPYING CHANGES README examples/

%defattr(-,root,root)
%{_prefix}/bin/%{AppProgram}
%{_mandir}/man1/%{AppProgram}.*
%{_mandir}/man7/%{AppProgram}-*.*

%changelog
# each patch should add its ChangeLog entries here

* Thu Jan 29 2026 Thomas E. Dickey
- testing mawk 1.3.4-20260129

* Thu Jan 29 2026 Thomas E. Dickey
- adapt rules from Fedora package script

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
