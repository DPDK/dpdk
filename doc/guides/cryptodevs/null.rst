..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Intel Corporation.

Null Crypto Poll Mode Driver
============================

The Null Crypto PMD (**librte_crypto_null**) provides a crypto poll mode
driver which provides a minimal implementation for a software crypto device. As
a null device it does not modify the data in the mbuf on which the crypto
operation is to operate and it only has support for a single cipher and
authentication algorithm.

When a burst of mbufs is submitted to a Null Crypto PMD for processing then
each mbuf in the burst will be enqueued in an internal buffer for collection on
a dequeue call as long as the mbuf has a valid rte_mbuf_offload operation with
a valid rte_cryptodev_session or rte_crypto_xform chain of operations.

Features
--------

Modes:

* RTE_CRYPTO_XFORM_CIPHER ONLY
* RTE_CRYPTO_XFORM_AUTH ONLY
* RTE_CRYPTO_XFORM_CIPHER THEN RTE_CRYPTO_XFORM_AUTH
* RTE_CRYPTO_XFORM_AUTH THEN RTE_CRYPTO_XFORM_CIPHER

Cipher algorithms:

* RTE_CRYPTO_CIPHER_NULL

Authentication algorithms:

* RTE_CRYPTO_AUTH_NULL

Limitations
-----------

* Only in-place is currently supported (destination address is the same as
  source address).

Installation
------------

The Null Crypto PMD is enabled and built by default in both the Linux and
FreeBSD builds.

Initialization
--------------

To use the PMD in an application, user must:

* Call rte_vdev_init("crypto_null") within the application.

* Use --vdev="crypto_null" in the EAL options, which will call rte_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./dpdk-l2fwd-crypto -l 1 -n 4 --vdev="crypto_null,socket_id=0,max_nb_sessions=128" \
    -- -p 1 --cdev SW --chain CIPHER_ONLY --cipher_algo "null"
