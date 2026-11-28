# NOTE: `Version:` and the `%changelog` top entry are rewritten by the
# packaging workflow from the top entry of NEWS via
# `build-aux/news-version.py`. Do NOT hand-edit those fields; the placeholder
# values below exist only so `rpmbuild -bs` accepts the spec outside CI.

%global plugin_name xfce4-meowmenu-plugin

Name:           %{plugin_name}
Version:        1.0.0~rc3
Release:        1%{?dist}
Summary:        OU Linux menu launcher plugin for the Xfce panel

License:        GPLv2+
URL:            https://github.com/PatrickNoFilter/xfce4-ou-menu-plugin
%global upstream_version 1.0.0-rc3
Source0:        %{name}-%{upstream_version}.tar.gz

Packager:       PatrickNoFilter <PatrickNoFilter@users.noreply.github.com>

# NOTE: BuildRequires are expressed via pkgconfig(...) / path capabilities
# rather than concrete devel-package names, which keeps the spec portable
# across RPM distributions where the Xfce devel package names diverge (e.g.
# garcon-devel vs libgarcon-1-devel, ninja-build vs ninja); the .pc filenames
# and /usr/bin paths are stable and match meson.build.
BuildRequires:  meson
BuildRequires:  gcc-c++
BuildRequires:  /usr/bin/ninja
BuildRequires:  /usr/bin/pkg-config
BuildRequires:  /usr/bin/msgfmt
BuildRequires:  /usr/bin/git
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  librsvg2
BuildRequires:  pkgconfig(garcon-1)
BuildRequires:  pkgconfig(libxfce4panel-2.0)
BuildRequires:  pkgconfig(libxfce4ui-2)
BuildRequires:  pkgconfig(libxfce4util-1.0)
BuildRequires:  pkgconfig(exo-2)
BuildRequires:  pkgconfig(libxfconf-0)

Requires:       xfce4-panel
Requires:       /usr/bin/exo-open
Recommends:     bc

%description
OU Menu is the OU Linux application launcher for the Xfce panel. It is a
fork of MeowMenu by Matteo Bonanomi, itself a fork of Xfce's Whisker Menu,
with OU branding and an OU Setting Wallpaper page.

%prep
%autosetup -n %{name}-%{upstream_version}

%build
%meson -Daccountsservice=disabled -Dgtk-layer-shell=disabled
%meson_build

%check
%meson_test
%{?meowmenu_testlog:cp -p redhat-linux-build/meson-logs/testlog.txt %{meowmenu_testlog}}

%install
%meson_install
# NOTE: GETTEXT_PACKAGE matches the Meson project name. Locale files install
# under that name, so %find_lang must use it.
%find_lang xfce4-meowmenu-plugin

%files -f xfce4-meowmenu-plugin.lang
%license COPYING
%doc README.md NEWS
%{_bindir}/xfce4-popup-meowmenu
%{_libdir}/xfce4/panel/plugins/lib*.so
%{_datadir}/xfce4/panel/plugins/*.desktop
%{_datadir}/icons/hicolor/*/apps/*meowmenu*
%{_datadir}/meowmenu/presets/
%{_datadir}/xfce4-meowmenu-plugin/
%{_datadir}/metainfo/io.github.PatrickNoFilter.xfce4-ou-menu-plugin.metainfo.xml
%{_mandir}/man1/xfce4-popup-meowmenu.1*

%changelog
* Sat Nov 28 2026 PatrickNoFilter <PatrickNoFilter@users.noreply.github.com> - 1.0.0~rc3-1
- OU Menu branding and OU Setting Wallpaper page with custom image picker.
