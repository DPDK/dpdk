..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Cavium Networks.

ZLIB Compression Poll Mode Driver
==================================

The ZLIB PMD (**librte_compress_zlib**) provides poll mode compression &
decompression driver based on SW zlib library,

Features
--------

ZLIB PMD has support for:

Compression/Decompression algorithm:

* DEFLATE

Huffman code type:

* FIXED
* DYNAMIC

Window size support:

* Min - 256 bytes
* Max - 32K

Limitations
-----------

* Scatter-Gather and Stateful not supported.

Installation
------------

* To build DPDK with ZLIB library, the user is required to download the ``libz`` library.
* Use following command for installation.

* For Fedora users::
     sudo yum install zlib-devel
* For Ubuntu users::
     sudo apt-get install zlib1g-dev

* Once downloaded, the user needs to build the library.

* To build from sources
  download zlib sources from http://zlib.net/ and do following before building DPDK::

    make
    sudo make install

Initialization
--------------

To use the PMD in an application, user must:

* Call ``rte_vdev_init("compress_zlib")`` within the application.

* Use ``--vdev="compress_zlib"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameter (optional) can be provided in the previous two calls:

* ``socket_id:`` Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).
