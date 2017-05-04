..  BSD LICENSE
    Copyright(c) 2015 Intel Corporation. All rights reserved.

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

AESN-NI Multi Buffer Crypto Poll Mode Driver
============================================


The AESNI MB PMD (**librte_pmd_aesni_mb**) provides poll mode crypto driver
support for utilizing Intel multi buffer library, see the white paper
`Fast Multi-buffer IPsec Implementations on Intel® Architecture Processors
<https://www-ssl.intel.com/content/www/us/en/intelligent-systems/intel-technology/fast-multi-buffer-ipsec-implementations-ia-processors-paper.html?wapkw=multi+buffer>`_.

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

Hash algorithms:

* RTE_CRYPTO_HASH_MD5_HMAC
* RTE_CRYPTO_HASH_SHA1_HMAC
* RTE_CRYPTO_HASH_SHA224_HMAC
* RTE_CRYPTO_HASH_SHA256_HMAC
* RTE_CRYPTO_HASH_SHA384_HMAC
* RTE_CRYPTO_HASH_SHA512_HMAC
* RTE_CRYPTO_HASH_AES_XCBC_HMAC

Limitations
-----------

* Chained mbufs are not supported.
* Only in-place is currently supported (destination address is the same as source address).
* Only supports session-oriented API implementation (session-less APIs are not supported).

Installation
------------

To build DPDK with the AESNI_MB_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v0.45, which
can be downloaded in `<https://github.com/01org/intel-ipsec-mb/archive/v0.45.zip>`_.

.. code-block:: console

	make

As a reference, the following table shows a mapping between the past DPDK versions
and the Multi-Buffer library version supported by them:

.. _table_aesni_mb_versions:

.. table:: DPDK and Multi-Buffer library version compatibility

   =============  ============================
   DPDK version   Multi-buffer library version
   =============  ============================
   2.2 - 16.11    0.43 - 0.44
   17.02          0.44
   17.05          0.45
   =============  ============================


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Export the environmental variable AESNI_MULTI_BUFFER_LIB_PATH with the path where
  the library was extracted.

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

    ./l2fwd-crypto -l 6 -n 4 --vdev="crypto_aesni_mb,socket_id=1,max_nb_sessions=128"
