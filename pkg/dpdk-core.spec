# Copyright 2014 6WIND S.A.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution.
#
# - Neither the name of 6WIND S.A. nor the names of its
#   contributors may be used to endorse or promote products derived
#   from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.

# name includes full version because there is no ABI stability yet
Name: dpdk-core-1.6.0
Version: r2
%define fullversion 1.6.0%{version}
Release: 1
Packager: packaging@6wind.com
URL: http://dpdk.org
Source: http://dpdk.org/browse/dpdk/snapshot/dpdk-%{fullversion}.tar.gz

Summary: Intel(r) Data Plane Development Kit core
Group: System Environment/Libraries
License: BSD and LGPLv2 and GPLv2

ExclusiveArch: i686, x86_64
%define target %{_arch}-native-linuxapp-gcc
%define machine default

BuildRequires: kernel-devel, kernel-headers, doxygen

%description
Intel(r) DPDK core includes kernel modules, core libraries and tools.
testpmd application allows to test fast packet processing environments
on x86 platforms. For instance, it can be used to check that environment
can support fast path applications such as 6WINDGate, pktgen, rumptcpip, etc.
More libraries are available as extensions in other packages.

%package devel
Summary: Intel(r) Data Plane Development Kit core for development
%description devel
Intel(r) DPDK core-devel is a set of makefiles, headers, examples and documentation
for fast packet processing on x86 platforms.
More libraries are available as extensions in other packages.

%define destdir %{buildroot}%{_prefix}
%define moddir  /lib/modules/%(uname -r)/extra
%define datadir %{_datadir}/dpdk
%define docdir  %{_docdir}/dpdk

%prep
%setup -qn dpdk-%{fullversion}

%build
make O=%{target} T=%{target} config
sed -ri 's,(RTE_MACHINE=).*,\1%{machine},' %{target}/.config
sed -ri 's,(RTE_APP_TEST=).*,\1n,'         %{target}/.config
sed -ri 's,(RTE_BUILD_SHARED_LIB=).*,\1y,' %{target}/.config
make O=%{target} %{?_smp_mflags}
make O=%{target} doc

%install
rm -rf %{buildroot}
make           O=%{target}     DESTDIR=%{destdir}
mkdir -p                               %{buildroot}%{moddir}
mv    %{destdir}/%{target}/kmod/*.ko   %{buildroot}%{moddir}
rmdir %{destdir}/%{target}/kmod
mkdir -p                               %{buildroot}%{_sbindir}
ln -s %{datadir}/tools/igb_uio_bind.py %{buildroot}%{_sbindir}/igb_uio_bind
mkdir -p                               %{buildroot}%{_bindir}
mv    %{destdir}/%{target}/app/testpmd %{buildroot}%{_bindir}
rmdir %{destdir}/%{target}/app
mv    %{destdir}/%{target}/include     %{buildroot}%{_includedir}
mv    %{destdir}/%{target}/lib         %{buildroot}%{_libdir}
mkdir -p                               %{buildroot}%{docdir}
mv    %{destdir}/%{target}/doc/*       %{buildroot}%{docdir}
rmdir %{destdir}/%{target}/doc
mkdir -p                               %{buildroot}%{datadir}
mv    %{destdir}/%{target}/.config     %{buildroot}%{datadir}/config
mv    %{destdir}/%{target}             %{buildroot}%{datadir}
mv    %{destdir}/mk                    %{buildroot}%{datadir}
cp -a            examples              %{buildroot}%{datadir}
cp -a            tools                 %{buildroot}%{datadir}
ln -s            %{datadir}/config     %{buildroot}%{datadir}/%{target}/.config
ln -s            %{_includedir}        %{buildroot}%{datadir}/%{target}/include
ln -s            %{_libdir}            %{buildroot}%{datadir}/%{target}/lib

%files
%dir %{datadir}
%{datadir}/config
%{datadir}/tools
%{moddir}/*
%{_sbindir}/*
%{_bindir}/*
%{_libdir}/*

%files devel
%{_includedir}/*
%{datadir}/mk
%{datadir}/%{target}
%{datadir}/examples
%doc %{docdir}

%post
/sbin/ldconfig
/sbin/depmod

%postun
/sbin/ldconfig
/sbin/depmod
