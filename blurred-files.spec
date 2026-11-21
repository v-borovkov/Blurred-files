%global debug_package %{nil}

Name:           blurred-files
Version:        0.7.0
Release:        1%{?dist}
Summary:        Fast, lightweight GTK4 file manager with superellipse UI

License:        GPL-3.0-only
URL:            https://github.com
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(gtk4-layer-shell-0)
Requires:       zip
Requires:       unzip
Requires:       tar
Requires:       xdg-utils

%description
A fast, lightweight GTK4 file manager written in C from scratch, featuring a
custom superellipse window silhouette, translucent Wayfire-blurred sidebar,
tactile Neumorphic header controls, and a Wayland wlr-layer-shell desktop mode.

%prep
%autosetup

%build
make PREFIX=%{_prefix} %{?_smp_mflags}

%install
make install PREFIX=%{_prefix} DESTDIR=%{buildroot}
rm -f %{buildroot}%{_datadir}/applications/mimeinfo.cache
rm -f %{buildroot}%{_datadir}/applications/intent.cache

%post
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &> /dev/null || :

%postun
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &> /dev/null || :

%files
%license LICENSE
%doc readme.md
%{_bindir}/blurred_files
%dir %{_datadir}/blurred_files
%{_datadir}/blurred_files/bfiles.css
%{_datadir}/icons/hicolor/scalable/apps/blurred_files.svg
%{_datadir}/applications/blurred_files.desktop

%changelog
* Sat Nov 21 2026 v-borovkov <v-borovkov@://github.com> - 0.7.0-1
- Add Wayfire layer-shell --desktop mode and GPU thumbnail cover widget