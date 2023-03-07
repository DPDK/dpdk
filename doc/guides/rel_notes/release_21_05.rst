.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.05
==================

New Features
------------

* **Added support for GCC 11 and clang 12.**

  Added support for building with GCC 11.1.1 and clang 12.0.0.

* **Added support for Alpine Linux with musl libc.**

  Added initial support for building DPDK, without modification,
  on Alpine Linux using musl libc and busybox.

* **Added phase-fair lock.**

  Added support for a Phase-fair lock. This provides fairness guarantees.
  It has two ticket pools, one for readers and one for writers.

* **Added support for Marvell CN10K SoC drivers.**

  Added Marvell CN10K SoC support. Marvell CN10K SoCs are based on the Octeon 10
  family of ARM64 processors with the ARM Neoverse N2 core with accelerators for
  packet processing, timers, cryptography, etc.

  * Added common/cnxk driver consisting of a common API to be used by
    net, crypto and event PMDs.
  * Added mempool/cnxk driver which provides the support for the integrated
    mempool device.
  * Added event/cnxk driver which provides the support for the integrated event
    device.

* **Added enhanced ethdev representor syntax.**

  * Introduced representor type of VF, SF and PF.
  * Added support for sub-function and multi-host in representor syntax::

      representor=#            [0,2-4]      /* Legacy VF compatible.         */
      representor=[[c#]pf#]vf# c1pf2vf3     /* VF 3 on PF 2 of controller 1. */
      representor=[[c#]pf#]sf# sf[0,2-1023] /* 1023 SFs.                     */
      representor=[c#]pf#      c2pf[0,1]    /* 2 PFs on controller 2.        */

* **Added queue state in queried Rx/Tx queue info.**

  * Added new field ``queue_state`` to the ``rte_eth_rxq_info`` structure to
    provide the indicated Rx queue state.
  * Added new field ``queue_state`` to the ``rte_eth_txq_info`` structure to
    provide the indicated Tx queue state.

* **Updated meter API.**

  * Added packet mode in the meter profile parameters data structures
    to support metering traffic by packet per second (PPS),
    in addition to the initial bytes per second (BPS) mode (value 0).
  * Added support for pre-defined meter policy via flow action list per color.

* **Added packet integrity match to flow rules.**

  * Added ``RTE_FLOW_ITEM_TYPE_INTEGRITY`` flow item.
  * Added ``rte_flow_item_integrity`` data structure.

* **Added TCP connection tracking offload in flow API.**

  * Added conntrack item and action for stateful connection offload.

* **Updated Amazon ENA PMD.**

  The new driver version (v2.3.0) introduced bug fixes and improvements,
  including:

  * Changed ``memcpy()`` mapping to the dpdk-optimized version.
  * Updated ena_com (HAL) to the latest version.
  * Added indication of the RSS hash presence in the mbuf.

* **Updated Arkville PMD.**

  Updated Arkville net driver with new features and improvements, including:

  * Generalized passing meta data between PMD and FPGA, allowing up to 20
    bytes of user specified information in RX and TX paths.

  * Updated dynamic PMD extensions API using standardized names.

  * Added support for new Atomic Rules PCI device IDs ``0x100f, 0x1010, 0x1017,
    0x1018, 0x1019``.

* **Updated Broadcom bnxt driver.**

  * Updated HWRM structures to 1.10.2.15 version.

* **Updated Hisilicon hns3 driver.**

  * Added support for module EEPROM dumping.
  * Added support for freeing Tx mbuf on demand.
  * Added support for copper port in Kunpeng930.
  * Added support for runtime config to select IO burst function.
  * Added support for outer UDP checksum in Kunpeng930.
  * Added support for query Tx descriptor status.
  * Added support for query Rx descriptor status.
  * Added support for IEEE 1588 PTP.

* **Updated Intel iavf driver.**

  Updated the Intel iavf driver with new features and improvements, including:

  * Added flow filter to support GTPU inner L3/L4 field matching.
  * In AVX512 code, added the new RX and TX paths to use the HW offload
    features. When the HW offload features are configured to be used, the
    offload paths are chosen automatically. In parallel the support for HW
    offload features was removed from the legacy AVX512 paths.

* **Updated Intel ice driver.**

  * Added Intel ice support on Windows.
  * Added GTPU TEID support for DCF switch filter.
  * Added flow priority support for DCF switch filter.

* **Updated Marvell OCTEON TX2 ethdev driver.**

  * Added support for flow action port id.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Added support for matching on packet integrity status.
  * Added support for VXLAN and NVGRE encap as sample actions.
  * Added support for flow COUNT action handle.
  * Support push VLAN on ingress traffic and pop VLAN on egress traffic in E-Switch mode.
  * Added support for pre-defined meter policy API.
  * Added support for ASO (Advanced Steering Operation) meter.
  * Added support for ASO metering by PPS (packet per second).
  * Added support for the monitor policy of Power Management API.
  * Added support for connection tracking.
  * Added support for Sub-Function representors.

* **Updated NXP DPAA driver.**

  * Added support for shared ethernet interface.
  * Added support for external buffers in Tx.

* **Updated NXP DPAA2 driver.**

  * Added support for traffic management.
  * Added support for configurable Tx confirmation.
  * Added support for external buffers in Tx.

* **Updated Wangxun txgbe driver.**

  * Added support for txgbevf PMD.
  * Support device arguments to handle AN training for backplane NICs.
  * Added support for VXLAN-GPE.

* **Enabled vmxnet3 PMD on Windows.**

* **Enabled libpcap-based PMD on Windows.**

  Enabled libpcap-based PMD support on Windows.
  A libpcap distribution, such as Npcap or WinPcap, is required to run the PMD.

* **Updated the AF_XDP driver.**

  * Added support for preferred busy polling.

* **Added support for vhost async packed ring data path.**

  Added packed ring support for async vhost.

* **Added support of multiple data-units in the cryptodev API.**

  The cryptodev library has been enhanced to allow operations on multiple
  data-units for the AES-XTS algorithm. The data-unit length should be set in the
  transformation. A capability for it was added too.

* **Added a cryptodev feature flag to support cipher wrapped keys.**

  A new feature flag has been added to allow applications to provide
  cipher wrapped keys in session xforms.

* **Updated the OCTEON TX crypto PMD.**

  * Added support for ``DIGEST_ENCRYPTED`` mode in the OCTEON TX crypto PMD.

* **Updated the OCTEON TX2 crypto PMD.**

  * Added support for ``DIGEST_ENCRYPTED`` mode in OCTEON TX2 crypto PMD.
  * Added support in lookaside protocol offload mode for IPsec with
    UDP encapsulation support for NAT Traversal.
  * Added support in lookaside protocol offload mode for IPsec with
    IPv4 transport mode.

* **Updated Intel QuickAssist compression PMD.**

  * The compression is now available on all QAT GEN3 devices
    that support it in hardware.

* **Updated Mellanox RegEx PMD.**

  * Added support for multi-segments mbuf.

* **Introduced period timer mode in eventdev timer adapter.**

  * Added support for periodic timer mode in eventdev timer adapter.
  * Added support for periodic timer mode in octeontx2 event device driver.

* **Added event device vector capability.**

  * Added the ``rte_event_vector`` data structure which is capable of holding
    multiple ``uintptr_t`` of the same flow thereby allowing applications
    to vectorize their pipelines and also reduce the complexity of pipelining
    the events across multiple stages.
  * This also reduced the scheduling overhead on a event device.

* **Enhanced crypto adapter forward mode.**

  * Added ``rte_event_crypto_adapter_enqueue()`` API to enqueue events to the
    crypto adapter if forward mode is supported by the driver.
  * Added support for crypto adapter forward mode in octeontx2 event and crypto
    device driver.

* **Updated Intel DLB2 driver.**

  * Added support for the DLB v2.5 device.

* **Added Predictable RSS functionality to the Toeplitz hash library.**

  Added feature for finding collisions of the Toeplitz hash function -
  the hash function used in NICs to spread the traffic among the queues.
  It can be used to get predictable mapping of the flows.

* **Updated testpmd.**

  * Added a command line option to configure forced speed for an Ethernet port:
    ``dpdk-testpmd -- --eth-link-speed N``.
  * Added command to show link flow control info:
    ``show port (port_id) flow_ctrl``.
  * Added command to display Rx queue used descriptor count:
    ``show port (port_id) rxq (queue_id) desc used count``.
  * Added command to cleanup a Tx queue's mbuf on a port:
    ``port cleanup (port_id) txq (queue_id) (free_cnt)``.
  * Added command to dump internal representation information of a single flow:
    ``flow dump (port_id) rule (rule_id)``.
  * Added commands to create and delete meter policy:
    ``add port meter policy (port_id) (policy_id) ...``.
  * Added commands to construct conntrack context and relevant indirect
    action handle creation, update for conntrack action as well as conntrack
    item matching.
  * Added commands for action meter color to color the packet to reflect
    the meter color result:
    ``color type (green|yellow|red)``.

* **Added support for the FIB lookup method in the l3fwd example app.**

  Added support to the l3fwd application to support
  the Forwarding Information Base (FIB) lookup method.
  Previously l3fwd only supported the LPM and Exact Match lookup methods.

* **Updated the ipsec-secgw sample application.**

  * Updated the ``ipsec-secgw`` sample application with UDP encapsulation
    support for NAT Traversal.

* **Added sub-testsuite support.**

  * The unit test suite struct now supports having both a nested
    list of sub-testsuites, and a list of testcases as before.


Removed Items
-------------

* Removed support for Intel DLB V1 hardware. This is not a broad market device,
  and existing customers already obtain the source code directly from Intel.


API Changes
-----------

* eal: The experimental TLS API added in ``rte_thread.h`` has been renamed
  from ``rte_thread_tls_*`` to ``rte_thread_*`` to avoid naming redundancy
  and confusion with the transport layer security term.

* pci: The value ``PCI_ANY_ID`` is marked as deprecated
  and can be replaced with ``RTE_PCI_ANY_ID``.

* ethdev: Added a ``rte_flow`` pointer parameter to the function
  ``rte_flow_dev_dump()`` allowing dumping of a single flow.

* cryptodev: The experimental raw data path API for dequeue
  ``rte_cryptodev_raw_dequeue_burst`` got a new parameter
  ``max_nb_to_dequeue`` to provide flexible control on dequeue.

* ethdev: The experimental flow API for shared action has been generalized
  as a flow action handle used in rules through an indirect action.
  The functions ``rte_flow_shared_action_*`` manipulating the action object
  are replaced with ``rte_flow_action_handle_*``.
  The action ``RTE_FLOW_ACTION_TYPE_SHARED`` is deprecated and can be
  replaced with ``RTE_FLOW_ACTION_TYPE_INDIRECT``.

* ethdev: The experimental function ``rte_mtr_policer_actions_update()``,
  the enum ``rte_mtr_policer_action``, and the struct members
  ``policer_action_recolor_supported`` and ``policer_action_drop_supported``
  have been removed.

* vhost: The vhost library currently populates received mbufs from a virtio
  driver with Tx offload flags while not filling Rx offload flags.
  While this behavior is arguable, it is kept untouched.
  A new flag ``RTE_VHOST_USER_NET_COMPLIANT_OL_FLAGS`` has been added to ask
  for a behavior compliant with the mbuf offload API.

* stack: Lock-free ``rte_stack`` no longer silently ignores push and pop when
  it's not supported on the current platform. Instead ``rte_stack_create()``
  fails and ``rte_errno`` is set to ``ENOTSUP``.

* raw/ioat: The experimental function ``rte_ioat_completed_ops()`` now
  supports two additional parameters, ``status`` and ``num_unsuccessful``,
  to allow the reporting of errors from hardware when performing copy
  operations.


ABI Changes
-----------

* No ABI change that would break compatibility with 20.11.

* The experimental function ``rte_telemetry_legacy_register`` has been
  removed from the public API and is now an internal-only function. This
  function was already marked as internal in the API documentation for it,
  and was not for use by external applications.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Atom\ |trade| CPU C3958 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1541 @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 8.3
    * CentOS Stream 8
    * Fedora 33
    * FreeBSD 12.1
    * OpenWRT 19.07.4
    * Red Hat Enterprise Linux Server release 8.3
    * Suse 15 SP2
    * Ubuntu 20.04
    * Ubuntu 21.04

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 3.00
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.6.0 (ice)
      * OS Default DDP: 1.3.26.0
      * COMMS DDP: 1.3.30.0
      * Wireless Edge DDP: 1.3.6.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 3.00
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.6.0 (ice)
      * OS Default DDP: 1.3.26.0
      * COMMS DDP: 1.3.30.0
      * Wireless Edge DDP: 1.3.6.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x8000113b
      * Device id (pf): 8086:1563
      * Driver version: 5.11.3 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 8.30 0x8000a49d 1.2926.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 5.30 0x80002a29 1.2527.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 8.30 0x8000a485 1.2926.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 8.30 0x8000a4ae 1.2926.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80001001
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 4.18.0-277.el8.x86_64 (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.5.2 (igb)

    * Intel\ |reg| Ethernet Controller 10-Gigabit X540-AT2

      * Firmware version: 0x800005f9
      * Device id (pf): 8086:1528
      * Driver version: 4.18.0-277.el8.x86_64 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.30 0x8000a489 1.2926.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.15.9 (i40e)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

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

    * MLNX_OFED 5.3-1.0.0.1 and above
    * MLNX_OFED 5.2-2.2.0.0

  * upstream kernel:

    * Linux 5.13.0-rc1 and above

  * rdma-core:

    * rdma-core-35.0-1 and above

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCCT (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.30.1004 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.30.1004 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 24.30.1004 and above

  * Embedded software:

    * CentOS Linux release 8.2.2004 (Core)
    * MLNX_OFED 5.3-1.0.0 and above
    * DPDK application running on Arm cores
