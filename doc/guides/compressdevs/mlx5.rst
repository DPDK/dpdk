.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 Mellanox Technologies, Ltd

.. include:: <isonum.txt>

MLX5 compress driver
====================

The MLX5 compress driver library
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

For security reasons and robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel,
combined with hardware specifications that allow to handle virtual memory
addresses directly, ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

The PMD uses libibverbs and libmlx5 to access the device firmware
or directly the hardware components.
There are different levels of objects and bypassing abilities
to get the best performances:

- Verbs is a complete high-level generic API.
- Direct Verbs is a device-specific API.
- DevX allows to access firmware objects.

Enabling librte_compress_mlx5 causes DPDK applications to be linked against
libibverbs.

Mellanox mlx5 PCI device can be probed by number of different PCI devices,
for example net / vDPA / RegEx. To select the compress PMD ``class=compress``
should be specified as device parameter. The compress device can be probed and
used with other Mellanox classes, by adding more options in the class.
For example: ``class=net:compress`` will probe both the net PMD and the compress
PMD.

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
  see :doc:`../../nics/mlx5` guide for more Mellanox OFED details.
