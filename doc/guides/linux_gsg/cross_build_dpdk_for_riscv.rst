..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 ARM Corporation.
    Copyright(c) 2022 StarFive
    Copyright(c) 2022 SiFive
    Copyright(c) 2022 Semihalf

Cross compiling DPDK for RISC-V
===============================

This chapter describes how to cross compile DPDK for RISC-V from x86 build
hosts.

.. note::

   While it's possible to compile DPDK natively on a RISC-V host, it is
   currently recommended to cross-compile as Linux kernel does not offer any
   way for userspace to discover the vendor and architecture identifiers of the
   CPU and therefore any per-chip optimization options have to be chosen via
   a cross-file or ``c_args``.


Prerequisites
-------------

Ensure that you have all pre-requisites for building DPDK natively as those will
be required also for cross-compilation.


Linux kernel
~~~~~~~~~~~~

Make sure that RISC-V host is running Linux kernel 5.13 or newer. This version
introduces patches necessary for PCIe BAR mapping to userspace.


GNU toolchain
-------------

Obtain the cross toolchain
~~~~~~~~~~~~~~~~~~~~~~~~~~

The build process was tested using:

* Ubuntu toolchain (the ``crossbuild-essential-riscv64`` package).

* Latest `RISC-V GNU toolchain
  <https://github.com/riscv/riscv-gnu-toolchain/releases>`_ on Ubuntu or Arch
  Linux.

Alternatively the toolchain may be built straight from the source, to do that
follow the instructions on the riscv-gnu-toolchain github page.


Unzip and add into the PATH
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This step is only required for the riscv-gnu-toolchain. The Ubuntu toolchain is
in the PATH already.

.. code-block:: console

   tar -xvf riscv64-glibc-ubuntu-20.04-<version>.tar.gz
   export PATH=$PATH:<cross_install_dir>/riscv/bin


Cross Compiling DPDK with GNU toolchain using Meson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To cross-compile DPDK for a desired target machine use the following command::

   meson setup cross-build --cross-file <target_machine_configuration>
   ninja -C cross-build

For example if the target machine is a generic rv64gc RISC-V, use the following
command::

   meson setup riscv64-build-gcc --cross-file config/riscv/riscv64_linux_gcc
   ninja -C riscv64-build-gcc

If riscv-gnu-toolchain is used, binary names should be updated to match. Update
the following lines in the cross-file:

.. code-block:: console

   [binaries]
   c = 'riscv64-unknown-linux-gnu-gcc'
   cpp = 'riscv64-unknown-linux-gnu-g++'
   ar = 'riscv64-unknown-linux-gnu-ar'
   strip = 'riscv64-unknown-linux-gnu-strip'
   ...

Some toolchains (such as freedom-u-sdk one) require also setting ``--sysroot``,
otherwise include paths might not be resolved. To do so, add the appropriate
paths to the cross-file:

.. code-block:: console

   [properties]
   ...
   sys_root = ['--sysroot', '<path/to/toolchain/sysroot>']
   ...


Supported cross-compilation targets
-----------------------------------

Currently the following targets are supported:

* Generic rv64gc ISA: ``config/riscv/riscv64_linux_gcc``

* SiFive U740 SoC: ``config/riscv/riscv64_sifive_u740_linux_gcc``

To add a new target support, ``config/riscv/meson.build`` has to be modified by
adding a new vendor/architecture id and a corresponding cross-file has to be
added to ``config/riscv`` directory.
