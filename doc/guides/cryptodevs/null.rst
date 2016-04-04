..  BSD LICENSE
    Copyright(c) 2016 Intel Corporation. All rights reserved.

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

Null Crypto Poll Mode Driver
============================

The Null Crypto PMD (**librte_pmd_null_crypto**) provides a crypto poll mode
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

* Call rte_eal_vdev_init("cryptodev_null_pmd") within the application.

* Use --vdev="cryptodev_null_pmd" in the EAL options, which will call rte_eal_vdev_init() internally.

The following parameters (all optional) can be provided in the previous two calls:

* socket_id: Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).

* max_nb_queue_pairs: Specify the maximum number of queue pairs in the device (8 by default).

* max_nb_sessions: Specify the maximum number of sessions that can be created (2048 by default).

Example:

.. code-block:: console

    ./l2fwd-crypto -c 40 -n 4 --vdev="cryptodev_null_pmd,socket_id=1,max_nb_sessions=128"
