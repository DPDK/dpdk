..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 ARM Corporation.

Cross compiling DPDK for aarch64 and aarch32
============================================

This chapter describes how to cross compile DPDK for aarch64 on x86 build
machine and compile 32-bit aarch32 DPDK on aarch64 build machine.

.. note::

   Whilst it is recommended to natively build DPDK on aarch64 (just
   like with x86), it is also possible to cross compile DPDK for aarch64.
   An aarch64 cross compiler GNU toolchain or an LLVM/clang toolchain
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
   ./configure --host=aarch64-linux-gnu CC=aarch64-none-linux-gnu-gcc --prefix=<numa install dir>
   make install

.. note::

   The compiler is ``aarch64-none-linux-gnu-gcc`` if you download GCC
   using the below guide. If you're using a different compiler,
   make sure you're using the proper executable name.

The numa header files and lib file is generated in the include and lib folder
respectively under ``<numa install dir>``.

Meson prerequisites
~~~~~~~~~~~~~~~~~~~

Meson depends on pkgconfig to find the dependencies.
The package ``pkg-config-aarch64-linux-gnu`` is required for aarch64.
To install it in Ubuntu::

   sudo apt install pkg-config-aarch64-linux-gnu

For aarch32, install ``pkg-config-arm-linux-gnueabihf``::

   sudo apt install pkg-config-arm-linux-gnueabihf


GNU toolchain
-------------

.. _obtain_GNU_toolchain:

Get the cross toolchain
~~~~~~~~~~~~~~~~~~~~~~~

The latest GNU cross compiler toolchain can be downloaded from:
https://developer.arm.com/open-source/gnu-toolchain/gnu-a/downloads.

It is always recommended to check and get the latest compiler tool
from the page and use it to generate better code.
As of this writing 9.2-2019.12 is the newest,
the following description is an example of this version.

For aarch64::

   wget https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
   tar -xvf gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz
   export PATH=$PATH:<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin

For aarch32::

   wget https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz
   tar -xvf gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz
   export PATH=$PATH:<cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin

.. note::

   For the host requirements and other info, refer to the release note section:
   https://releases.linaro.org/components/toolchain/binaries/

.. _augment_the_gnu_toolchain_with_numa_support:

Augment the GNU toolchain with NUMA support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copy the NUMA header files and lib to the cross compiler's directories:

.. code-block:: console

   cp <numa_install_dir>/include/numa*.h <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/include/
   cp <numa_install_dir>/lib/libnuma.a <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/9.2.1/
   cp <numa_install_dir>/lib/libnuma.so <cross_install_dir>/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/9.2.1/

.. note::

   Using LDFLAGS and CFLAGS is not a viable alternative to copying the files.
   The Meson docs say it is not recommended, as there are many caveats
   to their use with Meson, especially when rebuilding the project.
   A viable alternative would be to use the ``c_args`` and ``c_link_args``
   options with Meson 0.51.0 and higher:

   .. code-block:: console

      -Dc_args=-I<numa_install_dir>/include -Dc_link_args=-L<numa_install_dir>/lib

For Meson versions lower than 0.51.0, the ``c_args`` and ``c_link_args``
options do not apply to cross compilation.
However, the compiler/linker flags may be added to cross files under [properties]:

.. code-block:: console

   c_args = ['-I<numa_install_dir>/include']
   c_link_args = ['-L<numa_install_dir>/lib']

Cross Compiling DPDK with GNU toolchain using Meson
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   The names of GCC binaries in cross files differ from the downloaded ones,
   which have an extra ``-none-`` in their name.
   Please modify the cross file binaries accordingly
   when using the downloaded cross compilers.

   An example cross file with modified names and added NUMA paths
   would look like this:

   .. code-block:: console

      [binaries]
      c = 'aarch64-none-linux-gnu-gcc'
      cpp = 'aarch64-none-linux-gnu-cpp'
      ar = 'aarch64-none-linux-gnu-gcc-ar'
      strip = 'aarch64-none-linux-gnu-strip'
      pkgconfig = 'aarch64-linux-gnu-pkg-config' # the downloaded binaries
         # do not contain a pkgconfig binary, so it is not modified
      pcap-config = ''

      [host_machine]
      system = 'linux'
      cpu_family = 'aarch64'
      cpu = 'armv8-a'
      endian = 'little'

      [properties]
      # Generate binaries that are portable across all Armv8 machines
      platform = 'generic'
      c_args = ['-I<numa_install_dir>/include']  # replace <numa_install_dir>
      c_link_args = ['-L<numa_install_dir>/lib'] # with your path

To cross-compile DPDK on a desired target machine we can use the following
command::

   meson cross-build --cross-file <target_machine_configuration>
   ninja -C cross-build

For example if the target machine is aarch64 we can use the following
command, provided the cross file has been modified accordingly::

   meson aarch64-build-gcc --cross-file config/arm/arm64_armv8_linux_gcc
   ninja -C aarch64-build-gcc

If the target machine is aarch32 we can use the following command,
provided the cross file has been modified accordingly::

   meson aarch32-build --cross-file config/arm/arm32_armv8_linux_gcc
   ninja -C aarch32-build

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

Building for an aarch64 SoC on an aarch64 build machine
-------------------------------------------------------

If you wish to build on an aarch64 build machine for a different aarch64 SoC,
you don't need a separate cross toolchain, just a different set of
configuration options. To build for an aarch64 SoC, use the -Dplatform meson
option::

   meson soc_build -Dplatform=<target_soc>

Substitute <target_soc> with one of the supported SoCs

.. literalinclude:: ../../../config/arm/meson.build
   :start-after: Start of SoCs list
   :end-before: End of SoCs list

These SoCs are also used in cross files, e.g.::

   [properties]
   # Generate binaries that are portable across all Armv8 machines
   platform = 'generic'

Supported SoC configuration
---------------------------

The SoC configuration is a combination of implementer and CPU part number
configuration and SoC-specific configuration::

   soc_<name> = {
      'description': 'SoC Description',  # mandatory
      'implementer': <implementer_id>,   # mandatory
      'part_number': <part_number>,      # mandatory
      'numa': false,  # optional, specify for non-NUMA SoCs
      'enable_drivers': 'common/*,bus/*',  # optional, comma-separated list of
                              # drivers to build, wildcards are accepted
      'disable_drivers': 'crypto/*',       # optional, comma-separated list of
                              # drivers to disable, wildcards are accepted
      'flags': [
         ['RTE_MAX_LCORE', '16'],
         ['RTE_MAX_NUMA_NODES', '1']
      ]               # optional, list of DPDK options that will be added
                      # or overwritten
   }

Where <implementer_id> is a key defined in the implementers dictionary
in config/arm/meson.build (e.g. 0x41) and part_number is a key defined
in implementers[<implementer_id>]['part_number_config'] dictionary
(i.e. the part number must be defined for the implementer,
e.g. for 0x41, a valid value is 0xd49, which is the neoverse-n2 SoC).
