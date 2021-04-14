..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2020 ARM Corporation.

Cross compiling DPDK for ARM64
==============================
This chapter describes how to cross compile DPDK for ARM64 from x86 build hosts.

.. note::

   Whilst it is recommended to natively build DPDK on ARM64 (just
   like with x86), it is also possible to cross compile DPDK for ARM64.
   An ARM64 cross compiler GNU toolchain or an LLVM/clang toolchain
   may be used for cross-compilation.


Prerequisites
-------------

NUMA library
~~~~~~~~~~~~

NUMA is required by most modern machines, not needed for non-NUMA architectures.

.. note::

   For compiling the NUMA lib, run libtool --version to ensure the libtool version >= 2.2,
   otherwise the compilation will fail with errors.

.. code-block:: console

   git clone https://github.com/numactl/numactl.git
   cd numactl
   git checkout v2.0.13 -b v2.0.13
   ./autogen.sh
   autoconf -i
   ./configure --host=aarch64-linux-gnu CC=<compiler> --prefix=<numa install dir>
   make install

.. note::

   The compiler above can be either aarch64-linux-gnu-gcc or clang.
   See below for information on how to get specific compilers.

The numa header files and lib file is generated in the include and lib folder
respectively under ``<numa install dir>``.

Meson prerequisites
~~~~~~~~~~~~~~~~~~~

Meson depends on pkgconfig to find the dependencies.
The package ``pkg-config-aarch64-linux-gnu`` is required for aarch64.
To install it in Ubuntu::

   sudo apt install pkg-config-aarch64-linux-gnu


GNU toolchain
-------------

.. _obtain_GNU_toolchain:

Obtain the cross toolchain
~~~~~~~~~~~~~~~~~~~~~~~~~~

The latest GNU cross compiler toolchain can be downloaded from:
https://developer.arm.com/open-source/gnu-toolchain/gnu-a/downloads.

It is always recommended to check and get the latest compiler tool
from the page and use it to generate better code.
As of this writing 9.2-2019.12 is the newest,
the following description is an example of this version.

.. code-block:: console

   wget https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz

Unzip and add into the PATH
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   tar -xvf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
   export PATH=$PATH:<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin

.. note::

   For the host requirements and other info, refer to the release note section: https://releases.linaro.org/components/toolchain/binaries/

.. _augment_the_gnu_toolchain_with_numa_support:

Augment the GNU toolchain with NUMA support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   This way is optional, an alternative is to use extra CFLAGS and LDFLAGS.

Copy the NUMA header files and lib to the cross compiler's directories:

.. code-block:: console

   cp <numa_install_dir>/include/numa*.h <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include/
   cp <numa_install_dir>/lib/libnuma.a <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/9.2.1/
   cp <numa_install_dir>/lib/libnuma.so <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/9.2.1/

Cross Compiling DPDK with GNU toolchain using Meson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To cross-compile DPDK on a desired target machine we can use the following
command::

   meson cross-build --cross-file <target_machine_configuration>
   ninja -C cross-build

For example if the target machine is aarch64 we can use the following
command::

   meson aarch64-build-gcc --cross-file config/arm/arm64_armv8_linux_gcc
   ninja -C aarch64-build-gcc


LLVM/Clang toolchain
--------------------

Obtain the cross tool chain
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The latest LLVM/Clang cross compiler toolchain can be downloaded from:
https://developer.arm.com/tools-and-software/open-source-software/developer-tools/llvm-toolchain.

.. code-block:: console

   # Ubuntu binaries
   wget https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz

The LLVM/Clang toolchain does not implement the standard c library.
The GNU toolchain ships an implementation we can use.
Refer to obtain_GNU_toolchain_ to get the GNU toolchain.

Unzip and add into the PATH
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: console

   tar -xvf clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
   export PATH=$PATH:<cross_install_dir>/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04/bin

Cross Compiling DPDK with LLVM/Clang toolchain using Meson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   To use the NUMA library follow the same steps as for
   augment_the_gnu_toolchain_with_numa_support_.

The paths to GNU stdlib must be specified in a cross file.
Augmenting the default cross-file's ``c_args`` and ``c_link_args``
``config/arm/arm64_armv8_linux_clang_ubuntu1804`` would look like this:

.. code-block:: console

   ...
   c_args = ['-target', 'aarch64-linux-gnu', '--sysroot', '<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc']
   c_link_args = ['-target', 'aarch64-linux-gnu', '-fuse-ld=lld', '--sysroot', '<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc', '--gcc-toolchain=<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu']

Assuming the file with augmented ``c_args`` and ``c_link_args``
is named ``arm64_armv8_linux_clang``,
use the following command to cross-compile DPDK for the target machine::

   meson aarch64-build-clang --cross-file config/arm/arm64_armv8_linux_clang
   ninja -C aarch64-build-clang

Cross Compiling DPDK with LLVM/Clang toolchain using Meson on Ubuntu 18.04
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On most popular Linux distribution it is not necessary to download
the toolchains, but rather use the packages provided by said distributions.
On Ubuntu 18.04, these packages are needed:

.. code-block:: console

   sudo apt-get install pkg-config-aarch64-linux-gnu clang llvm llvm-dev lld
   libc6-dev-arm64-cross libatomic1-arm64-cross libgcc-8-dev-arm64-cross

Use the following command to cross-compile DPDK for the target machine::

   meson aarch64-build-clang --cross-file config/arm/arm64_armv8_linux_clang_ubuntu1804
   ninja -C aarch64-build-clang

Supported cross-compilation targets
-----------------------------------

If you wish to build for a target which is not among the current cross-files,
you may use various combinations of implementer/part number::

   Supported implementers:
      'generic': Generic armv8
      '0x41':    Arm
      '0x43':    Cavium
      '0x50':    Ampere Computing
      '0x56':    Marvell ARMADA
      'dpaa':    NXP DPAA

   Supported part_numbers for generic:
      'generic': valid for all armv8-a architectures (unoptimized portable build)

   Supported part_numbers for 0x41, 0x56, dpaa:
      '0xd03':   cortex-a53
      '0xd04':   cortex-a35
      '0xd09':   cortex-a73
      '0xd0a':   cortex-a75
      '0xd0b':   cortex-a76
      '0xd0c':   neoverse-n1

   Supported part_numbers for 0x43:
      '0xa1':    thunderxt88
      '0xa2':    thunderxt81
      '0xa3':    thunderxt83
      '0xaf':    thunderx2t99
      '0xb2':    octeontx2

   Supported part_numbers for 0x50:
      '0x0':     emag

Other cross file options
------------------------

There are other options you may specify in a cross file to tailor the build::

   Supported extra configuration
      max_numa_nodes = n  # will set RTE_MAX_NUMA_NODES
      max_lcores = n      # will set RTE_MAX_LCORE

      numa = false        # set to false to force building for a non-NUMA system
         # if not set or set to true, the build system will build for a NUMA
         # system only if libnuma is installed

      disable_drivers = 'bus/dpaa,crypto/*'  # add disabled drivers
         # valid values are dir/subdirs in the drivers directory
         # wildcards are allowed

      enable_drivers = 'common/*,bus/*'  # build only these drivers
         # valid values are dir/subdirs in the drivers directory
         # wildcards are allowed
