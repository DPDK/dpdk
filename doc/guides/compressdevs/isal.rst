..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

ISA-L Compression Poll Mode Driver
==================================

The ISA-L PMD (**librte_pmd_isal_comp**) provides poll mode compression &
decompression driver support for utilizing Intel ISA-L library,
which implements the deflate algorithm for both compression and decompression

Features
--------

ISA-L PMD has support for:

Compression/Decompression algorithm:

* DEFLATE

Huffman code type:

* FIXED
* DYNAMIC

Window size support:

* 32K

Limitations
-----------

* Chained mbufs are not supported, for future release.

* Compressdev level 0, no compression, is not supported. ISA-L level 0 used for fixed huffman codes.

* Checksums are not supported, for future release.

Installation
------------

* To build DPDK with Intel's ISA-L library, the user is required to download the library from `<https://github.com/01org/isa-l>`_.

* Once downloaded, the user needs to build the library, the ISA-L autotools are usually sufficient::

    ./autogen.sh
    ./configure

* make can  be used to install the library on their system, before building DPDK::

    make
    sudo make install

* To build with meson, the **libisal.pc** file, must be copied into "pkgconfig",
  e.g. /usr/lib/pkgconfig or /usr/lib64/pkgconfig depending on your system,
  for meson to find the ISA-L library. The **libisal.pc** is located in library sources::

    cp isal/libisal.pc /usr/lib/pkgconfig/


Initialization
--------------

In order to enable this virtual compression PMD, user must:

* Set ``CONFIG_RTE_LIBRTE_PMD_ISAL=y`` in config/common_base.

To use the PMD in an application, user must:

* Call ``rte_vdev_init("compress_isal")`` within the application.

* Use ``--vdev="compress_isal"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameter (optional) can be provided in the previous two calls:

* ``socket_id:`` Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).
