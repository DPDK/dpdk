..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2018 Intel Corporation.

AESN-NI Multi Buffer Crypto Poll Mode Driver
============================================


The AESNI MB PMD (**librte_pmd_aesni_mb**) provides poll mode crypto driver
support for utilizing Intel multi buffer library, see the white paper
`Fast Multi-buffer IPsec Implementations on Intel® Architecture Processors
<https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-multi-buffer-ipsec-implementations-ia-processors-paper.pdf>`_.

The AES-NI MB PMD has current only been tested on Fedora 21 64-bit with gcc.

Features
--------

AESNI MB PMD has support for:

Cipher algorithms:

* RTE_CRYPTO_CIPHER_AES128_CBC
* RTE_CRYPTO_CIPHER_AES192_CBC
* RTE_CRYPTO_CIPHER_AES256_CBC
* RTE_CRYPTO_CIPHER_AES128_CTR
* RTE_CRYPTO_CIPHER_AES192_CTR
* RTE_CRYPTO_CIPHER_AES256_CTR
* RTE_CRYPTO_CIPHER_AES_DOCSISBPI
* RTE_CRYPTO_CIPHER_DES_CBC
* RTE_CRYPTO_CIPHER_3DES_CBC
* RTE_CRYPTO_CIPHER_DES_DOCSISBPI

Hash algorithms:

* RTE_CRYPTO_HASH_MD5_HMAC
* RTE_CRYPTO_HASH_SHA1_HMAC
* RTE_CRYPTO_HASH_SHA224_HMAC
* RTE_CRYPTO_HASH_SHA256_HMAC
* RTE_CRYPTO_HASH_SHA384_HMAC
* RTE_CRYPTO_HASH_SHA512_HMAC
* RTE_CRYPTO_HASH_AES_XCBC_HMAC
* RTE_CRYPTO_HASH_AES_CMAC
* RTE_CRYPTO_HASH_AES_GMAC
* RTE_CRYPTO_HASH_SHA1
* RTE_CRYPTO_HASH_SHA224
* RTE_CRYPTO_HASH_SHA256
* RTE_CRYPTO_HASH_SHA384
* RTE_CRYPTO_HASH_SHA512

AEAD algorithms:

* RTE_CRYPTO_AEAD_AES_CCM
* RTE_CRYPTO_AEAD_AES_GCM

Limitations
-----------

* Chained mbufs are not supported.


Installation
------------

To build DPDK with the AESNI_MB_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v0.52, which
can be downloaded from `<https://github.com/01org/intel-ipsec-mb/archive/v0.52.zip>`.

.. code-block:: console

    make
    make install

As a reference, the following table shows a mapping between the past DPDK versions
and the Multi-Buffer library version supported by them:

.. _table_aesni_mb_versions:

.. table:: DPDK and Multi-Buffer library version compatibility

   ==============  ============================
   DPDK version    Multi-buffer library version
   ==============  ============================
   2.2 - 16.11     0.43 - 0.44
   17.02           0.44
   17.05 - 17.08   0.45 - 0.48
   17.11           0.47 - 0.48
   18.02           0.48
   18.05 - 19.02   0.49 - 0.52
   19.05+          0.52+
   ==============  ============================


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

* Set CONFIG_RTE_LIBRTE_PMD_AESNI_MB=y in config/common_base.

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_aesni_mb") within the application.

* Use --vdev="crypto_aesni_mb" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -l 1 -n 4 --vdev="crypto_aesni_mb,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_HASH --cipher_algo "aes-cbc" --auth_algo "sha1-hmac"

Extra notes
-----------

For AES Counter mode (AES-CTR), the library supports two different sizes for Initialization
Vector (IV):

* 12 bytes: used mainly for IPsec, as it requires 12 bytes from the user, which internally
  are appended the counter block (4 bytes), which is set to 1 for the first block
  (no padding required from the user)

* 16 bytes: when passing 16 bytes, the library will take them and use the last 4 bytes
  as the initial counter block for the first block.
