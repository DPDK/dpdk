.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024-2025 Huawei Technologies Co.,Ltd. All rights reserved.
   Copyright 2024-2025 Linaro ltd.

UADK Compression Poll Mode Driver
=================================

UADK compression PMD provides poll mode compression & decompression driver.
All compression operations are using UADK library compression API,
which is algorithm-level API, abstracting accelerators' low-level implementations.

UADK compression PMD relies on `UADK library <https://github.com/Linaro/uadk>`_.

UADK is a framework for user applications to access hardware accelerators.
UADK relies on IOMMU SVA (Shared Virtual Address) feature,
which shares the same page table between IOMMU and MMU.
As a result, the user application can directly use the virtual address for device DMA,
which enhances performance as well as easy usability.


Features
--------

UADK compression PMD has support for:

Compression/Decompression algorithm:

    * DEFLATE - using Fixed and Dynamic Huffman encoding

Window size support:

    * 32K


Test steps
----------

#. Build UADK

   .. code-block:: console

      git clone https://github.com/Linaro/uadk.git
      cd uadk
      mkdir build
      ./autogen.sh
      ./configure --prefix=$PWD/build
      make
      make install

   .. note::

      Without ``--prefix``, UADK will be installed to ``/usr/local/lib`` by default.

   .. note::

      If get error: "cannot find -lnuma", please install the libnuma-dev.

#. Run pkg-config libwd to ensure env is setup correctly

   .. code-block:: console

      export PKG_CONFIG_PATH=$PWD/build/lib/pkgconfig
      pkg-config libwd --cflags --libs -I/usr/local/include -L/usr/local/lib -lwd

   .. note::

      export ``PKG_CONFIG_PATH`` is required on demand,
      not needed if UADK is installed to ``/usr/local/lib``.

#. Build DPDK

   .. code-block:: console

      cd dpdk
      mkdir build
      meson setup build (--reconfigure)
      cd build
      ninja
      sudo ninja install

#. Prepare hugepages for DPDK (see also :doc:`../tools/hugepages`)

   .. code-block:: console

      echo 1024 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node2/hugepages/hugepages-2048kB/nr_hugepages
      echo 1024 > /sys/devices/system/node/node3/hugepages/hugepages-2048kB/nr_hugepages
      mkdir -p /mnt/huge_2mb
      mount -t hugetlbfs none /mnt/huge_2mb -o pagesize=2MB

#. Run test app

   .. code-block:: console

	sudo dpdk-test --vdev=compress_uadk
	RTE>>compressdev_autotest
	RTE>>quit
