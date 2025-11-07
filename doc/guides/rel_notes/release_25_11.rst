.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_25_11.html


New Features
------------

.. This section should contain new features added in this release.
   Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense.
     The description should be enough to allow someone scanning
     the release notes to understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list
     like this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     Suggested order in release notes items:
     * Core libs (EAL, mempool, ring, mbuf, buses)
     * Device abstraction libs and PMDs (ordered alphabetically by vendor name)
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =======================================================

* **Added automatic lcore-id remapping option.**

  Added the EAL option ``--remap-lcore-ids`` or ``-R``
  to enable automatic remapping of lcore-ids to a contiguous set starting from 0,
  or from a user-provided value.
  When this flag is passed, the lcores specified by core mask or core list options
  are taken as the physical cores on which the application will run,
  and one thread will be started per core, with sequential lcore-ids.
  For example: ``dpdk-test -l 140-144 -R``
  will start 5 threads with lcore-ids 0 to 4 on physical cores 140 to 144.

* **Added inter-process and inter-OS DMA device API.**

  * Added parameters in DMA device virtual channel to configure DMA operations
    that span across different processes or operating system domains.
  * Added functions to exchange handlers between DMA devices.

* **Added speed 800G.**

  Added Ethernet link speed for 800 Gb/s as it is well standardized in IEEE,
  and some devices already support this speed.

* **Added mbuf tracking for debug.**

  Added history dynamic field in mbuf (disabled by default)
  to store successive states of the mbuf lifecycle.
  Some functions were added to dump statistics.
  A script was added to parse mbuf tracking stored in a file.

* **Added ethdev API to get link connector.**

  Added API to report type of link connector for a port.
  The following connectors are enumerated:

  * None
  * Twisted Pair
  * Attachment Unit Interface (AUI)
  * Optical Fiber Link
  * BNC
  * Direct Attach Copper
  * XFI, SFI
  * Media Independent Interface (MII)
  * SGMII, QSGMII
  * XLAUI, GAUI, AUI, CAUI, LAUI
  * SFP, SFP+, SFP28, SFP-DD
  * QSFP, QSFP+, QSFP28, QSFP56, QSFP-DD
  * OTHER

  By default, it reports ``RTE_ETH_LINK_CONNECTOR_NONE``
  unless driver specifies it.

* **Updated Amazon ENA (Elastic Network Adapter) ethernet driver.**

  * Added support for retrieving HW timestamps for Rx packets with nanosecond resolution.
  * Fixed PCI BAR mapping on 64K page size.

* **Added Huawei hinic3 ethernet driver.**

  Added network driver for the Huawei SPx series Network Adapters.

* **Updated Intel ice ethernet driver.**

  * Added support for Data Center Bridging (DCB).
  * Added support for Priority Flow Control (PFC).

* **Updated Marvell cnxk ethernet driver.**

  Added support to set/get link configuration as mentioned below:

  * Get speed capability from firmware.

* **Added Nebulamatrix nbl ethernet driver.**

  Added the PMD for Nebulamatrix NICs.

* **Updated NVIDIA mlx5 driver.**

  * Added support for NVIDIA ConnectX-9 SuperNIC adapters.
  * Added support for count and age flow actions on root tables
    with HW steering flow engine.

* **Updated NXP DPAA2 ethernet driver.**

  * Enabled software taildrop for ordered queues.
  * Added additional MAC counters in xstats.

* **Added NXP ENETC4 ethernet driver.**

  Added ENETC4 PMD for multiple new generation SoCs.

* **Updated TAP ethernet driver.**

  * Replaced ioctl-based link control with a Netlink-based implementation.
  * Linux net devices can now be renamed without breaking link control.
  * Linux net devices can now be moved to different namespaces
    without breaking link control (requires Linux >= 5.2).

* **Updated Wangxun txgbe ethernet driver.**

  Added support for Wangxun Amber-Lite NIC series,
  including FF5025 (supporting 10G and 25G) and FF5040 (supporting 40G).
  As these new models share hardware similarities with the existing 10G Sapphire NICs,
  many of the existing configurations and practices are expected to apply.

* **Updated Yunsilicon xsc ethernet driver.**

  * Added FW version query.
  * Added TSO support.
  * Added module EEPROM dump.
  * Added promiscuous mode.
  * Added link status.
  * Added link event.
  * Added FEC get and set.
  * Added multi-process per port.
  * Optimized code.

* **Added PQC ML algorithms in cryptodev.**

  * Added PQC ML-KEM support with reference to FIPS203.
  * Added PQC ML-DSA support with reference to FIPS204.

* **Updated openssl crypto driver.**

  * Added support for PQC ML-KEM and ML-DSA algorithms.

* **Updated Intel QuickAssist Technology (QAT) crypto driver.**

  * Added SM2 encryption and decryption algorithms.

* **Renamed HiSilicon DMA driver.**

  Renamed ``dma/hisilicon`` to ``dma/hisi_pciep`` (PCIe internal endpoint)
  to reflect hardware IP.

* **Added HiSilicon Accelerator DMA driver.**

  Kunpeng SoC has an internal accelerator unit which includes zip function,
  and the zip also supports data copy and fill.
  This driver exposes this capability to DPDK applications.

* **Allow overriding the automatic usage/help generation in argparse library.**

  The argparse library now supports overriding the automatic help text generation,
  by allowing the user to provide a custom function to generate the output text.
  The built-in help text function is available as a public function which can be reused by custom functions,
  if so desired.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* build: as previously announced in the deprecation notices,
  the ``enable_kmods`` build option has been removed.
  Kernel modules will now automatically be built for OS's where out-of-tree kernel modules
  are required for DPDK operation.
  Currently, this means that modules will only be built for FreeBSD.
  No modules are shipped with DPDK for either Linux or Windows.

* ethdev: As previously announced in deprecation notes,
  queue specific stats fields are now removed from ``struct rte_eth_stats``.
  Mentioned fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``, ``q_errors``.
  Instead queue stats will be received via xstats API.
  Also compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` is removed from public headers.

* telemetry: As previously announced in the deprecation notices,
  the functions ``rte_tel_data_add_array_u64`` and ``rte_tel_data_add_dict_u64`` are removed.
  They are replaced by ``rte_tel_data_add_array_uint`` and ``rte_tel_data_add_dict_uint`` respectively.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * sample: Add a short 1-2 sentence description of the API change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* rawdev: Changed the return type of ``rte_rawdev_get_dev_id()``
  for negative error values.

* pcapng: Changed the API for adding interfaces to include a link type argument.
  The link type was previously hardcoded to the Ethernet link type in the API.
  This argument is added to ``rte_pcapng_add_interface``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * sample: Add a short 1-2 sentence description of the ABI change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* eal: The structure ``rte_mp_msg`` alignment has been updated to 8 bytes to limit unaligned
  accesses in messages payload.

* stack: The structure ``rte_stack_lf_head`` alignment has been updated to 16 bytes
  to avoid unaligned accesses.

* ethdev: Added ``link_connector`` field to ``rte_eth_link`` structure
  to report type of link connector for a port.

* cryptodev: The ``rte_crypto_sm2_op_param`` struct member ``cipher`` to hold ciphertext
  is changed to union data type. This change is required to support partial SM2 calculation
  which is driven by ``RTE_CRYPTO_SM2_PARTIAL`` capability flag.

* cryptodev: The enum ``rte_crypto_asym_xform_type``, struct ``rte_crypto_asym_xform``
  and struct ``rte_crypto_asym_op`` are updated to include new values
  to support ML-KEM and ML-DSA.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested
   with this release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================
