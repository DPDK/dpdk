.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.07
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_22_07.html


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

* **Added initial RISC-V architecture support.***

  Added EAL implementation for RISC-V architecture.
  The initial device the porting was tested on is
  a HiFive Unmatched development board based on the SiFive Freedom U740 SoC.
  In theory this implementation should work
  with any ``rv64gc`` ISA compatible implementation
  with MMU supporting a reasonable address space size (U740 uses sv39 MMU).

* **Added Sequence Lock.**

  Added a new synchronization primitive: the sequence lock
  (seqlock). A seqlock allows for low overhead, parallel reads. The
  DPDK seqlock uses a spinlock to serialize multiple writing threads.

* ** Added function get random floating point number.**

  Added the function ``rte_drand()`` to provide a pseudo-random
  floating point number.

* **Added protocol based input color selection for meter.**

  Added new functions ``rte_mtr_color_in_protocol_set()``,
  ``rte_mtr_color_in_protocol_get()``,
  ``rte_mtr_color_in_protocol_priority_get()``,
  ``rte_mtr_meter_vlan_table_update()``
  and updated ``struct rte_mtr_params`` and ``struct rte_mtr_capabilities`` to
  support protocol based input color selection for meter.

* **Added Rx queue available descriptors threshold and event.**

  Added ethdev API and corresponding driver operations to set Rx queue
  available descriptors threshold and query for queues with reached
  threshold when a new event ``RTE_ETH_EVENT_RX_AVAIL_THRESH`` is received.

* **Added telemetry for module EEPROM.**

  Added telemetry command to dump module EEPROM.
  Added support for module EEPROM information format defined in:

    * SFF-8079 revision 1.7
    * SFF-8472 revision 12.0
    * SFF-8636 revision 2.7

* **Added vhost API to get the number of in-flight packets.**

  Added an API which can get the number of in-flight packets in
  vhost async data path without using lock.

* **Added vhost async dequeue API to receive packets from guest.**

  Added vhost async dequeue API which can leverage DMA devices to
  accelerate receiving packets from guest.
  Split virtqueue and packed virtqueue are both supported.

* **Added thread-safe version of in-flight packet clear API in vhost library.**

  Added an API which can clear the in-flight packets submitted to
  the async channel in a thread-safe manner in the vhost async data path.

* **Added vhost API to get the device type of a vDPA device.**

  Added an API which can get the device type of vDPA device.

* **Updated NVIDIA mlx5 vDPA driver.**

  * Added new devargs options ``queue_size`` and ``queues``
    to allow prior creation of virtq resources.
  * Added new devargs option ``max_conf_threads``
    defining the number of management threads for parallel configurations.

* **Updated Amazon ena driver.**

  The new driver version (v2.7.0) includes:

  * Added fast mbuf free feature support.
  * Added ``enable_llq`` device argument for controlling the PMD LLQ
    (Low Latency Queue) mode.

* **Updated Atomic Rules' Arkville PMD.**

  * A firmware version update to Arkville 22.07 is required.
  * Added support for Atomic Rules PCI device IDs ``0x101a, 0x101b, 0x101c``.
  * Added PMD support for virtual functions and vfio_pci driver.

* **Updated HiSilicon hns3 driver.**

  * Added support for backplane media type.

* **Updated Intel iavf driver.**

  * Added Tx QoS queue rate limitation support.
  * Added quanta size configuration support.
  * Added ``DEV_RX_OFFLOAD_TIMESTAMP`` support.
  * Added Protocol Agnostic Flow Offloading support in AVF FDIR and RSS.

* **Updated Intel ice driver.**

 * Added support for RSS RETA configure in DCF mode.
 * Added support for RSS HASH configure in DCF mode.
 * Added support for MTU configure in DCF mode.
 * Added support for promisc configuration in DCF mode.
 * Added support for MAC configuration in DCF mode.
 * Added support for VLAN filter and offload configuration in DCF mode.
 * Added Tx QoS queue / queue group rate limitation configure support.
 * Added Tx QoS queue / queue group priority configuration support.
 * Added Tx QoS queue weight configuration support.

* **Updated Intel igc driver.**

  Added Intel Foxville I226 devices in ``igc`` driver.
  See the doc:`../nics/igc` NIC guide for more details.

* **Updated Mellanox mlx5 driver.**

  * Added support for promiscuous mode on Windows.
  * Added support for MTU on Windows.
  * Added matching and RSS on IPsec ESP.
  * Added matching on represented port.
  * Added support for modifying ECN field of IPv4/IPv6.
  * Added Rx queue available descriptor threshold support.
  * Added host shaper support.

* **Updated Netronome nfp driver.**

  * Added support for NFP3800 NIC.
  * Added support for firmware with NFDk.

* **Updated VMware vmxnet3 networking driver.**

  * Added version 5 support.
  * Added RETA query and RETA update support.
  * Added version 6 support with some new features:

    * Increased maximum MTU up to 9190;
    * Increased maximum number of Rx and Tx queues;
    * Removed power-of-two limitations on Rx and Tx queue size;
    * Extended interrupt structures (required for additional queues).

* **Updated Wangxun ngbe driver.**

  * Added support for yt8531s PHY.
  * Added support for OEM subsystem vendor ID.
  * Added autoneg on/off for external PHY SFI mode.
  * Added support for yt8521s/yt8531s PHY SGMII to RGMII mode.

* **Updated Wangxun txgbe driver.**

  * Added support for OEM subsystem vendor ID.

* **Added Elliptic Curve Diffie-Hellman (ECDH) algorithm in cryptodev.**

  Added support for Elliptic Curve Diffie Hellman (ECDH) asymmetric
  algorithm in cryptodev.

* **Updated OpenSSL crypto driver with 3.0 EVP API.**

  Updated OpenSSL driver to support OpenSSL v3.0 EVP API.
  Backward compatibility with OpenSSL v1.1.1 is also maintained.

* **Updated Marvell cnxk crypto driver.**

  * Added AH mode support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-GMAC support in lookaside protocol (IPsec) for CN9K & CN10K.

* **Updated Intel QuickAssist Technology (QAT) crypto PMD.**

  * Added support for secp384r1 elliptic curve.

* **Added Intel ACC101 baseband PMD.**

  Added a new baseband PMD for Intel ACC101 device.

* **Added eventdev API to quiesce an event port.**

  Added the function ``rte_event_port_quiesce()``
  to quiesce any lcore-specific resources consumed by the event port,
  when the lcore is no more associated with an event port.

* **Added support for setting queue attributes at runtime in eventdev.**

  Added new API ``rte_event_queue_attr_set()``, to set event queue attributes
  at runtime.

* **Added new queues attributes weight and affinity in eventdev.**

  Defined new event queue attributes weight and affinity as below:

  * ``RTE_EVENT_QUEUE_ATTR_WEIGHT``
  * ``RTE_EVENT_QUEUE_ATTR_AFFINITY``

* **Added telemetry to dmadev library.**

  Added telemetry callback functions which allow for a list of DMA devices,
  statistics and other DMA device information to be queried.

* **Added scalar version of the LPM library.**

  Added scalar implementation of ``rte_lpm_lookupx4``.
  This is a fall-back implementation for platforms that
  don't support vector operations.

* **Merged l3fwd-acl into l3fwd example.**

  Merged l3fwd-acl code into l3fwd as l3fwd-acl contains duplicate
  and common functions to l3fwd.

* **Renamed L2 payload RSS type in testpmd.**

  Renamed RSS type ``ether`` to ``l2-payload`` for ``port config all rss``
  command.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


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

* The DPDK header file ``rte_altivec.h``,
  which is a wrapper for the PPC header file ``altivec.h``,
  undefines the AltiVec keyword ``vector``.
  The alternative keyword ``__vector`` should be used instead.

* Experimental structures ``struct rte_mtr_params``
  and ``struct rte_mtr_capabilities`` updated to support
  protocol based input color for meter.


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

* No ABI change that would break compatibility with 21.11.


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

* Intel\ |reg| platforms with NVIDIA \ |reg| NICs combinations

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

    * Red Hat Enterprise Linux release 8.2 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.8 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Red Hat Enterprise Linux Server release 7.3 (Maipo)
    * Red Hat Enterprise Linux Server release 7.2 (Maipo)
    * Ubuntu 20.04
    * Ubuntu 18.04
    * Ubuntu 16.04
    * SUSE Enterprise Linux 15 SP2
    * SUSE Enterprise Linux 12 SP4

  * OFED:

    * MLNX_OFED 5.6-2.0.9.0 and above
    * MLNX_OFED 5.5-1.0.3.2

  * upstream kernel:

    * Linux 5.18.0 and above

  * rdma-core:

    * rdma-core-40.0 and above

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * NVIDIA\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCCT (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.33.1048 and above

    * NVIDIA\ |reg| ConnectX\ |reg| 7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.33.2028 and above

* NVIDIA \ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg| 2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.33.1048 and above

  * Embedded software:

    * Ubuntu 20.04.3
    * MLNX_OFED 5.6-2.0.9.0 and above
    * DPDK application running on Arm cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Red Hat Enterprise Linux Server release 8.2

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.33.1048

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.33.1048

  * OFED:

    * MLNX_OFED 5.6-2.0.9.0
