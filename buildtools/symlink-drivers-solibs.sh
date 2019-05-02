#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# post-install script for meson/ninja builds to symlink the PMDs stored in
# $libdir/dpdk/drivers/ to $libdir. This is needed as some PMDs depend on
# others, e.g. PCI device PMDs depending on the PCI bus driver.

# parameters to script are paths relative to install prefix:
# 1. directory for installed regular libs e.g. lib64
# 2. subdirectory of libdir where the pmds are

cd ${MESON_INSTALL_DESTDIR_PREFIX}/$1 && ln -sfv $2/librte_*.so* .
