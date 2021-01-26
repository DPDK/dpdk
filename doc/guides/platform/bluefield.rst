..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

Mellanox BlueField Board Support Package
========================================

This document has information about steps to setup Mellanox BlueField platform
and common offload HW drivers of **Mellanox BlueField** family SoC.


Supported BlueField family SoCs
-------------------------------

- `BlueField <https://docs.mellanox.com/category/bluefieldsnic>`_


Supported BlueField Platforms
-----------------------------

- `BlueField-1 <https://docs.mellanox.com/display/bluefieldsniceth/Introduction>`_
- `BlueField-2 <https://docs.mellanox.com/display/BlueField2DPUENUG/Introduction>`_


Common Offload HW Drivers
-------------------------

1. **NIC Driver**

   See :doc:`../nics/mlx5` for Mellanox mlx5 NIC driver information.

2. **Cryptodev Driver**

   This is based on the crypto extension support of armv8. See
   :doc:`../cryptodevs/armv8` for armv8 crypto driver information.

.. note::

   BlueField has a variant having no armv8 crypto extension support.


Steps To Setup Platform
-----------------------

Toolchains, OS and drivers can be downloaded and installed individually
from the web, but it is recommended to follow instructions at:

- `Mellanox BlueField-1 Software Website <https://docs.mellanox.com/display/BlueFieldSWv31011424/SmartNIC+Bring-Up+and+Driver+Installation>`_
- `Mellanox BlueField-2 Software Website <https://docs.mellanox.com/display/NVIDIABlueField2DPUQSG#NVIDIABlueField2DPUSoftwareQuickStartGuide-Post-installationProcedurePost-installationProcedure>`_


Compile DPDK
------------

DPDK can be compiled either natively on BlueField platforms or cross-compiled on
an x86 based platform.

Native Compilation
~~~~~~~~~~~~~~~~~~

Refer to :doc:`../nics/mlx5` for prerequisites. Either Mellanox OFED/EN or
rdma-core library with corresponding kernel drivers is required.

.. code-block:: console

        meson build
        ninja -C build

Cross Compilation
~~~~~~~~~~~~~~~~~

Refer to :doc:`../linux_gsg/cross_build_dpdk_for_arm64` to install the cross
toolchain for ARM64. Base on that, additional header files and libraries are
required:

   - libibverbs
   - libmlx5
   - libnl-3
   - libnl-route-3

Such header files and libraries can be cross-compiled and installed
in the cross toolchain environment.
They can also be simply copied from the filesystem of a working BlueField platform.
The following script can be run on a BlueField platform in order to create
a supplementary tarball for the cross toolchain.

.. code-block:: console

        mkdir -p aarch64-linux-gnu/libc
        pushd $PWD
        cd aarch64-linux-gnu/libc

        # Copy libraries
        mkdir -p lib64
        cp -a /lib64/libibverbs* lib64/
        cp -a /lib64/libmlx5* lib64/
        cp -a /lib64/libnl-3* lib64/
        cp -a /lib64/libnl-route-3* lib64/

        # Copy header files
        mkdir -p usr/include/infiniband
        cp -a /usr/include/infiniband/ib_user_ioctl_verbs.h usr/include/infiniband/
        cp -a /usr/include/infiniband/mlx5*.h usr/include/infiniband/
        cp -a /usr/include/infiniband/tm_types.h usr/include/infiniband/
        cp -a /usr/include/infiniband/verbs*.h usr/include/infiniband/

        # Create supplementary tarball
        popd
        tar cf aarch64-linux-gnu-mlx.tar aarch64-linux-gnu/

Then, untar the tarball at the cross toolchain directory on the x86 host.

.. code-block:: console

        cd $(dirname $(which aarch64-linux-gnu-gcc))/..
        tar xf aarch64-linux-gnu-mlx.tar

.. code-block:: console

        meson build --cross-file config/arm/arm64_bluefield_linux_gcc
        ninja -C build
