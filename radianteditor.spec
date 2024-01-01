Name:		radianteditor
Version:	0.9.12
Release:	2%{?dist}
Summary:	Level editor for Idtech4 games
Group:		Applications/Editors
License:	GPLv2 and LGPLv2 and BSD
URL:		https://github.com/FriskTheFallenHuman/RadiantEditor
Source0:	%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	automake, autoconf, libtool, desktop-file-utils

%description
 RadiantEditor is a fork base on DarkRadiant this fork aims to make DarkRadiant non tied to The Dark Mod game/engine.

%prep
%setup -q

%build
%configure --enable-darkmod-plugins --enable-debug --prefix=/usr
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
desktop-file-install					\
  --dir=${RPM_BUILD_ROOT}%{_datadir}/applications	\
  ${RPM_BUILD_ROOT}%{_datadir}/applications/radianteditor.desktop

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README
%{_bindir}/*
%{_libdir}/radianteditor/lib*
%{_libdir}/radianteditor/modules
%{_libdir}/radianteditor/scripts
%{_libdir}/radianteditor/plugins/eclasstree*
%{_datadir}/*

%changelog
* Tue Mar 26 2009 ibix <ibixian@gmail.com> - 0.9.12-2
- patches upstream. Removed here.

* Tue Mar 24 2009 ibix <ibixian@gmail.com> - 0.9.12-1
- spec file.
- patch for sound detection on fedora.
- patch for valid desktop entry.

