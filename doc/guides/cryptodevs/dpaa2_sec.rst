..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 NXP



NXP DPAA2 CAAM (DPAA2_SEC)
==========================

The DPAA2_SEC PMD provides poll mode crypto driver support for NXP DPAA2 CAAM
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

DPAA2_SEC is one of the hardware resource in DPAA2 Architecture. More information
on DPAA2 Architecture is described in :ref:`dpaa2_overview`.

DPAA2_SEC PMD is one of DPAA2 drivers which interacts with Management Complex (MC)
portal to access the hardware object - DPSECI. The MC provides access to create,
discover, connect, configure and destroy dpseci objects in DPAA2_SEC PMD.

DPAA2_SEC PMD also uses some of the other hardware resources like buffer pools,
queues, queue portals to store and to enqueue/dequeue data to the hardware SEC.

DPSECI objects are detected by PMD using a resource container called DPRC (like
in :ref:`dpaa2_overview`).

For example:

.. code-block:: console

    DPRC.1 (bus)
      |
      +--+--------+-------+-------+-------+---------+
         |        |       |       |       |         |
       DPMCP.1  DPIO.1  DPBP.1  DPNI.1  DPMAC.1  DPSECI.1
       DPMCP.2  DPIO.2          DPNI.2  DPMAC.2  DPSECI.2
       DPMCP.3

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

A block diagram similar to dpaa2 NIC is shown below to show where DPAA2_SEC
fits in the DPAA2 Bus model

.. code-block:: console


                                       +----------------+
                                       | DPDK DPAA2_SEC |
                                       |     PMD        |
                                       +----------------+       +------------+
                                       |  MC SEC object |.......|  Mempool   |
                    . . . . . . . . .  |   (DPSECI)     |       |  (DPBP)    |
                   .                   +---+---+--------+       +-----+------+
                  .                        ^   |                      .
                 .                         |   |<enqueue,             .
                .                          |   | dequeue>             .
               .                           |   |                      .
              .                        +---+---V----+                 .
             .      . . . . . . . . . .| DPIO driver|                 .
            .      .                   |  (DPIO)    |                 .
           .      .                    +-----+------+                 .
          .      .                     |  QBMAN     |                 .
         .      .                      |  Driver    |                 .
    +----+------+-------+              +-----+----- |                 .
    |   dpaa2 bus       |                    |                        .
    |   VFIO fslmc-bus  |....................|.........................
    |                   |                    |
    |     /bus/fslmc    |                    |
    +-------------------+                    |
                                             |
    ========================== HARDWARE =====|=======================
                                           DPIO
                                             |
                                           DPSECI---DPBP
    =========================================|========================



Features
--------

The DPAA2_SEC PMD has support for:

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

Supported DPAA2 SoCs
--------------------

* LS2080A/LS2040A
* LS2084A/LS2044A
* LS2088A/LS2048A
* LS1088A/LS1048A

Whitelisting & Blacklisting
---------------------------

For blacklisting a DPAA2 SEC device, following commands can be used.

 .. code-block:: console

    <dpdk app> <EAL args> -b "fslmc:dpseci.x" -- ...

Where x is the device object id as configured in resource container.

Limitations
-----------

* Chained mbufs are not supported.
* Hash followed by Cipher mode is not supported
* Only supports the session-oriented API implementation (session-less APIs are not supported).

Prerequisites
-------------

DPAA2_SEC driver has similar pre-requisites as described in :ref:`dpaa2_overview`.
The following dependencies are not part of DPDK and must be installed separately:

* **NXP Linux SDK**

  NXP Linux software development kit (SDK) includes support for the family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  SDK and related information can be obtained from:  `NXP QorIQ SDK  <http://www.nxp.com/products/software-and-tools/run-time-software/linux-sdk/linux-sdk-for-qoriq-processors:SDKLINUX>`_.

* **DPDK Extra Scripts**

  DPAA2 based resources can be configured easily with the help of ready scripts
  as provided in the DPDK helper repository.

  `DPDK Extra Scripts <https://github.com/qoriq-open-source/dpdk-extras>`_.

Currently supported by DPDK:

* NXP SDK **17.08+**.
* MC Firmware version **10.3.1** and higher.
* Supported architectures:  **arm64 LE**.

* Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

Basic DPAA2 config file options are described in :ref:`dpaa2_overview`.
In addition to those, the following options can be modified in the ``config`` file
to enable DPAA2_SEC PMD.

Please note that enabling debugging options may affect system performance.

* ``CONFIG_RTE_LIBRTE_PMD_DPAA2_SEC`` (default ``n``)
  By default it is only enabled in defconfig_arm64-dpaa2-* config.
  Toggle compilation of the ``librte_pmd_dpaa2_sec`` driver.

* ``CONFIG_RTE_DPAA2_SEC_PMD_MAX_NB_SESSIONS``
  By default it is set as 2048 in defconfig_arm64-dpaa2-* config.
  It indicates Number of sessions to create in the session memory pool
  on a single DPAA2 SEC device.

Installations
-------------
To compile the DPAA2_SEC PMD for Linux arm64 gcc target, run the
following ``make`` command:

.. code-block:: console

   cd <DPDK-source-directory>
   make config T=arm64-dpaa2-linuxapp-gcc install

Enabling logs
-------------

For enabling logs, use the following EAL parameter:

.. code-block:: console

   ./your_crypto_application <EAL args> --log-level=pmd.crypto.dpaa2:<level>

Using ``crypto.dpaa2`` as log matching criteria, all Crypto PMD logs can be
enabled which are lower than logging ``level``.
