.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.

AMD CCP Poll Mode Driver
========================

This code provides the initial implementation of the ccp poll mode driver.
The CCP poll mode driver library (**librte_crypto_ccp**) implements support for
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

To compile ccp PMD, openssl packages have to be installed in the build
environment.

For Ubuntu 16.04 LTS use below to install openssl in the build system:

.. code-block:: console

	sudo apt-get install openssl

This code was verified on Ubuntu 16.04.

Initialization
--------------

Bind the CCP devices to ``vfio_pci`` or ``igb_uio`` (see :ref:`linux_gsg_binding_kernel`)
before running the CCP PMD stack.

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_ccp") within the application.

* Use --vdev="crypto_ccp" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated.
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device.

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

* ccp_auth_opt: Specify authentication operations to perform on CPU using openssl APIs.

To validate ccp PMD, l2fwd-crypto example can be used with following command:

.. code-block:: console

        sudo ./<build_dir>/examples/dpdk-l2fwd-crypto -l 1 -n 4 --vdev "crypto_ccp" -- -p 0x1
        --chain CIPHER_HASH --cipher_op ENCRYPT --cipher_algo aes-cbc
        --cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f
        --cipher_iv 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:ff
        --auth_op GENERATE --auth_algo sha1-hmac
        --auth_key 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
        :11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
        :11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11

The CCP PMD also supports computing authentication over CPU with cipher offloaded to CCP.
To enable this feature, pass an additional argument as ccp_auth_opt=1 to --vdev parameters as
following:

.. code-block:: console

        sudo ./<build_dir>/examples/dpdk-l2fwd-crypto -l 1 -n 4 --vdev "crypto_ccp,ccp_auth_opt=1" -- -p 0x1
        --chain CIPHER_HASH --cipher_op ENCRYPT --cipher_algo aes-cbc
        --cipher_key 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f
        --cipher_iv 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:ff
        --auth_op GENERATE --auth_algo sha1-hmac
        --auth_key 11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
        :11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11
        :11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11:11

Limitations
-----------

* Chained mbufs are not supported.
* MD5_HMAC is supported only for CPU based authentication.
