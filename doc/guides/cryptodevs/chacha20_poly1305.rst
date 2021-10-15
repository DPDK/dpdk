..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 Intel Corporation.

Chacha20-poly1305 Crypto Poll Mode Driver
=========================================

The Chacha20-poly1305 PMD provides poll mode crypto driver support for
utilizing `Intel IPSec Multi-buffer library <https://github.com/01org/intel-ipsec-mb>`_.

Features
--------

Chacha20-poly1305 PMD has support for:

AEAD algorithms:

* RTE_CRYPTO_AEAD_CHACHA20_POLY1305


Installation
------------

To build DPDK with the Chacha20-poly1305 PMD the user is required to download
the multi-buffer library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v1.0, which
can be downloaded from `<https://github.com/01org/intel-ipsec-mb/archive/v1.0.zip>`_.

After downloading the library, the user needs to unpack and compile it
on their system before building DPDK:

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

.. _table_chacha20_poly1305_versions:

.. table:: DPDK and external crypto library version compatibility

   =============  ================================
   DPDK version   Crypto library version
   =============  ================================
   21.11+         Multi-buffer library 1.0*
   =============  ================================

\* Multi-buffer library 1.0 or newer only works for Meson but not Make build system.

Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_chacha20_poly1305") within the application.

* Use --vdev="crypto_chacha20_poly1305" in the EAL options, which will call
  rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    --vdev="crypto_chacha20_poly1305,socket_id=0,max_nb_sessions=128"
