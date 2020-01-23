..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017 Cavium, Inc

ARMv8 Crypto Poll Mode Driver
=============================

This code provides the initial implementation of the ARMv8 crypto PMD.
The driver uses ARMv8 cryptographic extensions to process chained crypto
operations in an optimized way. The core functionality is provided by
a low-level library, written in the assembly code.

Features
--------

ARMv8 Crypto PMD has support for the following algorithm pairs:

Supported cipher algorithms:

* ``RTE_CRYPTO_CIPHER_AES_CBC``

Supported authentication algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``

Installation
------------

In order to enable this virtual crypto PMD, user must:

* Download AArch64 crypto library source code from
  `here <https://github.com/ARM-software/AArch64cryptolib>`_

* Export the environmental variable ARMV8_CRYPTO_LIB_PATH with
  the path to ``AArch64cryptolib`` library.

* Build the library by invoking:

.. code-block:: console

	make -C $ARMV8_CRYPTO_LIB_PATH/

* Set CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO=y in
  config/defconfig_arm64-armv8a-linux-gcc

The corresponding device can be created only if the following features
are supported by the CPU:

* ``RTE_CPUFLAG_AES``
* ``RTE_CPUFLAG_SHA1``
* ``RTE_CPUFLAG_SHA2``
* ``RTE_CPUFLAG_NEON``

Initialization
--------------

User can use app/test application to check how to use this PMD and to verify
crypto processing.

Test name is cryptodev_sw_armv8_autotest.

Limitations
-----------

* Maximum number of sessions is 2048.
* Only chained operations are supported.
* AES-128-CBC is the only supported cipher variant.
* Cipher input data has to be a multiple of 16 bytes.
* Digest input data has to be a multiple of 8 bytes.
