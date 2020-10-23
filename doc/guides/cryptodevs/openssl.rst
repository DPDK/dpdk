..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

OpenSSL Crypto Poll Mode Driver
===============================

This code provides the initial implementation of the openssl poll mode
driver. All cryptography operations are using Openssl library crypto API.
Each algorithm uses EVP interface from openssl API - which is recommended
by Openssl maintainers.

For more details about openssl library please visit openssl webpage:
https://www.openssl.org/

Features
--------

OpenSSL PMD has support for:

Supported cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_CBC``
* ``RTE_CRYPTO_CIPHER_AES_CTR``
* ``RTE_CRYPTO_CIPHER_3DES_CTR``
* ``RTE_CRYPTO_CIPHER_DES_DOCSISBPI``

Supported authentication algorithms:

* ``RTE_CRYPTO_AUTH_AES_GMAC``
* ``RTE_CRYPTO_AUTH_MD5``
* ``RTE_CRYPTO_AUTH_SHA1``
* ``RTE_CRYPTO_AUTH_SHA224``
* ``RTE_CRYPTO_AUTH_SHA256``
* ``RTE_CRYPTO_AUTH_SHA384``
* ``RTE_CRYPTO_AUTH_SHA512``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``

Supported AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``
* ``RTE_CRYPTO_AEAD_AES_CCM``

Supported Asymmetric Crypto algorithms:

* ``RTE_CRYPTO_ASYM_XFORM_RSA``
* ``RTE_CRYPTO_ASYM_XFORM_DSA``
* ``RTE_CRYPTO_ASYM_XFORM_DH``
* ``RTE_CRYPTO_ASYM_XFORM_MODINV``
* ``RTE_CRYPTO_ASYM_XFORM_MODEX``


Installation
------------

To compile the OpenSSL PMD the openssl library must be installed. It will
then be picked up by the Meson/Ninja build system.

To ensure that you have the latest security fixes it is recommended that you
use version 1.1.1g or newer.

* 1.1.1g, 2020-Apr-21. https://www.openssl.org/source/

Initialization
--------------

User can use app/test application to check how to use this pmd and to verify
crypto processing.

Test name is cryptodev_openssl_autotest.
For asymmetric crypto operations testing, run cryptodev_openssl_asym_autotest.

To verify real traffic l2fwd-crypto example can be used with this command:

.. code-block:: console

	sudo ./<build_dir>/examples/dpdk-l2fwd-crypto -l 0-1 -n 4 --vdev "crypto_openssl"
	--vdev "crypto_openssl"-- -p 0x3 --chain CIPHER_HASH
	--cipher_op ENCRYPT --cipher_algo AES_CBC
	--cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f
	--iv 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:ff
	--auth_op GENERATE --auth_algo SHA1_HMAC
	--auth_key 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
	:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
	:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11

Limitations
-----------

* Maximum number of sessions is 2048.
* Chained mbufs are supported only for source mbuf (destination must be
  contiguous).
* Hash only is not supported for GCM and GMAC.
* Cipher only is not supported for GCM and GMAC.
