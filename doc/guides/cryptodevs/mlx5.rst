.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (c) 2021 NVIDIA Corporation & Affiliates

.. include:: <isonum.txt>

NVIDIA MLX5 Crypto Driver
=========================

.. note::

   NVIDIA acquired Mellanox Technologies in 2020.
   The DPDK documentation and code might still include instances
   of or references to Mellanox trademarks (like BlueField and ConnectX)
   that are now NVIDIA trademarks.

The MLX5 crypto driver library
(**librte_crypto_mlx5**) provides support for **NVIDIA ConnectX-6**,
**NVIDIA ConnectX-6 Dx**, **NVIDIA ConnectX-7**, **NVIDIA BlueField-2**,
and **NVIDIA BlueField-3** family adapters.

Overview
--------

NVIDIA MLX5 crypto driver supports AES-XTS and AES-GCM cryption.

AES-XTS
~~~~~~~

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

See :doc:`../../platform/mlx5` guide for more design details.

AES-GCM
~~~~~~~

The supported AAD/digest/key size can be read from ``dev_info``.

In AES-GCM mode, the HW requires continuous input and output of
Additional Authenticated Data (AAD), payload, and digest (if needed).
However, the API only provides a single AAD input,
which means that in the out-of-place mode,
the AAD will be used in both input and output.
This reuse of AAD in the out-of-place mode breaks the continuous output,
which degrades the performance and introduces extra UMR WQE.
If digest is not continuous after payload will also lead to that extra UMR WQE.

To address this issue, the API provides ``min_mbuf_headroom_req`` and
``min_mbuf_tailroom_req`` in ``rte_cryptodev_info`` as a hint to the PMD.
It indicates the PMD can use the buffer before and after the mbuf payload
as AAD and digest space.
With this hint, the PMD will use the buffer before and after the mbuf payload
directly via copying AAD and digest.
However, the application must ensure that there is enough headroom and tailroom
reserved for the mbuf.
Or, for non-continuous operations, extra UMR WQE will be used.


Configuration
-------------

See the :ref:`mlx5 common configuration <mlx5_common_env>`.

A device comes out of NVIDIA factory with pre-defined import methods.
There are two possible import methods: wrapped or plaintext (valid for AES-XTS only).

In case the device is in wrapped mode, it needs to be moved to crypto operational mode.
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
  For BlueField-2, BlueField-3 the internal ports in the ARM system should also be set.

- Query CRYPTO_OPERATIONAL register to make sure the device is in Operational
  mode.

  Example::

     mlxreg -d /dev/mst/mt4123_pciconf0 --reg_name CRYPTO_OPERATIONAL --get

  The "wrapped_crypto_operational" value will be "0x00000001" if the mode was
  successfully changed to operational mode.

On the other hand, in case of plaintext mode, there is no need for all the above,
DEK is passed in plaintext without keytag.

  The mlx5 crypto PMD can be verified by running the test application::
    Wrapped mode:
      dpdk-test -c 1 -n 1 -w <dev>,class=crypto,wcs_file=<file_path>
      RTE>>cryptodev_mlx5_autotest

    Plaintext mode:
      dpdk-test -c 1 -n 1 -w <dev>,class=crypto
      RTE>>cryptodev_mlx5_autotest


Driver options
--------------

Please refer to :ref:`mlx5 common options <mlx5_common_driver_options>`
for an additional list of options shared with other mlx5 drivers.

- ``algo`` parameter [int]

  - 0. AES-XTS crypto.

  - 1. AES-GCM crypto.

  Set to zero (AES-XTS) by default.

- ``wcs_file`` parameter [string] - mandatory in wrapped mode

  File path including only the wrapped credential in string format of hexadecimal
  numbers, represent 48 bytes (8 bytes IV added by the AES key wrap algorithm).
  This option is valid only for AES-XTS.

- ``import_kek_id`` parameter [int]

  The identifier of the KEK, default value is 0 represents the operational
  register import_kek..
  This option is valid only for AES-XTS.

- ``credential_id`` parameter [int]

  The identifier of the credential, default value is 0 represents the operational
  register credential.
  This option is valid only for AES-XTS.

- ``keytag`` parameter [int]

  The plaintext of the keytag appended to the AES-XTS keys, default value is 0.
  This option is valid only for AES-XTS.

- ``max_segs_num`` parameter [int]

  Maximum number of mbuf chain segments(src or dest), default value is 8.


Supported NICs
--------------

* NVIDIA\ |reg| ConnectX\ |reg|-6 200G MCX654106A-HCAT (2x200G)
* NVIDIA\ |reg| ConnectX\ |reg|-6 Dx
* NVIDIA\ |reg| ConnectX\ |reg|-7
* NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC
* NVIDIA\ |reg| BlueField\ |reg|-3 SmartNIC


Limitations
-----------

- AES-XTS keys provided in xform must include keytag and should be wrapped.
- The supported data-unit lengths are 512B and 4KB and 1MB. In case the `dataunit_len`
  is not provided in the cipher xform, the OP length is limited to the above
  values.
- AES-GCM is supported only on BlueField-3.
- AES-GCM supports only key import plaintext mode.


Prerequisites
-------------

FW Prerequisites
~~~~~~~~~~~~~~~~

- xx.31.0328 for ConnectX-6.
- xx.32.0108 for ConnectX-6 Dx and BlueField-2.
- xx.36.xxxx for ConnectX-7 and BlueField-3.
- xx.37.3010 for BlueField-3 and newer for AES-GCM.

Linux Prerequisites
~~~~~~~~~~~~~~~~~~~

- NVIDIA MLNX_OFED version: **5.3**.
- Compilation can be done also with rdma-core v15+.

  See :ref:`mlx5 common prerequisites <mlx5_linux_prerequisites>` for more details.

Windows Prerequisites
~~~~~~~~~~~~~~~~~~~~~

- NVIDIA WINOF-2 version: **2.60** or higher.
  See :ref:`mlx5 common prerequisites <mlx5_windows_prerequisites>` for more details.
