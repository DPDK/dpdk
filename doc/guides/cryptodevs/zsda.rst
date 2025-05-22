.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2025 ZTE Corporation.

ZTE Storage Data Accelerator (ZSDA) Poll Mode Driver
====================================================

The ZSDA crypto PMD provides poll mode Cipher and Hash driver
support for the following hardware accelerator devices:

* ZTE Processing accelerators 1cf2


Features
--------

The ZSDA SYM PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_AES_XTS``
* ``RTE_CRYPTO_CIPHER_SM4_XTS``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1``
* ``RTE_CRYPTO_AUTH_SHA224``
* ``RTE_CRYPTO_AUTH_SHA256``
* ``RTE_CRYPTO_AUTH_SHA384``
* ``RTE_CRYPTO_AUTH_SHA512``
* ``RTE_CRYPTO_AUTH_SM3``


Limitations
-----------

* Only supports the session-oriented API implementation
  (session-less API is not supported).
* No BSD and Windows support.
* Queue-pairs are thread-safe on Intel CPUs but queues are not:
  within a single queue-pair all enqueues to the Tx queue must be done from one thread
  and all dequeues from the Rx queue must be done from one thread,
  but enqueues and dequeues may be done in different threads.


Installation
------------

The ZSDA crypto service is built by default with a standard DPDK build.


Testing
-------
