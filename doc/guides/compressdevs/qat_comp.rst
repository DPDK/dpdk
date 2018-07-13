..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

Intel(R) QuickAssist (QAT) Compression Poll Mode Driver
=======================================================

The QAT compression PMD provides poll mode compression & decompression driver
support for the following hardware accelerator devices:

* ``Intel QuickAssist Technology C62x``
* ``Intel QuickAssist Technology C3xxx``


Features
--------

QAT compression PMD has support for:

Compression/Decompression algorithm:

    * DEFLATE

Huffman code type:

    * FIXED

Window size support:

    * 32K

Checksum generation:

    * CRC32, Adler and combined checksum

Limitations
-----------

* Chained mbufs are not yet supported, therefore max data size which can be passed to the PMD in a single mbuf is 64K - 1. If data is larger than this it will need to be split up and sent as multiple operations.

* Compressdev level 0, no compression, is not supported.

* Dynamic Huffman encoding is not yet supported.

Installation
------------

The QAT compression PMD is built by default with a standard DPDK build.

It depends on a QAT kernel driver, see :ref:`qat_kernel_installation`.
