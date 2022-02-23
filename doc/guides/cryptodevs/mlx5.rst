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

- Verbs is a complete high-level generic API (Linux only).
- Direct Verbs is a device-specific API (Linux only).
- DevX allows to access firmware objects.

Enabling ``librte_crypto_mlx5`` causes DPDK applications
to be linked against libibverbs on Linux OS.

In order to move the device to crypto operational mode, credential and KEK
(Key Encrypting Key) should be set as the first step.
The credential will be used by the software in order to perform crypto login, and the KEK is
the AES Key Wrap Algorithm (rfc3394) key that will be used for sensitive data
wrapping.
The credential and the AES-XTS keys should be provided to the hardware, as ciphertext
encrypted by the KEK.

A keytag (64 bits) should be appended to the AES-XTS keys (before wrapping),
and will be validated when the hardware attempts to access it.

When crypto engines are defined to work in wrapped import method, they come out
of the factory in Commissioning mode, and thus, cannot be used for crypto operations
yet. A dedicated tool is used for changing the mode from Commissioning to
Operational, while setting the first import_KEK and credential in plaintext.
The mlxreg dedicated tool should be used as follows:

- Set CRYPTO_OPERATIONAL register to set the device in crypto operational mode.

  The input to this tool is:

  - The first credential in plaintext, 40B.
  - The first import_KEK in plaintext: kek size 0 for 16B or 1 for 32B, kek data.

  Example::

     mlxreg -d /dev/mst/mt4123_pciconf0 --reg_name CRYPTO_OPERATIONAL --get

  The "wrapped_crypto_operational" value will be "0x00000000".
  The command to set the register should be executed only once, and all the
  values mentioned above should be specified in the same command.

  Example::

     mlxreg -d /dev/mst/mt4123_pciconf0 --reg_name CRYPTO_OPERATIONAL \
     --set "credential[0]=0x10000000, credential[1]=0x10000000, kek[0]=0x00000000"

  All values not specified will remain 0.
  "wrapped_crypto_going_to_commissioning" and  "wrapped_crypto_operational"
  should not be specified.

  All the device ports should set it in order to move to operational mode.
  For BlueField-2, the internal ports in the ARM system should also be set.

- Query CRYPTO_OPERATIONAL register to make sure the device is in Operational
  mode.

  Example::

     mlxreg -d /dev/mst/mt4123_pciconf0 --reg_name CRYPTO_OPERATIONAL --get

  The "wrapped_crypto_operational" value will be "0x00000001" if the mode was
  successfully changed to operational mode.

  The mlx5 crypto PMD can be verified by running the test application::

     dpdk-test -c 1 -n 1 -w <dev>,class=crypto,wcs_file=<file_path>
     RTE>>cryptodev_mlx5_autotest


Driver options
--------------

- ``class`` parameter [string]

  Select the class of the driver that should probe the device.
  `crypto` for the mlx5 crypto driver.

- ``wcs_file`` parameter [string] - mandatory

  File path including only the wrapped credential in string format of hexadecimal
  numbers, represent 48 bytes (8 bytes IV added by the AES key wrap algorithm).

- ``import_kek_id`` parameter [int]

  The identifier of the KEK, default value is 0 represents the operational
  register import_kek..

- ``credential_id`` parameter [int]

  The identifier of the credential, default value is 0 represents the operational
  register credential.

- ``keytag`` parameter [int]

  The plaintext of the keytag appended to the AES-XTS keys, default value is 0.

- ``max_segs_num`` parameter [int]

  Maximum number of mbuf chain segments(src or dest), default value is 8.


Supported NICs
--------------

* Mellanox\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)
* Mellanox\ |reg| BlueField-2 SmartNIC
* Mellanox\ |reg| ConnectX\ |reg|-6 Dx


Limitations
-----------

- AES-XTS keys provided in xform must include keytag and should be wrapped.
- The supported data-unit lengths are 512B and 4KB and 1MB. In case the `dataunit_len`
  is not provided in the cipher xform, the OP length is limited to the above
  values.


Prerequisites
-------------

FW Prerequisites
~~~~~~~~~~~~~~~~

- xx.31.0328 for ConnectX-6.
- xx.32.0108 for ConnectX-6 Dx and BlueField-2.

Linux Prerequisites
~~~~~~~~~~~~~~~~~~~

- Mellanox OFED version: **5.3**.
  see :doc:`../../nics/mlx5` guide for more Mellanox OFED details.

- Compilation can be done also with rdma-core v15+.
  see :doc:`../../nics/mlx5` guide for more rdma-core details.

Windows Prerequisites
~~~~~~~~~~~~~~~~~~~~~

- Mellanox WINOF-2 version: **2.60** or higher.
  see :doc:`../../nics/mlx5` guide for more Mellanox WINOF-2 details.
