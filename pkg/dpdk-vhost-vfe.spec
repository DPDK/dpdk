# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014 6WIND S.A.
# Copyright 2018 Luca Boccassi <bluca@debian.org>

%undefine _include_gdb_index

%define __arch_install_post QA_SKIP_RPATHS=2 %{__arch_install_post}

# debug symbols add ~300 MB of storage requirement on OBS per target
%define debug_package %{nil}
%bcond_with bluefield

Name: dpdk-vhost-vfe
Version: 1.2.rc1
Release: 1%{?dist}
Packager: dev@dpdk.org
URL: http://dpdk.org
Source: http://dpdk.org/browse/dpdk/snapshot/dpdk-vhost-vfe-%{version}.tar.gz

Summary: Data Plane Development Kit core
Group: System Environment/Libraries
License: BSD-3-Clause AND GPL-2.0-only AND LGPL-2.1-only

ExclusiveArch: i686 x86_64 aarch64 ppc64 ppc64le armv7l armv7hl

%global dst_prefix /opt/mellanox/dpdk-vhost-vfe
%global dst_lib lib64

%ifarch aarch64
%global machine armv8a
%global target arm64-%{machine}-linux-gcc
%global config arm64-%{machine}-linux-gcc
%else
%ifarch armv7l armv7hl
%global machine armv7a
%global target arm-%{machine}-linux-gcc
%global config arm-%{machine}-linux-gcc
%else
%ifarch ppc64 ppc64le
%global machine power8
%global target ppc_64-%{machine}-linux-gcc
%global config ppc_64-%{machine}-linux-gcc
%else
%global machine default
%global target %{_arch}-%{machine}-linux-gcc
%global config %{_arch}-native-linux-gcc
%endif
%endif
%endif

%define _unpackaged_files_terminate_build 0

BuildRequires: zlib-devel, meson, libev-devel
%if ! 0%{?rhel_version}
BuildRequires: libpcap-devel
%endif
%if 0%{?suse_version}
%ifnarch armv7l armv7hl
BuildRequires: libnuma-devel
%endif
BuildRequires: libelf-devel
BuildRequires:  %{kernel_module_package_buildreqs}
%if 0%{?sle_version} >= 150000
BuildRequires:  rdma-core-devel
%endif
%else
%if 0%{?fedora} || 0%{?rhel_version} || 0%{?centos_version}
%ifnarch armv7l armv7hl
BuildRequires: numactl-devel
%endif
BuildRequires: elfutils-libelf-devel
BuildRequires: kernel-devel, kernel-headers
%endif
%endif

Requires: libev

%description
DPDK core includes kernel modules, core libraries and tools.
testpmd application allows to test fast packet processing environments
on x86 platforms. For instance, it can be used to check that environment
can support fast path applications such as 6WINDGate, pktgen, rumptcpip, etc.
More libraries are available as extensions in other packages.

%package devel
Summary: Data Plane Development Kit for development
Requires: %{name}%{?_isa} = %{version}-%{release}
Provides: pkgconfig(libdpdk) = %{version}-%{release}
%description devel
DPDK devel is a set of makefiles, headers and examples
for fast packet processing on x86 platforms.

%prep
%setup -q
MESON_PARAMS=%{?meson_params}
ENABLED_DRVS="vdpa/virtio,common/virtio,common/virtio_mi,common/virtio_ha"

%if %{with bluefield}
MESON_PARAMS="$MESON_PARAMS --cross-file config/arm/arm64_bluefield_linux_native_gcc"
MACHINE=native
%else
%ifarch aarch64
MACHINE=native
%else
MACHINE=default
%endif
%endif

CFLAGS="$CFLAGS -fcommon -Werror" meson %{target} -Dexamples=vdpa -Dc_args='-DRTE_LIBRTE_VDPA_DEBUG' --debug -Dprefix=%{dst_prefix} -Dlibdir=%{dst_prefix}/%{dst_lib} --includedir=include/dpdk -Dmachine=$MACHINE -Dmax_ethports=1024 -Denable_drivers=$ENABLED_DRVS -Dtests=false -Ddrivers_install_subdir=dpdk/pmds --default-library=shared $MESON_PARAMS

%build
%{__ninja} -v -C %{target}

%install
rm -rf %{buildroot}
DESTDIR=%{buildroot} %{__ninja} -v -C %{target} install

# Create ld.so.conf.d entry
mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d
cat > %{buildroot}%{_sysconfdir}/ld.so.conf.d/%{name}-%{_arch}.conf << EOF
%{dst_prefix}/%{dst_lib}
EOF

# Export PKG_CONFIG_PATH
mkdir -p %{buildroot}%{_sysconfdir}/profile.d
cat > %{buildroot}%{_sysconfdir}/profile.d/%{name}-%{_arch}.sh << 'EOF'
export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:%{dst_prefix}/%{dst_lib}/pkgconfig
EOF

%files
%dir %{dst_prefix}/%{dst_lib}/dpdk
%dir %{dst_prefix}/%{dst_lib}/dpdk/pmds
%{dst_prefix}/bin/vfe-vhost*
%{dst_prefix}/bin/vhostmgmt
%{dst_prefix}/bin/dpdk-vfe-vdpa
%{dst_prefix}/bin/dpdk-virtio-ha
%{dst_prefix}/%{dst_lib}/*.so.*
%{dst_prefix}/%{dst_lib}/dpdk/*/*.so.*
/etc/ld.so.conf.d/%{name}-%{_arch}.conf
/usr/lib/systemd/system/*

%files devel
%{dst_prefix}/include/dpdk
%{dst_prefix}/%{dst_lib}/*.so
%{dst_prefix}/%{dst_lib}/dpdk/*/*.so
%{dst_prefix}/%{dst_lib}/*.a
%{dst_prefix}/%{dst_lib}/pkgconfig/*.pc
/etc/ld.so.conf.d/%{name}-%{_arch}.conf
/etc/profile.d/%{name}-%{_arch}.sh

%post
/sbin/ldconfig
%ifarch %{ix86}
/sbin/depmod
%endif

%postun
/sbin/ldconfig
%ifarch %{ix86}
/sbin/depmod
%endif
