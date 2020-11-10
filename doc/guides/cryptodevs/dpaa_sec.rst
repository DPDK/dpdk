..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 NXP



NXP DPAA CAAM (DPAA_SEC)
========================

The DPAA_SEC PMD provides poll mode crypto driver support for NXP DPAA CAAM
hardware accelerator.

Architecture
------------

SEC is the SOC's security engine, which serves as NXP's latest cryptographic
acceleration and offloading hardware. It combines functions previously
implemented in separate modules to create a modular and scalable acceleration
and assurance engine. It also implements block encryption algorithms, stream
cipher algorithms, hashing algorithms, public key algorithms, run-time
integrity checking, and a hardware random number generator. SEC performs
higher-level cryptographic operations than previous NXP cryptographic
accelerators. This provides significant improvement to system level performance.

DPAA_SEC is one of the hardware resource in DPAA Architecture. More information
on DPAA Architecture is described in :ref:`dpaa_overview`.

DPAA_SEC PMD is one of DPAA drivers which interacts with QBMAN to create,
configure and destroy the device instance using queue pair with CAAM portal.

DPAA_SEC PMD also uses some of the other hardware resources like buffer pools,
queues, queue portals to store and to enqueue/dequeue data to the hardware SEC.

Implementation
--------------

SEC provides platform assurance by working with SecMon, which is a companion
logic block that tracks the security state of the SOC. SEC is programmed by
means of descriptors (not to be confused with frame descriptors (FDs)) that
indicate the operations to be performed and link to the message and
associated data. SEC incorporates two DMA engines to fetch the descriptors,
read the message data, and write the results of the operations. The DMA
engine provides a scatter/gather capability so that SEC can read and write
data scattered in memory. SEC may be configured by means of software for
dynamic changes in byte ordering. The default configuration for this version
of SEC is little-endian mode.

Features
--------

The DPAA PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_CIPHER_AES256_CTR``
* ``RTE_CRYPTO_CIPHER_SNOW3G_UEA2``
* ``RTE_CRYPTO_CIPHER_ZUC_EEA3``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_SNOW3G_UIA2``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``
* ``RTE_CRYPTO_AUTH_ZUC_EIA3``

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``

Supported DPAA SoCs
--------------------

* LS1046A/LS1026A
* LS1043A/LS1023A

Allowing & Blocking
-------------------

For blocking a DPAA device, following commands can be used.

 .. code-block:: console

    <dpdk app> <EAL args> -b "dpaa:dpaa_sec-X" -- ...
    e.g. "dpaa:dpaa_sec-1"

    or to disable all 4 SEC devices
    -b "dpaa:dpaa_sec-1"  -b "dpaa:dpaa_sec-2" -b "dpaa:dpaa_sec-3" -b "dpaa:dpaa_sec-4"

Limitations
-----------

* Hash followed by Cipher mode is not supported
* Only supports the session-oriented API implementation (session-less APIs are not supported).

Prerequisites
-------------

DPAA_SEC driver has similar pre-requisites as described in :ref:`dpaa_overview`.

See :doc:`../platform/dpaa` for setup information


- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.


Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_crypto_application <EAL args> --log-level=pmd.crypto.dpaa:<level>

Using ``pmd.crypto.dpaa`` as log matching criteria, all Crypto PMD logs can be
enabled which are lower than logging ``level``.
