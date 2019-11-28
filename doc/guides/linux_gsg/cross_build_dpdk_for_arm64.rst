..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 ARM Corporation.

Cross compile DPDK for ARM64
============================
This chapter describes how to cross compile DPDK for ARM64 from x86 build hosts.

.. note::

   Whilst it is recommended to natively build DPDK on ARM64 (just
   like with x86), it is also possible to cross-build DPDK for ARM64. An
   ARM64 cross compile GNU toolchain is used for this.

Obtain the cross tool chain
---------------------------
The latest cross compile tool chain can be downloaded from:
https://developer.arm.com/open-source/gnu-toolchain/gnu-a/downloads.

It is always recommended to check and get the latest compiler tool from the page and use
it to generate better code. As of this writing 8.3-2019.03 is the newest, the following
description is an example of this version.

.. code-block:: console

   wget https://developer.arm.com/-/media/Files/downloads/gnu-a/8.3-2019.03/binrel/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu.tar.xz

Unzip and add into the PATH
---------------------------

.. code-block:: console

   tar -xvf gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu.tar.xz
   export PATH=$PATH:<cross_install_dir>/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin

.. note::

   For the host requirements and other info, refer to the release note section: https://releases.linaro.org/components/toolchain/binaries/

.. _arm_cross_build_getting_the_prerequisite_library:

Getting the prerequisite library
--------------------------------

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
   ./configure --host=aarch64-linux-gnu CC=aarch64-linux-gnu-gcc --prefix=<numa install dir>
   make install

The numa header files and lib file is generated in the include and lib folder respectively under <numa install dir>.

.. _augment_the_cross_toolchain_with_numa_support:

Augment the cross toolchain with NUMA support
---------------------------------------------

.. note::

   This way is optional, an alternative is to use extra CFLAGS and LDFLAGS, depicted in :ref:`configure_and_cross_compile_dpdk_build` below.

Copy the NUMA header files and lib to the cross compiler's directories:

.. code-block:: console

   cp <numa_install_dir>/include/numa*.h <cross_install_dir>/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/aarch64-linux-gnu/libc/usr/include/
   cp <numa_install_dir>/lib/libnuma.a <cross_install_dir>/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/lib/gcc/aarch64-linux-gnu/8.3.0/
   cp <numa_install_dir>/lib/libnuma.so <cross_install_dir>/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/lib/gcc/aarch64-linux-gnu/8.3.0/

.. _configure_and_cross_compile_dpdk_build:

Cross Compiling DPDK using Meson
--------------------------------

Meson depends on pkgconfig to find the dependencies.
The package ``pkg-config-aarch64-linux-gnu`` is required for aarch64.
To install it in Ubuntu::

   sudo apt-get install pkg-config-aarch64-linux-gnu

To cross-compile DPDK on a desired target machine we can use the following
command::

	meson cross-build --cross-file <target_machine_configuration>
	ninja -C cross-build

For example if the target machine is arm64 we can use the following
command::

	meson arm64-build --cross-file config/arm/arm64_armv8_linux_gcc
	ninja -C arm64-build

Configure and Cross Compile DPDK using Make
-------------------------------------------
To configure a build, choose one of the target configurations, like arm64-dpaa-linux-gcc and arm64-thunderx-linux-gcc.

.. code-block:: console

   make config T=arm64-armv8a-linux-gcc

To cross-compile, without compiling the kernel modules, use the following command:

.. code-block:: console

   make -j CROSS=aarch64-linux-gnu- CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_EAL_IGB_UIO=n

To cross-compile, including the kernel modules, the kernel source tree needs to be specified by setting
RTE_KERNELDIR:

.. code-block:: console

   make -j CROSS=aarch64-linux-gnu- RTE_KERNELDIR=<kernel_src_rootdir> CROSS_COMPILE=aarch64-linux-gnu-

To compile for non-NUMA targets, without compiling the kernel modules, use the following command:

.. code-block:: console

   make -j CROSS=aarch64-linux-gnu- CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_EAL_IGB_UIO=n CONFIG_RTE_LIBRTE_VHOST_NUMA=n CONFIG_RTE_EAL_NUMA_AWARE_HUGEPAGES=n

.. note::

   1. EXTRA_CFLAGS and EXTRA_LDFLAGS should be added to include the NUMA headers and link the library respectively,
   if the above step :ref:`augment_the_cross_toolchain_with_numa_support` was skipped therefore the toolchain was not
   augmented with NUMA support.

   2. "-isystem <numa_install_dir>/include" should be add to EXTRA_CFLAGS, otherwise the numa.h file will get a lot of compiling
   errors of Werror=cast-qual, Werror=strict-prototypes and Werror=old-style-definition.

   An example is given below:

   .. code-block:: console

      make -j CROSS=aarch64-linux-gnu- CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_EAL_IGB_UIO=n EXTRA_CFLAGS="-isystem <numa_install_dir>/include" EXTRA_LDFLAGS="-L<numa_install_dir>/lib -lnuma"
