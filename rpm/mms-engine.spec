Name:     mms-engine
Summary:  MMS engine
Version:  1.0.0
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

%prep
%setup -q -n %{name}-%{version}

%build
make -C %{src} KEEP_SYMBOLS=1 release

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{dbusconfig}
mkdir -p %{buildroot}%{dbuspolicy}
mkdir -p %{buildroot}%{pushconfig}
cp %{src}/build/release/%{exe} %{buildroot}%{_sbindir}/
cp %{src}/%{dbusname}.service %{buildroot}%{dbusconfig}/
cp %{src}/%{dbusname}.dbus.conf %{buildroot}%{dbuspolicy}/%{dbusname}.conf
cp %{src}/%{dbusname}.push.conf %{buildroot}%{pushconfig}/%{dbusname}.conf

%files
%defattr(-,root,root,-)
%config %{dbuspolicy}/%{dbusname}.conf
%config %{pushconfig}/%{dbusname}.conf
%{dbusconfig}/%{dbusname}.service
%{_sbindir}/%{exe}
