..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2018 Intel Corporation.

AESN-NI Multi Buffer Crypto Poll Mode Driver
============================================


The AESNI MB PMD (**librte_crypto_aesni_mb**) provides poll mode crypto driver
support for utilizing Intel multi buffer library, see the white paper
`Fast Multi-buffer IPsec Implementations on Intel® Architecture Processors
<https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-multi-buffer-ipsec-implementations-ia-processors-paper.pdf>`_.

The AES-NI MB PMD has current only been tested on Fedora 21 64-bit with gcc.

The AES-NI MB PMD supports synchronous mode of operation with
``rte_cryptodev_sym_cpu_crypto_process`` function call.

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
* RTE_CRYPTO_CIPHER_AES128_ECB
* RTE_CRYPTO_CIPHER_AES192_ECB
* RTE_CRYPTO_CIPHER_AES256_ECB
* RTE_CRYPTO_CIPHER_ZUC_EEA3
* RTE_CRYPTO_CIPHER_SNOW3G_UEA2
* RTE_CRYPTO_CIPHER_KASUMI_F8

Hash algorithms:

* RTE_CRYPTO_AUTH_MD5_HMAC
* RTE_CRYPTO_AUTH_SHA1_HMAC
* RTE_CRYPTO_AUTH_SHA224_HMAC
* RTE_CRYPTO_AUTH_SHA256_HMAC
* RTE_CRYPTO_AUTH_SHA384_HMAC
* RTE_CRYPTO_AUTH_SHA512_HMAC
* RTE_CRYPTO_AUTH_AES_XCBC_HMAC
* RTE_CRYPTO_AUTH_AES_CMAC
* RTE_CRYPTO_AUTH_AES_GMAC
* RTE_CRYPTO_AUTH_SHA1
* RTE_CRYPTO_AUTH_SHA224
* RTE_CRYPTO_AUTH_SHA256
* RTE_CRYPTO_AUTH_SHA384
* RTE_CRYPTO_AUTH_SHA512
* RTE_CRYPTO_AUTH_ZUC_EIA3
* RTE_CRYPTO_AUTH_SNOW3G_UIA2
* RTE_CRYPTO_AUTH_KASUMI_F9

AEAD algorithms:

* RTE_CRYPTO_AEAD_AES_CCM
* RTE_CRYPTO_AEAD_AES_GCM
* RTE_CRYPTO_AEAD_CHACHA20_POLY1305

Protocol offloads:

* RTE_SECURITY_PROTOCOL_DOCSIS

Limitations
-----------

* Chained mbufs are not supported.
* Out-of-place is not supported for combined Crypto-CRC DOCSIS security
  protocol.
* RTE_CRYPTO_CIPHER_DES_DOCSISBPI is not supported for combined Crypto-CRC
  DOCSIS security protocol.


Installation
------------

To build DPDK with the AESNI_MB_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v0.55, which
can be downloaded from `<https://github.com/01org/intel-ipsec-mb/archive/v0.55.zip>`_.

.. code-block:: console

    make
    make install

The library requires NASM to be built. Depending on the library version, it might
require a minimum NASM version (e.g. v0.54 requires at least NASM 2.14).

NASM is packaged for different OS. However, on some OS the version is too old,
so a manual installation is required. In that case, NASM can be downloaded from
`NASM website <https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D>`_.
Once it is downloaded, extract it and follow these steps:

.. code-block:: console

    ./configure
    make
    make install

.. note::

   Compilation of the Multi-Buffer library is broken when GCC < 5.0, if library <= v0.53.
   If a lower GCC version than 5.0, the workaround proposed by the following link
   should be used: `<https://github.com/intel/intel-ipsec-mb/issues/40>`_.

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
   19.05 - 19.08   0.52
   19.11+          0.52 - 0.55
   ==============  ============================


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

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

    ./dpdk-l2fwd-crypto -l 1 -n 4 --vdev="crypto_aesni_mb,socket_id=0,max_nb_sessions=128" \
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
