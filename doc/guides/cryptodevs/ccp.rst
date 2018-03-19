.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.

AMD CCP Poll Mode Driver
========================

This code provides the initial implementation of the ccp poll mode driver.
The CCP poll mode driver library (librte_pmd_ccp) implements support for
AMD’s cryptographic co-processor (CCP). The CCP PMD is a virtual crypto
poll mode driver which schedules crypto operations to one or more available
CCP hardware engines on the platform. The CCP PMD provides poll mode crypto
driver support for the following hardware accelerator devices::

	AMD Cryptographic Co-processor (0x1456)
	AMD Cryptographic Co-processor (0x1468)

Features
--------

CCP crypto PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_AES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_ECB``
* ``RTE_CRYPTO_CIPHER_AES_CTR``
* ``RTE_CRYPTO_CIPHER_3DES_CBC``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1``
* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_AES_CMAC``
* ``RTE_CRYPTO_AUTH_SHA3_224``
* ``RTE_CRYPTO_AUTH_SHA3_224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA3_256``
* ``RTE_CRYPTO_AUTH_SHA3_256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA3_384``
* ``RTE_CRYPTO_AUTH_SHA3_384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA3_512``
* ``RTE_CRYPTO_AUTH_SHA3_512_HMAC``

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``

Installation
------------

To compile CCP PMD, it has to be enabled in the config/common_base file.
* ``CONFIG_RTE_LIBRTE_PMD_CCP=y``

The CCP PMD also supports computing authentication over CPU with cipher offloaded
to CCP. To enable this feature, enable following in the configuration.
* ``CONFIG_RTE_LIBRTE_PMD_CCP_CPU_AUTH=y``

This code was verified on Ubuntu 16.04.

Initialization
--------------

Bind the CCP devices to DPDK UIO driver module before running the CCP PMD stack.
e.g. for the 0x1456 device::

	cd to the top-level DPDK directory
	modprobe uio
	insmod ./build/kmod/igb_uio.ko
	echo "1022 1456" > /sys/bus/pci/drivers/igb_uio/new_id

Another way to bind the CCP devices to DPDK UIO driver is by using the ``dpdk-devbind.py`` script.
The following command assumes ``BFD`` of ``0000:09:00.2``::

	cd to the top-level DPDK directory
	./usertools/dpdk-devbind.py -b igb_uio 0000:09:00.2

To verify real traffic l2fwd-crypto example can be used with following command:

.. code-block:: console

	sudo ./build/l2fwd-crypto -l 1 -n 4 --vdev "crypto_ccp" -- -p 0x1
	--chain CIPHER_HASH --cipher_op ENCRYPT --cipher_algo AES_CBC
	--cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f
	--iv 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:ff
	--auth_op GENERATE --auth_algo SHA1_HMAC
	--auth_key 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
	:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
	:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11

Limitations
-----------

* Chained mbufs are not supported
* MD5_HMAC is supported only if ``CONFIG_RTE_LIBRTE_PMD_CCP_CPU_AUTH=y`` is enabled in configuration
