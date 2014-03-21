Name:     mms-engine
Summary:  MMS engine
Version:  1.0.3
Release:  1
Group:    Communications/Telephony and IM
License:  GPLv2
URL:      https://github.com/nemomobile/mms-engine
Source0:  %{name}-%{version}.tar.bz2
Requires: dbus
Requires: ofono
#Requires: ImageMagick

BuildRequires: python
BuildRequires: file-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: pkgconfig(glib-2.0) >= 2.32
BuildRequires: pkgconfig(libsoup-2.4) >= 2.38
BuildRequires: pkgconfig(libwspcodec) >= 2.1
#BuildRequires: pkgconfig(ImageMagick)
BuildRequires:  pkgconfig(Qt5Gui)

%define src mms-engine
%define exe mms-engine
%define dbusname org.nemomobile.MmsEngine
%define dbusconfig %{_datadir}/dbus-1/system-services
%define dbuspolicy %{_sysconfdir}/dbus-1/system.d

# Activation method:
%define pushconfig %{_sysconfdir}/ofono/push_forwarder.d
#define pushconfig {_sysconfdir}/push-agent
#Requires: push-agent >= 1.1

%description
MMS engine

%package tools
Summary:    MMS tools
Group:      Development/Tools

%description tools
MMS command line utilities

%prep
%setup -q -n %{name}-%{version}

%build
make -C %{src} KEEP_SYMBOLS=1 release
make -C mms-dump KEEP_SYMBOLS=1 release
make -C mms-send KEEP_SYMBOLS=1 release

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{dbusconfig}
mkdir -p %{buildroot}%{dbuspolicy}
mkdir -p %{buildroot}%{pushconfig}
mkdir -p %{buildroot}%{_prefix}/bin/
cp %{src}/build/release/%{exe} %{buildroot}%{_sbindir}/
cp %{src}/%{dbusname}.service %{buildroot}%{dbusconfig}/
cp %{src}/%{dbusname}.dbus.conf %{buildroot}%{dbuspolicy}/%{dbusname}.conf
cp %{src}/%{dbusname}.push.conf %{buildroot}%{pushconfig}/%{dbusname}.conf
cp mms-dump/build/release/mms-dump %{buildroot}%{_prefix}/bin/
cp mms-send/build/release/mms-send %{buildroot}%{_prefix}/bin/

%files
%defattr(-,root,root,-)
%config %{dbuspolicy}/%{dbusname}.conf
%config %{pushconfig}/%{dbusname}.conf
%{dbusconfig}/%{dbusname}.service
%{_sbindir}/%{exe}

%files tools
%defattr(-,root,root,-)
%{_prefix}/bin/mms-dump
%{_prefix}/bin/mms-send
