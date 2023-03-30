.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.08
==================

New Features
------------

* **Added non-EAL threads registration API.**

  Added a new API to register non-EAL threads as lcores. This can be used by
  applications to have its threads known to DPDK without suffering from the
  non-EAL previous limitations in terms of performance.

* **rte_*mb APIs are updated to use the DMB instruction for ARMv8.**

  ARMv8 memory model has been strengthened to require other-multi-copy
  atomicity. This allows for using the DMB instruction instead of DSB for IO
  barriers. The rte_*mb APIs, for ARMv8 platforms, have changed to use the DMB
  instruction to reflect this.

* **Added support for RTS and HTS modes into mempool ring driver.**

  Added the ability to select new ring synchronisation modes:
  ``relaxed tail sync (ring_mt_rts)`` and ``head/tail sync (ring_mt_hts)``
  via the mempool ops API.

* **Added the support for vfio-pci new VF token interface.**

  From Linux 5.7, vfio-pci supports to bind both SR-IOV PF and the created
  VFs, where it uses a shared VF token (UUID) to represent the collaboration
  between PF and VFs. The DPDK PCI driver has been updated to gain the access
  to the PF and VFs devices by appending the VF token parameter.

* **Added the RegEx Library, a generic RegEx service library.**

  Added the RegEx library which provides an API for offload of regular
  expressions search operations to hardware or software accelerator devices.

  Added Mellanox RegEx PMD, allowing to offload RegEx searches.

* **Added vhost async data path APIs.**

  4 new APIs have been added to enable vhost async data path, including:

  * Async device channel register/unregister APIs.
  * Async packets enqueue/completion APIs (only split ring was implemented).

* **Added vDPA device APIs to query virtio queue statistics.**

  Added new vDPA APIs to query virtio queue statistics, to get their names and
  to reset them.

* **Updated Mellanox mlx5 vDPA driver.**

  Updated Mellanox mlx5 vDPA driver with new features, including:

  * Added support for virtio queue statistics.
  * Added support for MTU update.

* **Added eCPRI protocol support in rte_flow.**

  The ``ECPRI`` item has been added to support eCPRI packet offloading for
  5G network.

* **Introduced send packet scheduling based on timestamps.**

  Added a new mbuf dynamic field and flag to provide a timestamp on which
  packet transmitting can be synchronized. A device Tx offload flag has been
  added to indicate the PMD supports send scheduling.

* **Updated PCAP driver.**

  Updated PCAP driver with new features and improvements, including:

  * Support software Tx nanosecond timestamps precision.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with new features and improvements, including:

  * Added support for VF representors.
  * Added support for multiple devices.
  * Added support for new resource manager API.
  * Added support for VXLAN encap/decap.
  * Added support for rte_flow_query for COUNT action.
  * Added support for rx_burst_mode_get and tx_burst_mode_get.
  * Added vector mode support for ARM CPUs.
  * Added support for VLAN push and pop actions.
  * Added support for NAT action items.
  * Added TruFlow hash API for common hash uses across TruFlow core functions.

* **Updated Cisco enic driver.**

  * Added support for VLAN push and pop flow actions.

* **Updated Hisilicon hns3 driver.**

  * Added support for 200G speed rate.
  * Added support for copper media type.
  * Added support for keeping CRC.
  * Added support for LRO.
  * Added support for setting VF PVID by PF driver.

* **Updated Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for DCF datapath configuration.
  * Added support for more PPPoE packet type for switch filter.
  * Added RSS support for IPv6 32bit, 48bit, 64bit prefix.
  * Added RSS support for GTPU, L2TP, ESP, AH, PFCP and PPPoE.
  * Added support for FDIR filter by GTPU outer IPv4 and IPv6.

* **Updated Intel iavf driver.**

  Updated iavf PMD with new features and improvements, including:

  * Added support for FDIR filter by GTPU outer IPv4 and IPv6.
  * Added IPv6 RSS support for GTPU.

* **Updated Intel i40e driver.**

  Updated i40e PMD with new features and improvements, including:

  * Supported cloud filter for IPv4/6_TCP/UDP/SCTP with SRC port only or DST port only.
  * Re-implemented ``get_fdir_info`` and ``get_fdir_stat`` in private API.
  * Re-implemented ``set_gre_key_len`` in private API.
  * Added support for flow query RSS.

* **Updated Intel ixgbe driver.**

  Updated the Intel ixgbe driver with new features and improvements, including:

  * Re-implemented ``get_fdir_info`` and ``get_fdir_stat`` in private API.

* **Updated Marvell octeontx2 ethdev PMD.**

  Updated Marvell octeontx2 driver with cn98xx support.

* **Updated Mellanox mlx5 net driver and common layer.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added mlx5 PCI layer to share a PCI device among multiple PMDs.
  * Added support for eCPRI protocol offloading.
  * Added devargs options ``reclaim_mem_mode``, ``sys_mem_en``,
    ``lacp_by_user`` and ``decap_en``.

* **Updated NXP dpaa ethdev PMD.**

  Updated the NXP dpaa ethdev with new features and improvements, including:

  * Added support for link status and interrupt.
  * Added support to use datapath APIs from non-EAL pthread.

* **Updated NXP dpaa2 ethdev PMD.**

  Updated the NXP dpaa2 ethdev with new features and improvements, including:

  * Added support to use datapath APIs from non-EAL pthread.
  * Added support for dynamic flow management.

* **Added DOCSIS protocol to rte_security.**

  Added support for combined crypto and CRC operations for the DOCSIS protocol
  to ``rte_security`` API.

* **Updated the AESNI MB crypto PMD.**

  Added support for lookaside protocol offload for DOCSIS through the
  ``rte_security`` API.

* **Updated the QuickAssist Technology (QAT) PMD.**

  * Added support for lookaside protocol offload in QAT crypto PMD
    for DOCSIS through the ``rte_security`` API.
  * Added Chacha20-Poly1305 AEAD algorithm in QAT crypto PMD.
  * Improved handling of multi process in QAT crypto and compression PMDs.
  * Added support for Intel GEN2 QuickAssist device 200xx
    (PF device id 0x18ee, VF device id 0x18ef).

* **Updated the OCTEON TX2 crypto PMD.**

  * Added Chacha20-Poly1305 AEAD algorithm support in OCTEON TX2 crypto PMD.

  * Updated the OCTEON TX2 crypto PMD to support ``rte_security`` lookaside
    protocol offload for IPsec.

* **Added support for BPF_ABS/BPF_IND load instructions.**

  Added support for two BPF non-generic instructions:
  ``(BPF_ABS | <size> | BPF_LD)`` and ``(BPF_IND | <size> | BPF_LD)``
  which are used to access packet data in a safe manner. Currently JIT support
  for these instructions is implemented for x86 only.

* **Added new testpmd forward mode.**

  Added new ``5tswap`` forward mode to testpmd.
  the  ``5tswap`` swaps source and destination in layers 2,3,4
  for ipv4 and ipv6 in L3 and UDP and TCP in L4.

* **Added flow performance test application.**

  Added new application to test ``rte_flow`` performance, including:

  * Measure ``rte_flow`` insertion rate.
  * Measure ``rte_flow`` deletion rate.
  * Dump ``rte_flow`` memory consumption.
  * Measure packet per second forwarding.

* **Added --portmap command line parameter to l2fwd example.**

  Added new command line option ``--portmap="(port, port)[,(port, port)]"`` to
  pass forwarding port details.
  See the :doc:`../sample_app_ug/l2_forward_real_virtual` for more
  details of this parameter usage.

* **Updated ipsec-secgw sample application.**

  Added ``rte_flow`` based rules, which allows hardware parsing and steering
  of ingress packets to specific NIC queues.
  See the :doc:`../sample_app_ug/ipsec_secgw` for more details.


Removed Items
-------------

* Removed ``RTE_KDRV_NONE`` based PCI device driver probing.


API Changes
-----------

* ``rte_page_sizes`` enumeration is replaced with ``RTE_PGSIZE_xxx`` defines.

* vhost: The API of ``rte_vhost_host_notifier_ctrl`` was changed to be per
  queue and not per device, a qid parameter was added to the arguments list.


ABI Changes
-----------

* No ABI change that would break compatibility with 19.11.


Known Issues
------------

* **mlx5 PMD does not work on Power 9 with OFED 5.1-0.6.6.0.**

  Consider using the newer OFED releases, the previous
  OFED 5.0-2.1.8.0, or upstream rdma-core library v29 and above.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Atom\ |trade| CPU C3858 @ 2.00GHz
    * Intel\ |reg| Atom\ |trade| CPU C3958 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1541 @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 0 @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 5218N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 7.7
    * CentOS 8.0
    * CentOS 8.2
    * Fedora 32
    * FreeBSD 12.1
    * OpenWRT 19.07
    * Red Hat Enterprise Linux Server release 8.2
    * Suse15 SP1
    * Ubuntu 16.04
    * Ubuntu 18.04
    * Ubuntu 20.04

  * NICs:

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.7.1 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.7.1 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 7.20 0x800079e8 1.2585.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.12.6 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 4.11 0x80001def 1.1999.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.12.6 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.12.6 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 7.30 0x800080a2 1.2658.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.12.6 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 7.30 0x800080ab 1.2658.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.12.6 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000cbc
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Ethernet Controller 10-Gigabit X540-AT2

      * Firmware version: 0x800005f9
      * Device id (pf): 8086:1528
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 7.30 0x80008061 1.2585.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.12.6(i40e)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
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

    * MLNX_OFED 5.0-2.1.8.0
    * MLNX_OFED 5.1-0.6.6.0 and above

  * upstream kernel:

    * Linux 5.8.0-rc6 and above

  * rdma-core:

    * rdma-core-30.0-1 and above

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
      * Firmware version: 14.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.28.1002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.28.1002 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 24.28.1002

  * Embedded software:

    * CentOS Linux release 7.6.1810 (AltArch)
    * MLNX_OFED 5.1-0.6.2
    * DPDK application running on Arm cores

* IBM Power 9 platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Red Hat Enterprise Linux Server release 7.6

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.28.1002

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.28.1002

  * OFED:

    * MLNX_OFED 5.0-2.1.8.0

* Intel\ |reg| platforms with Broadcom\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2667 v3 @ 3.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v2 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6142 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Silver 4110 CPU @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux Server release 8.1
    * Red Hat Enterprise Linux Server release 7.6
    * Red Hat Enterprise Linux Server release 7.5
    * Ubuntu 16.04
    * Centos 8.1
    * Centos 7.7

  * upstream kernel:

    * Linux 5.3.4

  * NICs:

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P225p (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Firmware version: 214.4.81.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 216.4.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 216.1.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 216.1.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 216.1.259.0 and above

* Broadcom\ |reg| NetXtreme-S\ |reg| Series SmartNIC

  * Broadcom\ |reg| NetXtreme-S\ |reg| Series PS225-H16 SmartNIC (2x25G)

    * Host interface: PCI Express 3.0 x8
    * Firmware version: 217.0.59.0

  * Embedded software:

    * Broadcom Yocto Linux
    * Kernel version: 4.14.174
    * DPDK application running on 8 Arm Cortex-A72 cores
