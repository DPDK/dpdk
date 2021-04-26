#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

import os
import sys
import glob
import shutil

# post-install script for meson/ninja builds to symlink the PMDs stored in
# $libdir/dpdk/pmds-*/ to $libdir. This is needed as some PMDs depend on
# others, e.g. PCI device PMDs depending on the PCI bus driver.

# parameters to script are paths relative to install prefix:
# 1. directory for installed regular libs e.g. lib64
# 2. subdirectory of libdir where the PMDs are
# 3. directory for installed regular binaries e.g. bin

os.chdir(os.environ['MESON_INSTALL_DESTDIR_PREFIX'])

lib_dir = sys.argv[1]
pmd_subdir = sys.argv[2]
bin_dir = sys.argv[3]
pmd_dir = os.path.join(lib_dir, pmd_subdir)

# copy Windows PMDs to avoid any issues with symlinks since the
# build could be a cross-compilation under WSL, Msys or Cygnus.
# the filenames are dependent upon the specific toolchain in use.

def copy_pmd_files(pattern, to_dir):
	for file in glob.glob(os.path.join(pmd_dir, pattern)):
		to = os.path.join(to_dir, os.path.basename(file))
		shutil.copy2(file, to)
		print(to + ' -> ' + file)

copy_pmd_files('*rte_*.dll', bin_dir)
copy_pmd_files('*rte_*.pdb', bin_dir)
copy_pmd_files('*rte_*.lib', lib_dir)
copy_pmd_files('*rte_*.dll.a', lib_dir)

# symlink shared objects

os.chdir(lib_dir)
for file in glob.glob(os.path.join(pmd_subdir, 'librte_*.so*')):
	to = os.path.basename(file)
	if os.path.exists(to):
		os.remove(to)
	os.symlink(file, to)
	print(to + ' -> ' + file)
