..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2019 Intel Corporation.

SNOW 3G Crypto Poll Mode Driver
===============================

The SNOW3G PMD (**librte_crypto_snow3g**) provides poll mode crypto driver support for
utilizing `Intel IPSec Multi-buffer library <https://github.com/01org/intel-ipsec-mb>`_
which implements F8 and F8 functions for SNOW 3G UEA2 cipher and UIA2 hash algorithms.

Features
--------

SNOW 3G PMD has support for:

Cipher algorithm:

* RTE_CRYPTO_CIPHER_SNOW3G_UEA2

Authentication algorithm:

* RTE_CRYPTO_AUTH_SNOW3G_UIA2

.. note::

   The latest v1.3 add ARM64 port of ipsec-mb library support ARM platform.

Limitations
-----------

* Chained mbufs are not supported.
* SNOW 3G (UIA2) supported only if hash offset field is byte-aligned.
* In-place bit-level operations for SNOW 3G (UEA2) are not supported
  (if length and/or offset of data to be ciphered is not byte-aligned).

SNOW3G PMD vs AESNI MB PMD
--------------------------

AESNI MB PMD also supports SNOW3G cipher and authentication algorithms.
It is recommended to use the AESNI MB PMD,
which offers better performance on Intel processors.
Take a look at the PMD documentation (:doc:`aesni_mb`) for more information.

Installation
------------

To build DPDK with the SNOW3G_PMD the user is required to download the multi-buffer
library and compile it on their user system before building DPDK.

For x86 system, the multi-buffer library is available
`here <https://github.com/01org/intel-ipsec-mb>`_.
The latest version of the library supported by this PMD is v1.5, which
can be downloaded from `<https://github.com/01org/intel-ipsec-mb/archive/v1.5.zip>`_.

For Arm system, ARM64 port of the multi-buffer library can be downloaded from
`<https://gitlab.arm.com/arm-reference-solutions/ipsec-mb/-/tree/main/>`_. The
latest version of the library supported by this PMD is tagged as SECLIB-IPSEC-2023.06.20.

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

.. note::

   Compilation of the Multi-Buffer library is broken when GCC < 5.0, if library <= v0.53.
   If a lower GCC version than 5.0, the workaround proposed by the following link
   should be used: `<https://github.com/intel/intel-ipsec-mb/issues/40>`_.

As a reference, the following table shows a mapping between the past DPDK versions
and the external crypto libraries supported by them:

.. _table_snow3g_versions:

.. table:: DPDK and external crypto library version compatibility

   =============  ================================
   DPDK version   Crypto library version
   =============  ================================
   16.04 - 19.11  LibSSO SNOW3G
   20.02 - 21.08  Multi-buffer library 0.53 - 1.3*
   21.11+         Multi-buffer library 1.0  - 1.5*
   =============  ================================

\* Multi-buffer library 1.0 or newer only works for Meson but not Make build system.

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_snow3g") within the application.

* Use --vdev="crypto_snow3g" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./dpdk-l2fwd-crypto -l 1 -n 4 --vdev="crypto_snow3g,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "snow3g-uea2"
