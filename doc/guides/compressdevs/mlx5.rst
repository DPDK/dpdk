.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 Compress Driver
====================

The mlx5 compress driver library
(**librte_compress_mlx5**) provides support for **Mellanox BlueField-2**
families of 25/50/100/200 Gb/s adapters.

Design
------

This PMD is configuring the compress, decompress amd DMA engines.

GGAs (Generic Global Accelerators) are offload engines that can be used
to do memory to memory tasks on data.
These engines are part of the ARM complex of the BlueField chip, and as
such they do not use NIC related resources (e.g. RX/TX bandwidth).
They do share the same PCI and memory bandwidth.

So, using the BlueField device (starting from BlueField-2), the compress
class operations can be supported in parallel to the net, vDPA and
RegEx class operations.

See :doc:`../../platform/mlx5` guide for more design details.

Features
--------

Compress mlx5 PMD has support for:

Compression/Decompression algorithm:

* DEFLATE.

NULL algorithm for DMA operations.

Huffman code type:

* FIXED.
* DYNAMIC.

Window size support:

1KB, 2KB, 4KB, 8KB, 16KB and 32KB.

Shareable transformation.

Checksum generation:

* CRC32, Adler32 and combined checksum.

Limitations
-----------

* Scatter-Gather, SHA and Stateful are not supported.
* Non-compressed block is not supported in compress (supported in decompress).

Driver options
--------------

Please refer to :ref:`mlx5 common options <mlx5_common_driver_options>`
for an additional list of options shared with other mlx5 drivers.

- ``log-block-size`` parameter [int]

  Log of the Huffman block size in the Deflate algorithm.
  Values from [4-15]; value x means block size is 2^x.
  The default value is 15.


Supported NICs
--------------

* Mellanox\ |reg| BlueField-2 SmartNIC

Prerequisites
-------------

- Mellanox OFED version: **5.2**
  See :ref:`mlx5 common prerequisites <mlx5_linux_prerequisites>` for more details.
