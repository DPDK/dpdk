..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016-2019 Intel Corporation.

AES-NI GCM Crypto Poll Mode Driver
==================================


The AES-NI GCM PMD (**librte_pmd_aesni_gcm**) provides poll mode crypto driver
support for utilizing Intel multi buffer library (see AES-NI Multi-buffer PMD documentation
to learn more about it, including installation).

Features
--------

AESNI GCM PMD has support for:

Authentication algorithms:

* RTE_CRYPTO_AUTH_AES_GMAC

AEAD algorithms:

* RTE_CRYPTO_AEAD_AES_GCM


Limitations
-----------

* Chained mbufs are supported but only out-of-place (destination mbuf must be contiguous).
* Cipher only is not supported.


Installation
------------

To build DPDK with the AESNI_GCM_PMD the user is required to download the multi-buffer
library from `here <https://github.com/01org/intel-ipsec-mb>`_
and compile it on their user system before building DPDK.
The latest version of the library supported by this PMD is v0.52, which
can be downloaded in `<https://github.com/01org/intel-ipsec-mb/archive/v0.52.zip>`_.

.. code-block:: console

    make
    make install

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
   19.05+         Multi-buffer library 0.52
   =============  ================================


Initialization
--------------

In order to enable this virtual crypto PMD, user must:

* Build the multi buffer library (explained in Installation section).

* Set CONFIG_RTE_LIBRTE_PMD_AESNI_GCM=y in config/common_base.

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

    ./l2fwd-crypto -l 1 -n 4 --vdev="crypto_aesni_gcm,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain AEAD --aead_algo "aes-gcm"
