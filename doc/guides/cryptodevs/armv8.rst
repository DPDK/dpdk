..  BSD LICENSE
    Copyright (C) Cavium networks Ltd. 2017.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in
        the documentation and/or other materials provided with the
        distribution.
      * Neither the name of Cavium networks nor the names of its
        contributors may be used to endorse or promote products derived
        from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


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

* Download ARMv8 crypto library source code from
  `here <https://github.com/caviumnetworks/armv8_crypto>`_

* Export the environmental variable ARMV8_CRYPTO_LIB_PATH with
  the path where the ``armv8_crypto`` library was downloaded
  or cloned.

* Build the library by invoking:

.. code-block:: console

	make -C $ARMV8_CRYPTO_LIB_PATH/

* Set CONFIG_RTE_LIBRTE_PMD_ARMV8_CRYPTO=y in
  config/defconfig_arm64-armv8a-linuxapp-gcc

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
For performance test cryptodev_sw_armv8_perftest can be used.

Limitations
-----------

* Maximum number of sessions is 2048.
* Only chained operations are supported.
* AES-128-CBC is the only supported cipher variant.
* Cipher input data has to be a multiple of 16 bytes.
* Digest input data has to be a multiple of 8 bytes.
