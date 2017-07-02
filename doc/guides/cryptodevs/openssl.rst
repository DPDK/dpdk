..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
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


Installation
------------

To compile openssl PMD, it has to be enabled in the config/common_base file
and appropriate openssl packages have to be installed in the build environment.

The newest openssl library version is supported:
* 1.0.2h-fips  3 May 2016.
Older versions that were also verified:
* 1.0.1f 6 Jan 2014
* 1.0.1 14 Mar 2012

For Ubuntu 14.04 LTS these packages have to be installed in the build system:
sudo apt-get install openssl
sudo apt-get install libc6-dev-i386 (for i686-native-linuxapp-gcc target)

This code was also verified on Fedora 24.
This code was NOT yet verified on FreeBSD.

Initialization
--------------

User can use app/test application to check how to use this pmd and to verify
crypto processing.

Test name is cryptodev_openssl_autotest.
For performance test cryptodev_openssl_perftest can be used.

To verify real traffic l2fwd-crypto example can be used with this command:

.. code-block:: console

	sudo ./build/l2fwd-crypto -l 0-1 -n 4 --vdev "crypto_openssl"
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
