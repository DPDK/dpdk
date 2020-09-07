..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Intel Corporation.

Enable DPDK on OpenWrt
======================

This document describes how to enable Data Plane Development Kit (DPDK) on
OpenWrt in both a virtual and physical x86 environment.

Introduction
------------

The OpenWrt project is a well-known source-based router OS which provides a
fully writable filesystem with package management.

Build OpenWrt
-------------

You can obtain OpenWrt image through https://downloads.openwrt.org/releases.
To fully customize your own OpenWrt, it is highly recommended to build it from
the source code. You can clone the OpenWrt source code as follows:

.. code-block:: console

    git clone https://git.openwrt.org/openwrt/openwrt.git

OpenWrt configuration
~~~~~~~~~~~~~~~~~~~~~

* Select ``x86`` in ``Target System``
* Select ``x86_64`` in ``Subtarget``
* Select ``Build the OpenWrt SDK`` for cross-compilation environment
* Select ``Use glibc`` in ``Advanced configuration options (for developers)``
  then ``ToolChain Options`` and ``C Library implementation``

Kernel configuration
~~~~~~~~~~~~~~~~~~~~

The following configurations should be enabled:

* ``CONFIG_VFIO_IOMMU_TYPE1=y``
* ``CONFIG_VFIO_VIRQFD=y``
* ``CONFIG_VFIO=y``
* ``CONFIG_VFIO_NOIOMMU=y``
* ``CONFIG_VFIO_PCI=y``
* ``CONFIG_VFIO_PCI_MMAP=y``
* ``CONFIG_HUGETLBFS=y``
* ``CONFIG_HUGETLB_PAGE=y``
* ``CONFIG_PROC_PAGE_MONITOR=y``

Build steps
~~~~~~~~~~~

For detailed OpenWrt build steps and prerequisites, please refer to the
`OpenWrt build guide
<https://openwrt.org/docs/guide-developer/build-system/use-buildsystem>`_.

After the build is completed, you can find the images and SDK in
``<OpenWrt Root>/bin/targets/x86/64-glibc/``.


DPDK Cross Compilation for OpenWrt
----------------------------------

Pre-requisites
~~~~~~~~~~~~~~

NUMA is required to run DPDK in x86.

.. note::

    For compiling the NUMA lib, run ``libtool --version`` to ensure the libtool
    version >= 2.2, otherwise the compilation will fail with errors.

.. code-block:: console

    git clone https://github.com/numactl/numactl.git
    cd numactl
    git checkout v2.0.13 -b v2.0.13
    ./autogen.sh
    autoconf -i
    export PATH=<OpenWrt SDK>/glibc/openwrt-sdk-x86-64_gcc-8.3.0_glibc.Linux-x86_64/staging_dir/toolchain-x86_64_gcc-8.3.0_glibc/bin/:$PATH
    ./configure CC=x86_64-openwrt-linux-gnu-gcc --prefix=<OpenWrt SDK toolchain dir>
    make install

The numa header files and lib file is generated in the include and lib folder
respectively under <OpenWrt SDK toolchain dir>.

Build DPDK
~~~~~~~~~~

To cross compile with meson build, you need to write a customized cross file
first.

.. code-block:: console

    [binaries]
    c = 'x86_64-openwrt-linux-gcc'
    cpp = 'x86_64-openwrt-linux-cpp'
    ar = 'x86_64-openwrt-linux-ar'
    strip = 'x86_64-openwrt-linux-strip'

    meson builddir --cross-file openwrt-cross
    ninja -C builddir

Running DPDK application on OpenWrt
-----------------------------------

Virtual machine
~~~~~~~~~~~~~~~

* Extract the boot image

.. code-block:: console

    gzip -d openwrt-x86-64-combined-ext4.img.gz

* Launch Qemu

.. code-block:: console

    qemu-system-x86_64 \
            -cpu host \
            -smp 8 \
            -enable-kvm \
            -M q35 \
            -m 2048M \
            -object memory-backend-file,id=mem,size=2048M,mem-path=/tmp/hugepages,share=on \
            -drive file=<Your OpenWrt images folder>/openwrt-x86-64-combined-ext4.img,id=d0,if=none,bus=0,unit=0 \
            -device ide-hd,drive=d0,bus=ide.0 \
            -net nic,vlan=0 \
            -net nic,vlan=1 \
            -net user,vlan=1 \
            -display none \


Physical machine
~~~~~~~~~~~~~~~~

You can use the ``dd`` tool to write the OpenWrt image to the drive you
want to write the image on.

.. code-block:: console

    dd if=openwrt-18.06.1-x86-64-combined-squashfs.img of=/dev/sdX

Where sdX is name of the drive. (You can find it though ``fdisk -l``)

Running DPDK
~~~~~~~~~~~~

More detailed info about how to run a DPDK application please refer to
``Running DPDK Applications`` section of :ref:`the DPDK documentation <linux_gsg>`.

.. note::

    You need to install pre-built NUMA libraries (including soft link)
    to /usr/lib64 in OpenWrt.
