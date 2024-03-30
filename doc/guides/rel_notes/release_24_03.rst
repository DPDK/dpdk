.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.03
==================

New Features
------------

* **Added HiSilicon UACCE bus support.**

  Added UACCE (Unified/User-space-access-intended Accelerator Framework) bus
  driver so that the accelerator devices such as a compress, crypto,
  DMA and ethernet devices could be seen and registered in DPDK.

* **Introduced argument parsing library.**

  The argparse library was added to help writing user-friendly applications,
  replacing ``getopt()`` usage.

* **Improved RSS hash algorithm support.**

  Added new function ``rte_eth_find_rss_algo``
  to get RSS hash algorithm by its name.

* **Added query of used descriptors number in Tx queue.**

  * Added a fast path function ``rte_eth_tx_queue_count``
    to get the number of used descriptors for a Tx queue.

* **Added hash calculation of an encapsulated packet as done by the HW.**

  Added function to calculate hash when doing tunnel encapsulation:
  ``rte_flow_calc_encap_hash()``

* **Added flow matching items and action.**

  * Added ``RTE_FLOW_ITEM_TYPE_COMPARE`` to allow matching
    on comparison result between packet fields or value.
  * Added ``RTE_FLOW_ITEM_TYPE_RANDOM`` to match a random value,
    and ``RTE_FLOW_FIELD_RANDOM`` to represent it with a field ID.
  * Added ``RTE_FLOW_ACTION_TYPE_NAT64`` to offload header conversion
    between IPv4 and IPv6.

* **Added flow template table resizing.**

  * ``RTE_FLOW_TABLE_SPECIALIZE_RESIZABLE_TABLE`` table configuration bit.
    Set at table creation to allow future resizing.
  * ``rte_flow_template_table_resizable()``.
    Query whether template table can be resized.
  * ``rte_flow_template_table_resize()``.
    Reconfigure template table for new flows capacity.
  * ``rte_flow_async_update_resized()``.
    Reconfigure flows for the updated table configuration.
  * ``rte_flow_template_table_resize_complete()``.
    Complete table resize.

* **Updated Amazon ena (Elastic Network Adapter) net driver.**

  * Removed the reporting of ``rx_overruns`` errors from xstats
    and instead updated ``imissed`` counter with its value.
  * Added support for sub-optimal configuration notifications from the device.
  * Added ``normal_llq_hdr`` devarg that enforces normal LLQ header policy.
  * Added support for LLQ header size recommendation from the device.
  * Allowed large LLQ with 1024 entries when the device supports enlarged memory BAR.
  * Added `control_poll_interval` devarg that configures the control-path to work in poll-mode.
  * Added support for binding ports to `uio_pci_generic` kernel module.

* **Updated Atomic Rules' Arkville driver.**

  * Added support for Atomic Rules' TK242 packet-capture family of devices
    with PCI IDs: ``0x1024, 0x1025, 0x1026``.

* **Updated Broadcom bnxt driver.**

  * Added support for 5760X device family.

* **Updated HiSilicon hns3 ethdev driver.**

  * Added new device supporting RDMA/DCB/ROH with PCI IDs: ``0xa227, 0xa22c``.

* **Updated Marvell cnxk net driver.**

  * Added support for port representors.
  * Added support for ``RTE_FLOW_ITEM_TYPE_PPPOES`` flow item.
  * Added support for ``RTE_FLOW_ACTION_TYPE_SAMPLE`` flow item.
  * Added support for Rx inject.
  * Added support for ``rte_eth_tx_queue_count()``.
  * Optimized SW external mbuf free for better performance and avoid SQ corruption.

* **Updated Marvell OCTEON EP driver.**

  * Optimized mbuf rearm sequence.
  * Updated Tx queue mbuf free thresholds from 128 to 256 for better performance.
  * Updated Rx queue mbuf refill routine to use mempool alloc and reorder it
    to avoid mbuf write commits.
  * Added option to control ISM memory accesses which gives better performance
    for lower packet sizes when enabled.
  * Added optimized SSE Rx routines.
  * Added optimized AVX2 Rx routines.
  * Added optimized NEON Rx routines.

* **Updated NVIDIA mlx5 driver.**

  * Added support for VXLAN-GPE matching in DV and HWS flow engines.
  * Added support for GENEVE matching and modifying in HWS flow engine.
  * Added support for modifying IPv4 proto field in HWS flow engine.
  * Added support for modifying IPsec ESP fields in HWS flow engine.
  * Added support for modifying IPv6 traffic class field in HWS flow engine.
  * Added support for modifying IPv6 flow label field in HWS flow engine.
  * Added support for matching a random value.
  * Added support for comparing result between packet fields or value.
  * Added support for accumulating value of field into another one.
  * Added support for copying inner fields in HWS flow engine.
  * Added support for ``RTE_FLOW_ACTION_TYPE_NAT64`` flow action in HWS flow engine.
  * Added support for sharing indirect action objects
    of type ``RTE_FLOW_ACTION_TYPE_METER_MARK`` and ``RTE_FLOW_ACTION_TYPE_CONNTRACK``
    in HWS flow engine.

* **Updated Intel QuickAssist Technology driver.**

  * Enabled support for new QAT GEN3 (578a) and QAT GEN5 (4946)
    devices in QAT crypto driver.
  * Enabled ZUC256 cipher and auth algorithm for wireless slice
    enabled GEN3 and GEN5 devices.
  * Added support for GEN LCE (1454) device, for AES-GCM only.
  * Enabled support for virtual QAT - vQAT (0da5) devices in QAT crypto driver.

* **Updated Marvell cnxk crypto driver.**

  * Added support for Rx inject in crypto_cn10k.
  * Added support for TLS record processing in crypto_cn10k
    to support TLS v1.2, TLS v1.3 and DTLS v1.2.
  * Added PMD API to allow raw submission of instructions to CPT.

* **Added Marvell Nitrox compression driver.**

  Added a new compression driver for Marvell Nitrox devices to support
  the deflate compression and decompression algorithm.

* **Updated Marvell cnxk eventdev driver.**

  * Added power-saving during polling within the ``rte_event_dequeue_burst()`` API.
  * Added support for DMA adapter.

* **Added DMA producer mode in eventdev test.**

  Added DMA producer mode to measure performance of ``OP_FORWARD`` mode
  of event DMA adapter.


Removed Items
-------------

* log: Removed the statically defined logtypes that were used internally by DPDK.
  All code should be using the dynamic logtypes (see ``RTE_LOG_REGISTER()``).
  The application reserved statically defined logtypes ``RTE_LOGTYPE_USER1..RTE_LOGTYPE_USER8``
  are still defined.

* acc101: Removed obsolete code for non productized HW variant.


API Changes
-----------

* eal: Removed ``typeof(type)`` from the expansion of ``RTE_DEFINE_PER_LCORE``
  and ``RTE_DECLARE_PER_LCORE`` macros aligning them with their intended design.
  If use with an expression is desired applications can adapt by supplying
  ``typeof(e)`` as an argument.

* eal: Improved ``RTE_BUILD_BUG_ON`` by using C11 ``static_assert``.
  Non-constant expressions are now rejected instead of being silently ignored.

* gso: ``rte_gso_segment`` now returns ``-ENOTSUP`` for unknown protocols.

* ethdev: Renamed structure ``rte_flow_action_modify_data`` to be
  ``rte_flow_field_data`` for more generic usage.


ABI Changes
-----------

* No ABI change that would break compatibility with 23.11.


Tested Platforms
----------------

* AMD platforms

  * CPU

    * AMD EPYC\ |trade| 7543 32-Core Processor @ 3.70GHz

      * BIOS 7.00.30.00

    * AMD EPYC\ |trade| 8534 64-Core Processor @ 3.10GHz

      * BIOS 7.00.00.00

  * OS:

    * Ubuntu 22.04.4 LTS

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel Atom\ |reg| P5342 processor
    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-1747NTE CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H

  * OS:

    * CBL Mariner 2.0
    * Fedora 39
    * FreeBSD 14.0
    * OpenAnolis OS 8.8
    * openEuler 22.03 (LTS-SP2)
    * Red Hat Enterprise Linux Server release 8.7
    * Red Hat Enterprise Linux Server release 9.0
    * Red Hat Enterprise Linux Server release 9.2
    * Ubuntu 22.04.3

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.40 0x8001c982 1.3534.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.13.7 (ice)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3) /
        5.14.0-284.11.1.rt14.296.el9_2.x86_64 (RHEL9.2) (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.40 0x8001c967 1.3534.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.13.7 (ice)
      * Driver version(in-tree): 5.15.55.1-1.cm2-5464b22cac7+ (CBL Mariner 2.0) (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.40 0x8001af86 1.3444.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.13.7 (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0

    * Intel\ |reg| Ethernet Connection E823-C for QSFP

      * Firmware version: 3.33 0x8001b295 1.3443.0
      * Device id (pf/vf): 8086:188b / 8086:1889
      * Driver version: 1.13.7 (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Connection E823-L for QSFP

      * Firmware version: 3.33 0x8001b4b0 1.3429.0
      * Device id (pf/vf): 8086:124c / 8086:1889
      * Driver version: 1.13.7 (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Connection E822-L for backplane

      * Firmware version: 3.33 0x8001b4b6 1.3429.0
      * Device id (pf/vf): 8086:1897 / 8086:1889
      * Driver version: 1.13.7 (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.19.9 (ixgbe)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3) /
        5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.40 0x8000eca2 1.3429.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.24.6 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.20 0x80003d82 1.3353.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.24.6 (i40e)
      * Driver version(in-tree): 5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 9.40 0x8000ed12 1.3429.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.24.6 (i40e)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3) /
        5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.40 0x8000ece4 1.3429.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.24.6 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)(igc)

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

    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Red Hat Enterprise Linux release 8.4 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.9 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP2

  * OFED:

    * MLNX_OFED 24.01-0.3.3.1 and above

  * upstream kernel:

    * Linux 6.8.0 and above

  * rdma-core:

    * rdma-core-50.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.40.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.40.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.40.1000 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.40.1000 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.40.1000 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 24.01-0.3.3.0 and above
    * DOCA_2.6.0_BSP_4.6.0_Ubuntu_22.04-5.24-01
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
      * Firmware version: 22.40.1000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.40.1000 and above

  * OFED:

    * MLNX_OFED 24.01-0.3.3.1
