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
  * Report link type, mode and status.
  * Configure link mode.

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

* **Added RCU support in the FIB6 library.**

  It is now possible to register an RCU QSBR object
  to handle graceful deletion of table groups.

* **Added packet capture (pdump) for secondary process.**

  Added multi-process support to allow packets sent and received
  by secondary process to be visible in packet capture.

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

* net/mlx5: ``repr_matching_en`` device argument has been removed.
  Applications which disabled this option were able to receive traffic
  from any physical port/VF/SF on any representor port.
  Specifically, in most cases, this was used to process all traffic on representor port
  which is a transfer proxy port.
  Similar behavior in mlx5 PMD can be achieved without this device argument,
  by using ``RTE_FLOW_ACTION_TYPE_RSS`` in transfer flow rules.


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

* bitmap: Changed the return type of ``rte_bitmap_free()`` to void
  for consistency with other free functions.


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

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| x7835RE
    * Intel\ |reg| Xeon\ |reg| 6553P-B CPU @ 2.6GHz
    * Intel\ |reg| Xeon\ |reg| 6767P CPU @ 2.4GHz
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480 CPU @ 1.9GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6330 CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H CPU @ 2.1GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H CPU @ 1.9GHz

  * OS:

    * Fedora 42
    * FreeBSD 14.3
    * Microsoft Azure Linux 3.0
    * OpenAnolis OS 8.10
    * openEuler 24.03 (LTS-SP2)
    * Red Hat Enterprise Linux Server release 10
    * Red Hat Enterprise Linux Server release 9.6
    * Ubuntu 24.04 LTS
    * Ubuntu 24.04.3 LTS
    * Vmware ESXi 8.0.3

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.90 0x80020eeb 1.3863.0
      * Device ID (PF/VF): 8086:1593 / 8086:1889
      * Driver version (out-of-tree): 2.3.14 (ice)
      * Driver version (in-tree): 6.8.0-86-generic (Ubuntu24.04 LTS) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.90 0x80020ef2 1.3863.0
      * Device ID (PF/VF): 8086:1592 / 8086:1889
      * Driver version (out-of-tree): 2.3.14 (ice)
      * Driver version (in-tree): 6.6.12.1-1.azl3+ice+ (Microsoft Azure Linux 3.0) /
        6.12.0-55.9.1.el10_0.x86_64+rt (RHEL10) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E830-CC for SFP

      * Firmware version: 1.20 0x800179c0 1.3862.0
      * Device ID (PF/VF): 8086:12d3 / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E830-CC for QSFP

      * Firmware version: 1.20 0x800179c1 1.3862.0
      * Device ID (PF/VF): 8086:12d2 / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 4.04 0x80007e5c 1.3885.0
      * Device ID (PF/VF): 8086:579d / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.30 0x8000e8a2 1.3864.0
      * Device ID (PF/VF): 8086:57b0 / 8086:57ad
      * Driver version (out-of-tree): 6.2.5 (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT4

      * Firmware version: 1.30 0x8000e8a5 1.3864.0
      * Device ID (PF/VF): 8086:57b0 / 8086:57ad
      * Driver version (out-of-tree): 6.2.5 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.55 0x8000fdf4 1.3862.0
      * Device ID (PF/VF): 8086:1572 / 8086:154c
      * Driver version (in-tree): 6.8.0-88-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.51 0x80004430 1.3862.0
      * Device ID (PF/VF): 8086:37d0 / 8086:37cd
      * Driver version (out-of-tree): 2.28.13 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.55 0x8000fe4c 1.3862.0
      * Device ID (PF/VF): 8086:158b / 8086:154c
      * Driver version (out-of-tree): 2.28.13 (i40e)
      * Driver version (in-tree): 6.8.0-88-generic (Ubuntu24.04.3 LTS) /
        6.12.0-55.9.1.el10_0.x86_64 (RHEL10) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.55 0x8000fe31 1.3862.0
      * Device ID (PF/VF): 8086:1583 / 8086:154c
      * Driver version (out-of-tree): 2.28.13 (i40e)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device ID (PF): 8086:125b
      * Driver version (in-tree): 6.8.0-87-generic (Ubuntu24.04.3 LTS) (igc)

    * Intel\ |reg| Corporation Ethernet Server Adapter I350-T4

      * Firmware version: 1.63, 0x8000116b
      * Device ID (PF/VF): 8086:1521 /8086:1520
      * Driver version: 6.14.0-35-generic (Ubuntu 24.04.3 LTS) (igb)

    * Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100

      * Firmware version: ci-ts.release.2.0.0.11126
      * Device ID (idpf/cpfl): 8086:1452/8086:1453
      * Driver version: 0.0.772 (idpf)

* Intel\ |reg| platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 9.2 (Plow)
    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Ubuntu 24.04
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP4

  * DOCA:

    * DOCA 3.2.0-125000 and above.

  * upstream kernel:

    * Linux 6.17.0 and above

  * rdma-core:

    * rdma-core-58.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-8 SuperNIC 400G  MT4131 - 900-9X81Q-00CN-STA (2x400G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1023
      * Firmware version: 40.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-9 SuperNIC 800G  MT4133 - 900-9X91E-00EB-STA_Ax (1x800G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1025
      * Firmware version: 82.47.0366 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.47.1026 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.47.1026 and above

  * Embedded software:

    * Ubuntu 24.04
    * MLNX_OFED 25.10-1.2.8.0
    * bf-bundle-3.2.0-113_25.10_ubuntu-24.04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.47.1026 and above

   * DOCA:

      * DOCA 3.2.0-125000 and above
