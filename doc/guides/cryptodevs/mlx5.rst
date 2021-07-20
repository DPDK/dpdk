.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2021 NVIDIA Corporation & Affiliates

.. include:: <isonum.txt>

MLX5 Crypto Driver
==================

The MLX5 crypto driver library
(**librte_crypto_mlx5**) provides support for **Mellanox ConnectX-6**
family adapters.

Overview
--------

The device can provide disk encryption services,
allowing data encryption and decryption towards a disk.
Having all encryption/decryption operations done in a single device
can reduce cost and overheads of the related FIPS certification,
as ConnectX-6 is FIPS 140-2 level-2 ready.
The encryption cipher is AES-XTS of 256/512 bit key size.

MKEY is a memory region object in the hardware,
that holds address translation information and attributes per memory area.
Its ID must be tied to addresses provided to the hardware.
The encryption operations are performed with MKEY read/write transactions,
when the MKEY is configured to perform crypto operations.

The encryption does not require text to be aligned to the AES block size (128b).

For security reasons and to increase robustness, this driver only deals with virtual
memory addresses. The way resources allocations are handled by the kernel,
combined with hardware specifications that allow handling virtual memory
addresses directly, ensure that DPDK applications cannot access random
physical memory (or memory that does not belong to the current process).

The PMD uses ``libibverbs`` and ``libmlx5`` to access the device firmware
or to access the hardware components directly.
There are different levels of objects and bypassing abilities.
To get the best performances:

- Verbs is a complete high-level generic API.
- Direct Verbs is a device-specific API.
- DevX allows to access firmware objects.

Enabling ``librte_crypto_mlx5`` causes DPDK applications
to be linked against libibverbs.


Driver options
--------------

- ``class`` parameter [string]

  Select the class of the driver that should probe the device.
  `crypto` for the mlx5 crypto driver.


Supported NICs
--------------

* Mellanox\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)


Limitations
-----------

- AES-XTS keys provided in xform must include keytag and should be wrapped.
- The supported data-unit lengths are 512B and 1KB. In case the `dataunit_len`
  is not provided in the cipher xform, the OP length is limited to the above
  values and 1MB.


Prerequisites
-------------

- Mellanox OFED version: **5.3**
  see :doc:`../../nics/mlx5` guide for more Mellanox OFED details.

- Compilation can be done also with rdma-core v15+.
  see :doc:`../../nics/mlx5` guide for more rdma-core details.
