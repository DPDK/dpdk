.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2021 Intel Corporation

Running AddressSanitizer
========================

`AddressSanitizer
<https://github.com/google/sanitizers/wiki/AddressSanitizer>`_ (ASan)
is a widely-used debugging tool to detect memory access errors.
It helps to detect issues like use-after-free, various kinds of buffer
overruns in C/C++ programs, and other similar errors, as well as
printing out detailed debug information whenever an error is detected.

AddressSanitizer is a part of LLVM (3.1+) and GCC (4.8+).

Enabling ASan is done by passing the -Db_sanitize=address option to the meson build system,
see :ref:`linux_gsg_compiling_dpdk` for details.

The way ASan is integrated with clang requires to allow undefined symbols when linking code.
To do this, the -Db_lundef=false option must be added.

Additionally, passing -Dbuildtype=debug option might help getting more readable ASan reports.

Example::

  - gcc: meson setup -Db_sanitize=address <build_dir>
  - clang: meson setup -Db_sanitize=address -Db_lundef=false <build_dir>

.. Note::

  - The libasan package must be installed when compiling with gcc in Centos/RHEL.
  - If the program is tested using cmdline, you may need to execute the
    "stty echo" command when an error occurs.
