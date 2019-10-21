..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 NXP


NXP CAAM JOB RING (caam_jr)
===========================

The caam_jr PMD provides poll mode crypto driver support for NXP SEC 4.x+ (CAAM)
hardware accelerator. More information is available at:

`NXP Cryptographic Acceleration Technology  <https://www.nxp.com/applications/solutions/internet-of-things/secure-things/network-security-technology/cryptographic-acceleration-technology:NETWORK_SECURITY_CRYPTOG>`_.

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

SEC HW accelerator above 4.x+ version are also known as CAAM.

caam_jr PMD is one of DPAA drivers which uses uio interface to interact with
Linux kernel for configure and destroy the device instance (ring).


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

Note that one physical Job Ring represent one caam_jr device.

Features
--------

The CAAM_JR PMD has support for:

Cipher algorithms:

* ``RTE_CRYPTO_CIPHER_3DES_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CBC``
* ``RTE_CRYPTO_CIPHER_AES192_CBC``
* ``RTE_CRYPTO_CIPHER_AES256_CBC``
* ``RTE_CRYPTO_CIPHER_AES128_CTR``
* ``RTE_CRYPTO_CIPHER_AES192_CTR``
* ``RTE_CRYPTO_CIPHER_AES256_CTR``

Hash algorithms:

* ``RTE_CRYPTO_AUTH_SHA1_HMAC``
* ``RTE_CRYPTO_AUTH_SHA224_HMAC``
* ``RTE_CRYPTO_AUTH_SHA256_HMAC``
* ``RTE_CRYPTO_AUTH_SHA384_HMAC``
* ``RTE_CRYPTO_AUTH_SHA512_HMAC``
* ``RTE_CRYPTO_AUTH_MD5_HMAC``

AEAD algorithms:

* ``RTE_CRYPTO_AEAD_AES_GCM``

Supported DPAA SoCs
--------------------

* LS1046A/LS1026A
* LS1043A/LS1023A
* LS1028A
* LS1012A

Limitations
-----------

* Hash followed by Cipher mode is not supported
* Only supports the session-oriented API implementation (session-less APIs are not supported).

Prerequisites
-------------

caam_jr driver has following dependencies are not part of DPDK and must be installed separately:

* **NXP Linux SDK**

  NXP Linux software development kit (SDK) includes support for the family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  SDK and related information can be obtained from:  `NXP QorIQ SDK  <http://www.nxp.com/products/software-and-tools/run-time-software/linux-sdk/linux-sdk-for-qoriq-processors:SDKLINUX>`_.

Currently supported by DPDK:

* NXP SDK **18.09+**.
* Supported architectures:  **arm64 LE**.

* Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file
to enable caam_jr PMD.

Please note that enabling debugging options may affect system performance.

* ``CONFIG_RTE_LIBRTE_PMD_CAAM_JR`` (default ``n``)
  By default it is only enabled in common_linux config.
  Toggle compilation of the ``librte_pmd_caam_jr`` driver.

* ``CONFIG_RTE_LIBRTE_PMD_CAAM_JR_BE`` (default ``n``)
  By default it is disabled.
  It can be used when the underlying hardware supports the CAAM in BE mode.
  LS1043A, LS1046A and LS1012A support CAAM in BE mode.
  LS1028A supports CAAM in LE mode.

Installations
-------------
To compile the caam_jr PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-armv8a-linux-gcc install

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_crypto_application <EAL args> --log-level=pmd.crypto.caam,<level>
