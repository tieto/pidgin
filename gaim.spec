%define name gaim
%define version 0.9.15
%define release 1
%define prefix /usr

Summary: A client compatible with AOL's 'Instant Messenger'

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Internet
Copyright: GPL

Url: http://marko.net/gaim
Packager: rflynn@blueridge.net 
Source: ftp://ftp.marko.net/pub/gaim/gaim-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
Gaim allows you to talk to anyone using AOL's 
Instant Messenger service (you can sign up at http://www.aim.aol.com).  

It uses the TOC version of the AOL protocol, so your buddy list is 
stored on AOL's servers and can be retrieved from anywhere.

It contains many of the same features as AOL's IM client while at
the same time incorporating many new features.

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --enable-plugins
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%attr(755,root,root) %{prefix}/bin/gaim
%doc doc/the_penguin.txt doc/PROTOCOL doc/CREDITS NEWS COPYING AUTHORS doc/FAQ README README.plugins ChangeLog plugins/CRAZY plugins/HOWTO plugins/Makefile plugins/SIGNALS plugins/autorecon.c plugins/chkmail.c plugins/filectl.c plugins/gaiminc.c plugins/iconaway.c plugins/simple.c plugins/spellchk.c

%clean
rm -r $RPM_BUILD_ROOT

