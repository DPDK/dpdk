..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2019 Intel Corporation.

ZUC Crypto Poll Mode Driver
===========================

The ZUC PMD (**librte_crypto_zuc**) provides poll mode crypto driver support for
utilizing the ARM64 port of `Intel IPsec Multi-Buffer library
<https://gitlab.arm.com/arm-reference-solutions/ipsec-mb>`_
which implements F8 and F9 functions for ZUC EEA3 cipher and EIA3 hash algorithms.

Features
--------

ZUC PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_CIPHER_ZUC_EEA3

Authentication algorithm:

* RTE_CRYPTO_AUTH_ZUC_EIA3

.. note::

   The latest v1.3 add ARM64 port of ipsec-mb library support ARM platform.

Limitations
-----------

* Chained mbufs are not supported.
* ZUC (EIA3) supported only if hash offset field is byte-aligned.
* ZUC (EEA3) supported only if cipher length, cipher offset fields are byte-aligned.

ZUC PMD vs AESNI MB PMD
-----------------------

On Intel processors this PMD is no longer supported, please use the AESNI MB PMD.
Take a look at the PMD documentation (:doc:`aesni_mb`) for more information.

Installation
------------

To build DPDK with the ZUC_PMD the user is required to download the multi-buffer
library and compile it on their user system before building DPDK.

The ARM64 port of the multi-buffer library can be downloaded from
`<https://gitlab.arm.com/arm-reference-solutions/ipsec-mb/-/tree/main/>`_.
The latest version of the library supported by this PMD is tagged as SECLIB-IPSEC-2026.05.30.

After downloading the library, the user needs to unpack and compile it
on their system before building DPDK:

.. code-block:: console

    make
    make install

The library requires NASM to be built on x86. Depending on the library version,
it might require a minimum NASM version (e.g. v0.54 requires at least NASM 2.14).

NASM is packaged for different OS. However, on some OS the version is too old,
so a manual installation is required. In that case, NASM can be downloaded from
`NASM website <https://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D>`_.
Once it is downloaded, extract it and follow these steps:

.. code-block:: console

    ./configure
    make
    make install

As a reference, the following table shows a mapping between the past DPDK versions
and the external crypto libraries supported by them:

.. _table_zuc_versions:

.. table:: DPDK and external crypto library version compatibility

   =============  ================================
   DPDK version   Crypto library version
   =============  ================================
   20.02 - 21.08  Multi-buffer library 0.53 - 1.3
   21.11 - 24.07  Multi-buffer library 1.0  - 1.5
   24.11+         Multi-buffer library 1.4+
   =============  ================================

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_zuc") within the application.

* Use --vdev="crypto_zuc" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./dpdk-l2fwd-crypto -l 1 --vdev="crypto_zuc,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "zuc-eea3"
