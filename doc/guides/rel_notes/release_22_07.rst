.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.07
==================

New Features
------------

* **Added initial RISC-V architecture support.**

  Added EAL implementation for RISC-V architecture.
  The initial device the porting was tested on was
  a HiFive Unmatched development board based on the SiFive Freedom U740 SoC.
  In theory this implementation should work
  with any ``rv64gc`` ISA compatible implementation
  with MMU supporting a reasonable address space size (U740 uses sv39 MMU).

* **Added Sequence Lock.**

  Added a new synchronization primitive: the sequence lock
  (seqlock). A seqlock allows for low overhead, parallel reads. The
  DPDK seqlock uses a spinlock to serialize multiple writing threads.

* **Added function to get random floating point number.**

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
  available descriptors threshold and query for queues that have reached the
  threshold when a new event ``RTE_ETH_EVENT_RX_AVAIL_THRESH`` is received.

* **Added telemetry for module EEPROM.**

  Added telemetry command to dump module EEPROM.
  Added support for module EEPROM information format defined in:

    * SFF-8079 revision 1.7
    * SFF-8472 revision 12.0
    * SFF-8636 revision 2.7

* **Added vhost API to get the number of in-flight packets.**

  Added an API which can get the number of in-flight packets in
  the vhost async data path without using lock.

* **Added vhost async dequeue API to receive packets from guest.**

  Added vhost async dequeue API which can leverage DMA devices to
  accelerate receiving packets from a guest.
  Both split and packed virtqueues are supported.

* **Added thread-safe version of in-flight packet clear API in vhost library.**

  Added an API which can clear the in-flight packets submitted to
  the async channel in a thread-safe manner, in the vhost async data path.

* **Added vhost API to get the device type of a vDPA device.**

  Added an API which can get the device type of vDPA devices.

* **Updated NVIDIA mlx5 vDPA driver.**

  * Added new devargs options ``queue_size`` and ``queues``
    to allow prior creation of virtq resources.
  * Added new devargs option ``max_conf_threads``
    defining the number of management threads for parallel configurations.

* **Updated Amazon ENA driver to version 2.7.0.**

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
  See the :doc:`../nics/igc` NIC guide for more details.

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

  Defined new event queue attributes weight and affinity:

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


API Changes
-----------

* The DPDK header file ``rte_altivec.h``,
  which is a wrapper for the PPC header file ``altivec.h``,
  undefines the AltiVec keyword ``vector``.
  The alternative keyword ``__vector`` should be used instead.

* Experimental structures ``struct rte_mtr_params``
  and ``struct rte_mtr_capabilities`` were updated to support
  protocol based input color for meter.


ABI Changes
-----------

* No ABI change that would break compatibility with 21.11.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180M CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz

  * OS:

    * Fedora 35
    * FreeBSD 13.0
    * Red Hat Enterprise Linux Server release 8.4
    * Red Hat Enterprise Linux Server release 8.5
    * CentOS7.9
    * Ubuntu 20.04.4
    * Ubuntu 22.04

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.00 0x80011845 1.3236.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.9.5_dirty (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0
      * Wireless Edge DDP: 1.3.10.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.00 0x800117e8 1.3236.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.9.5_dirty (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0
      * Wireless Edge DDP: 1.3.10.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.00 0x800117e5 1.3236.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.9.5_dirty (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.15.2 (ixgbe)
      * Driver version(in-tree): 5.15.0-27-generic (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 8.70 0x8000c3d5 1.3179.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.19.3 (i40e)
      * Driver version(in-tree): 5.15.0-27-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 5.60 0x800035cb 1.3179.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.19.3 (i40e)
      * Driver version(in-tree): 5.13.0-30-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T

      * Firmware version: 5.60 0x8000357f 1.2935.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version(out-tree): 2.19.3 (i40e)
      * Driver version(in-tree): 5.13.0-30-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 8.70 0x8000c3eb 1.3179.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.19.3 (i40e)
      * Driver version(in-tree): 5.15.0-27-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 8.70 0x8000c40f 1.3179.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.19.3 (i40e)
      * Driver version(in-tree): 5.15.0-27-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.70 0x8000c3e3 1.3179.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.19.3 (i40e)

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

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.33.2028 and above

* NVIDIA \ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

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
