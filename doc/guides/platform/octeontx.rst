..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Cavium, Inc

OCTEON TX Board Support Package
===============================

This doc has information about steps to setup OCTEON TX platform
and information about common offload hw block drivers of
**Cavium OCTEON TX** SoC family.


More information about SoC can be found at `Cavium, Inc Official Website
<http://www.cavium.com/OCTEON-TX_ARM_Processors.html>`_.

Common Offload HW Block Drivers
-------------------------------

1. **Crypto Driver**
   See :doc:`../cryptodevs/octeontx` for octeontx crypto driver
   information.

2. **Eventdev Driver**
   See :doc:`../eventdevs/octeontx` for octeontx ssovf eventdev driver
   information.

3. **Mempool Driver**
   See :doc:`../mempool/octeontx` for octeontx fpavf mempool driver
   information.

Steps To Setup Platform
-----------------------

There are three main pre-prerequisites for setting up Platform drivers on
OCTEON TX compatible board:

1. **OCTEON TX Linux kernel PF driver for Network acceleration HW blocks**

   The OCTEON TX Linux kernel drivers (includes the required PF driver for the
   Platform drivers) are available on Github at `octeontx-kmod <https://github.com/caviumnetworks/octeontx-kmod>`_
   along with build, install and dpdk usage instructions.

.. note::

   The PF driver and the required microcode for the crypto offload block will be
   available with OCTEON TX SDK only. So for using crypto offload, follow the steps
   mentioned in :ref:`setup_platform_using_OCTEON_TX_SDK`.

2. **ARM64 Tool Chain**

   For example, the *aarch64* Linaro Toolchain, which can be obtained from
   `here <https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/aarch64-linux-gnu>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `<http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

   As an alternative method, Platform drivers can also be executed using images provided
   as part of SDK from Cavium. The SDK includes all the above prerequisites necessary
   to bring up a OCTEON TX board. Please refer :ref:`setup_platform_using_OCTEON_TX_SDK`.

- Follow the DPDK :doc:`../linux_gsg/index` to setup the basic DPDK environment.

.. _setup_platform_using_OCTEON_TX_SDK:

Setup Platform Using OCTEON TX SDK
----------------------------------

The OCTEON TX platform drivers can be compiled either natively on
**OCTEON TX** :sup:`®` board or cross-compiled on an x86 based platform.

The **OCTEON TX** :sup:`®` board must be running the linux kernel based on
OCTEON TX SDK 6.2.0 patch 3. In this, the PF drivers for all hardware
offload blocks are already built in.

Native Compilation
~~~~~~~~~~~~~~~~~~

If the kernel and modules are cross-compiled and copied to the target board,
some intermediate binaries required for native build would be missing on the
target board. To make sure all the required binaries are available in the
native architecture, the linux sources need to be compiled once natively.

.. code-block:: console

        cd /lib/modules/$(uname -r)/source
        make menuconfig
        make

The above steps would rebuild the modules and the required intermediate binaries.
Once the target is ready for native compilation, the OCTEON TX platform
drivers can be compiled with the following steps,

.. code-block:: console

        cd <dpdk directory>
        make config T=arm64-thunderx-linux-gcc
        make

The example applications can be compiled using the following:

.. code-block:: console

        cd <dpdk directory>
        export RTE_SDK=$PWD
        export RTE_TARGET=build
        cd examples/<application>
        make

Cross Compilation
~~~~~~~~~~~~~~~~~

The DPDK applications can be cross-compiled on any x86 based platform. The
OCTEON TX SDK need to be installed on the build system. The SDK package will
provide the required toolchain etc.

Refer to :doc:`../linux_gsg/cross_build_dpdk_for_arm64` for further steps on
compilation. The 'host' & 'CC' to be used in the commands would change,
in addition to the paths to which libnuma related files have to be
copied.

The following steps can be used to perform cross-compilation with OCTEON TX
SDK 6.2.0 patch 3:

.. code-block:: console

        cd <sdk_install_dir>
        source env-setup

        git clone https://github.com/numactl/numactl.git
        cd numactl
        git checkout v2.0.11 -b v2.0.11
        ./autogen.sh
        autoconf -i
        ./configure --host=aarch64-thunderx-linux CC=aarch64-thunderx-linux-gnu-gcc --prefix=<numa install dir>
        make install

The above steps will prepare build system with numa additions. Now this build system can be used
to build applications for **OCTEON TX** :sup:`®` platforms.

.. code-block:: console

        cd <dpdk directory>
        export RTE_SDK=$PWD
        export RTE_KERNELDIR=$THUNDER_ROOT/linux/kernel/linux
        make config T=arm64-thunderx-linux-gcc
        make -j CROSS=aarch64-thunderx-linux-gnu- CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_EAL_IGB_UIO=n EXTRA_CFLAGS="-isystem <numa_install_dir>/include" EXTRA_LDFLAGS="-L<numa_install_dir>/lib -lnuma"

If NUMA support is not required, it can be disabled as explained in
:doc:`../linux_gsg/cross_build_dpdk_for_arm64`.

Following steps could be used in that case.

.. code-block:: console

        make config T=arm64-thunderx-linux-gcc
        make CROSS=aarch64-thunderx-linux-gnu-


SDK and related information can be obtained from: `Cavium support site <https://support.cavium.com/>`_.
