..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2020 Intel Corporation.

AES-NI GCM Crypto Poll Mode Driver
==================================


The AES-NI GCM PMD (**librte_crypto_aesni_gcm**) provides poll mode crypto driver
support for utilizing Intel multi buffer library (see AES-NI Multi-buffer PMD documentation
to learn more about it, including installation).

The AES-NI GCM PMD supports synchronous mode of operation with
``rte_cryptodev_sym_cpu_crypto_process`` function call for both AES-GCM and
GMAC, however GMAC support is limited to one segment per operation. Please
refer to ``rte_crypto`` programmer's guide for more detail.

Features
--------

AESNI GCM PMD has support for:

Authentication algorithms:

* RTE_CRYPTO_AUTH_AES_GMAC

AEAD algorithms:

* RTE_CRYPTO_AEAD_AES_GCM

Limitations
-----------

* In out-of-place operations, chained destination mbufs are not supported.
* Cipher only is not supported.


Installation
------------

To build DPDK with the AESNI_GCM_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v1.5, which
can be downloaded in `<https://github.com/01org/intel-ipsec-mb/archive/v1.5.zip>`_.

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
and the external crypto libraries supported by them:

.. _table_aesni_gcm_versions:

.. table:: DPDK and external crypto library version compatibility

   =============  ================================
   DPDK version   Crypto library version
   =============  ================================
   16.04 - 16.11  Multi-buffer library 0.43 - 0.44
   17.02 - 17.05  ISA-L Crypto v2.18
   17.08 - 18.02  Multi-buffer library 0.46 - 0.48
   18.05 - 19.02  Multi-buffer library 0.49 - 0.52
   19.05 - 20.08  Multi-buffer library 0.52 - 0.55
   20.11 - 21.08  Multi-buffer library 0.53 - 1.3*
   21.11+         Multi-buffer library 1.0  - 1.5*
   =============  ================================

\* Multi-buffer library 1.0 or newer only works for Meson but not Make build system.

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_aesni_gcm") within the application.

* Use --vdev="crypto_aesni_gcm" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./dpdk-l2fwd-crypto -l 1 -n 4 --vdev="crypto_aesni_gcm,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain AEAD --aead_algo "aes-gcm"
