..  BSD LICENSE
    Copyright 2017 NXP.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of NXP nor the names of its
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

Limitations
-----------

* Chained mbufs are not supported.
* Hash followed by Cipher mode is not supported
* Only supports the session-oriented API implementation (session-less APIs are not supported).

Prerequisites
-------------

DPAA_SEC driver has similar pre-requisites as described in :ref:`dpaa_overview`.
The following dependencies are not part of DPDK and must be installed separately:

* **NXP Linux SDK**

  NXP Linux software development kit (SDK) includes support for the family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  SDK and related information can be obtained from:  `NXP QorIQ SDK  <http://www.nxp.com/products/software-and-tools/run-time-software/linux-sdk/linux-sdk-for-qoriq-processors:SDKLINUX>`_.

* **DPDK Extras Scripts**

  DPAA based resources can be configured easily with the help of ready scripts
  as provided in the DPDK Extras repository.

  `DPDK Extras Scripts <https://github.com/qoriq-open-source/dpdk-extras>`_.

Currently supported by DPDK:

* NXP SDK **2.0+**.
* Supported architectures:  **arm64 LE**.

* Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

Basic DPAA config file options are described in :ref:`dpaa_overview`.
In addition to those, the following options can be modified in the ``config`` file
to enable DPAA_SEC PMD.

Please note that enabling debugging options may affect system performance.

* ``CONFIG_RTE_LIBRTE_PMD_DPAA_SEC`` (default ``n``)
  By default it is only enabled in defconfig_arm64-dpaa-* config.
  Toggle compilation of the ``librte_pmd_dpaa_sec`` driver.

* ``CONFIG_RTE_LIBRTE_DPAA_SEC_DEBUG_INIT`` (default ``n``)
  Toggle display of initialization related driver messages

* ``CONFIG_RTE_LIBRTE_DPAA_SEC_DEBUG_DRIVER`` (default ``n``)
  Toggle display of driver runtime messages

* ``CONFIG_RTE_LIBRTE_DPAA_SEC_DEBUG_RX`` (default ``n``)
  Toggle display of receive fast path run-time message

* ``CONFIG_RTE_DPAA_SEC_PMD_MAX_NB_SESSIONS``
  By default it is set as 2048 in defconfig_arm64-dpaa-* config.
  It indicates Number of sessions to create in the session memory pool
  on a single DPAA SEC device.

Installations
-------------
To compile the DPAA_SEC PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa-linuxapp-gcc install
