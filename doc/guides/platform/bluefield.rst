..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Mellanox Technologies, Ltd

Mellanox BlueField Board Support Package
========================================

This document has information about steps to setup Mellanox BlueField platform
and common offload HW drivers of **Mellanox BlueField** family SoC.


Supported BlueField family SoCs
-------------------------------

- `BlueField <http://www.mellanox.com/page/products_dyn?product_family=256&mtag=soc_overview>`_


Supported BlueField Platforms
-----------------------------

- `BlueField SmartNIC <http://www.mellanox.com/page/products_dyn?product_family=275&mtag=bluefield_smart_nic>`_
- `BlueField Reference Platforms <http://www.mellanox.com/page/products_dyn?product_family=286&mtag=bluefield_platforms>`_
- `BlueField Controller Card <http://www.mellanox.com/page/products_dyn?product_family=288&mtag=bluefield_controller_card>`_


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

Toolchains, OS and drivers can be downloaded and installed individually from the
Web. But it is recommended to follow instructions at
`Mellanox BlueField Software Website
<http://www.mellanox.com/page/products_dyn?product_family=279&mtag=bluefield_software>`_.


Compile DPDK
------------

DPDK can be compiled either natively on BlueField platforms or cross-compiled on
an x86 based platform.

Native Compilation
~~~~~~~~~~~~~~~~~~

Refer to :doc:`../nics/mlx5` for prerequisites. Either Mellanox OFED/EN or
rdma-core library with corresponding kernel drivers is required.

make build
^^^^^^^^^^

.. code-block:: console

        make config T=arm64-bluefield-linux-gcc
        make -j

meson build
^^^^^^^^^^^

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

Such header files and libraries can be cross-compiled and installed on to the
cross toolchain directory like depicted in
:ref:`arm_cross_build_getting_the_prerequisite_library`, but those can also be
simply copied from the filesystem of a working BlueField platform. The following
script can be run on a BlueField platform in order to create a supplementary
tarball for the cross toolchain.

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

make build
^^^^^^^^^^

.. code-block:: console

        make config T=arm64-bluefield-linux-gcc
        make -j CROSS=aarch64-linux-gnu- CONFIG_RTE_KNI_KMOD=n CONFIG_RTE_EAL_IGB_UIO=n

meson build
^^^^^^^^^^^

.. code-block:: console

        meson build --cross-file config/arm/arm64_bluefield_linux_gcc
        ninja -C build
