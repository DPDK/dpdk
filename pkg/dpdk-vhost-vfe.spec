# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014 6WIND S.A.
# Copyright 2018 Luca Boccassi <bluca@debian.org>

%undefine _include_gdb_index

%define __arch_install_post QA_SKIP_RPATHS=2 %{__arch_install_post}

# debug symbols add ~300 MB of storage requirement on OBS per target
%define debug_package %{nil}

Name: dpdk-vhost-vfe
Version: 24.07.rc3
Release: 1%{?dist}
Packager: dev@dpdk.org
URL: https://github.com/Mellanox/dpdk-vhost-vfe
Source: https://github.com/Mellanox/dpdk-vhost-vfe/archive/refs/tags/dpdk-vhost-vfe-%{version}.tar.gz

Summary: Vhost Acceleration for VirtIO VF PCI devices
Group: System Environment/Libraries
License: BSD-3-Clause AND GPL-2.0-only AND LGPL-2.1-only

ExclusiveArch: x86_64

%global dst_prefix /opt/mellanox/dpdk-vhost-vfe
%global dst_lib lib64

%define _unpackaged_files_terminate_build 0

BuildRequires: zlib-devel, meson, libev-devel, libuuid-devel, numactl-devel
Requires: libev, libuuid, zlib, numactl-libs

%description
Virtio VF PCIe devices can be attached to the guest VM using vhost acceleration software stack.
This enables performing live migration of guest VMs.
Vhost acceleration software stack is built using open-source BSD licensed DPDK.


%prep
%setup -q
MESON_PARAMS=%{?meson_params}
ENABLED_DRVS="vdpa/virtio,common/virtio,common/virtio_mi,common/virtio_ha"
MACHINE=default
CFLAGS="$CFLAGS -fcommon -Werror" meson %{target} -Dexamples=vdpa -Dc_args='-DRTE_LIBRTE_VDPA_DEBUG' --debug -Dprefix=%{dst_prefix} -Dlibdir=%{dst_prefix}/%{dst_lib} --includedir=include/dpdk -Dmachine=$MACHINE -Dmax_ethports=1024 -Denable_drivers=$ENABLED_DRVS -Dtests=false -Ddrivers_install_subdir=dpdk/pmds --default-library=static -Dlog_ts=true $MESON_PARAMS

%build
%{__ninja} -v -C %{target}

%install
rm -rf %{buildroot}
DESTDIR=%{buildroot} %{__ninja} -v -C %{target} install


%files
%{dst_prefix}/bin/vfe-vhost*
%{dst_prefix}/bin/vhostmgmt
%{dst_prefix}/bin/dpdk-vfe-vdpa
%{dst_prefix}/bin/dpdk-virtio-ha
%{dst_prefix}/bin/check_pf_reset.sh
%{dst_prefix}/doc/vhostd.md
/usr/lib/systemd/system/*
