#! /bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# post-install script for meson/ninja builds to symlink the PMDs stored in
# $libdir/dpdk/drivers/ to $libdir. This is needed as some PMDs depend on
# others, e.g. PCI device PMDs depending on the PCI bus driver.

# parameters to script are paths relative to install prefix:
# 1. directory containing driver files e.g. lib64/dpdk/drivers
# 2. directory for installed regular libs e.g. lib64
ln -sf ${DESTDIR}/${MESON_INSTALL_PREFIX}/$1/* ${DESTDIR}/${MESON_INSTALL_PREFIX}/$2
