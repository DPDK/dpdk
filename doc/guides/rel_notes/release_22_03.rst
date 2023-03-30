.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.03
==================

New Features
------------

* **Added ability to reuse hugepages in Linux.**

  It is now possible to reuse files in hugetlbfs to speed up hugepage mapping,
  which may be useful for fast restart and large allocations.
  The new mode is activated with ``--huge-unlink=never``
  and has security implications, refer to the user and programmer guides.

* **Added functions to calculate UDP/TCP checksum in mbuf.**

  Added the following functions to calculate UDP/TCP checksum of packets
  which can be over multi-segments:

  - ``rte_ipv4_udptcp_cksum_mbuf()``
  - ``rte_ipv4_udptcp_cksum_mbuf_verify()``
  - ``rte_ipv6_udptcp_cksum_mbuf()``
  - ``rte_ipv6_udptcp_cksum_mbuf_verify()``

* **Added functions to configure the flow engine.**

  Added the ``rte_flow_configure`` API to configure the flow management
  engine, allowing preallocation of some resources for better performance.
  Added ``rte_flow_info_get`` API to retrieve available resources.

  Added ``rte_flow_template_table_create`` API to group flow rules
  with the same flow attributes and common matching patterns and actions
  defined by ``rte_flow_pattern_template_create`` and
  ``rte_flow_actions_template_create`` respectively.
  The corresponding functions to destroy these entities are:
  ``rte_flow_template_table_destroy``, ``rte_flow_pattern_template_destroy``
  and ``rte_flow_actions_template_destroy``.

* **Added functions for asynchronous flow rules creation and destruction.**

  Added the ``rte_flow_async_create`` and ``rte_flow_async_destroy`` APIs
  to enqueue flow creation/destruction operations asynchronously as well as
  ``rte_flow_pull`` to poll and retrieve results of these operations and
  ``rte_flow_push`` to push all the in-flight	operations to the NIC.

  Added asynchronous APIs for indirect actions management:

  - ``rte_flow_async_action_handle_create``
  - ``rte_flow_async_action_handle_destroy``
  - ``rte_flow_async_action_handle_update``

* **Added rte_flow support for matching GRE optional fields.**

  Added ``gre_option`` item in rte_flow to support checksum/key/sequence
  matching in GRE packets.

* **Added new RSS offload types for L2TPv2 in RSS flow.**

  Added ``RTE_ETH_RSS_L2TPV2`` macro so that he L2TPv2 session ID field can be used as
  input set for RSS.

* **Added IP reassembly Ethernet offload APIs to get and set config.**

  Added IP reassembly offload APIs which provide functions to query IP
  reassembly capabilities, to set configuration and to get currently set
  reassembly configuration.

* **Added an API to enable queue based priority flow ctrl (PFC).**

  Added new APIs, ``rte_eth_dev_priority_flow_ctrl_queue_info_get()`` and
  ``rte_eth_dev_priority_flow_ctrl_queue_configure()`` for queue based
  priority flow ctrl (PFC).

* **Added a private dump API, to dump private info from device.**

  Added the private dump API which provides a facility for querying private info from a device.
  There exists many private properties in different PMD drivers and
  the information in these properties is useful for debugging.

* **Updated AF_XDP PMD.**

  * Added support for libxdp >= v1.2.2.
  * Re-enabled secondary process support. RX/TX is not supported.

* **Updated Amazon ENA PMD.**

  The new driver version (v2.6.0) introduces bug fixes and improvements, including:

  * Added new checksum related xstats: ``l3_csum_bad``, ``l4_csum_bad`` and
    ``l4_csum_good``.
  * Added support for the link status configuration.
  * Added optimized memcpy support for the ARM platforms.
  * Added ENA admin queue support for the MP applications.
  * Added free Tx mbuf on demand feature support.
  * Added ``rte_eth_xstats_get_names_by_id`` API support.
  * Added ``miss_txc_to`` device argument for setting the Tx completion timeout.

* **Updated Cisco enic driver.**

  * Added rte_flow support for matching GENEVE packets.
  * Added rte_flow support for matching eCPRI packets.

* **Updated Intel iavf driver.**

  * Added L2TPv2 (include PPP over L2TPv2) RSS support based on outer
    MAC src/dst address and L2TPv2 session ID.
  * Added L2TPv2 (include PPP over L2TPv2) FDIR support based on outer
    MAC src/dst address and L2TPv2 session ID.
  * Added PPPoL2TPv2oUDP FDIR distribute packets based on inner IP
    src/dst address and UDP/TCP src/dst port.

* **Updated Marvell cnxk ethdev PMD.**

  * Added queue based priority flow control support for CN9K and CN10K.
  * Added support for IP reassembly for inline inbound IPsec packets.
  * Added support for packet marking in traffic manager.
  * Added support for CNF95xx B0 variant SoC.

* **Updated Mellanox mlx5 driver.**

  * Added support for ConnectX-7 capability to schedule traffic sending on timestamp.
  * Added WQE based hardware steering support with ``rte_flow_async`` API.
  * Added steering for external Rx queue created outside the PMD.
  * Added GRE optional fields matching.

* **Updated Wangxun ngbe driver.**

  * Added support for devices of custom PHY interfaces.

    - M88E1512 PHY connects to RJ45
    - M88E1512 PHY connects to RGMII combo
    - YT8521S PHY connects to SFP

  * Added LED OEM support.

* **Updated Wangxun txgbe driver.**

  Added LED OEM support.

* **Added an API for private user data in asymmetric crypto session.**

  An API was added to get/set an asymmetric crypto session's user data.

* **Updated Marvell cnxk crypto PMD.**

  * Added SHA256-HMAC support in lookaside protocol (IPsec) for CN10K.
  * Added SHA384-HMAC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added SHA512-HMAC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-CTR support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added NULL cipher support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-XCBC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-CMAC support in CN9K & CN10K.
  * Added ESN and anti-replay support in lookaside protocol (IPsec) for CN10K.

* **Updated Intel QuickAssist Technology crypto PMD.**

  * Added support for CPM2.0b (4942) devices.
  * Added ECDSA algorithm support.
  * Added ECPM algorithm support.

* **Added an API to retrieve event port id of ethdev Rx adapter.**

  The new API ``rte_event_eth_rx_adapter_event_port_get()`` was added.

* **Added support for Kunpeng930 DMA devices to HiSilicon DMA PMD.**

  Kunpeng930 DMA devices are now enabled for HiSilicon DMA PMD.

* **Added CNXK GPIO PMD.**

  Added a new rawdevice PMD which allows managing userspace GPIOs and installing
  custom GPIO interrupt handlers which bypass the kernel. This is especially useful
  for applications that as well as providing standard dataplane functionality
  also want to have fast and low latency access to GPIO pin state.

  See the :doc:`../rawdevs/cnxk_gpio` rawdev guide for more details on this
  driver.

* **Updated testpmd to support software UDP/TCP checksum over multiple segments.**

  Updated ``rte_ipv4/6_udptcp_cksum_mbuf()`` functions in testpmd csum mode
  to support software UDP/TCP checksum over multiple segments.

* **Added crypto producer mode in test-eventdev.**

  Crypto producer mode helps to measure performance of OP_NEW and OP_FORWARD
  modes of event crypto adapter.


Removed Items
-------------

* **Removed experimental performance thread example application.**


API Changes
-----------

* net: added experimental functions ``rte_ipv4_udptcp_cksum_mbuf()``,
  ``rte_ipv4_udptcp_cksum_mbuf_verify()``, ``rte_ipv6_udptcp_cksum_mbuf()``,
  ``rte_ipv6_udptcp_cksum_mbuf_verify()``

* ethdev: Old public macros and enumeration constants without ``RTE_ETH_`` prefix,
  which are kept for backward compatibility, are marked as deprecated.

* cryptodev: The asymmetric session handling was modified to use a single
  mempool object. An API ``rte_cryptodev_asym_session_pool_create`` was added
  to create a mempool with element size big enough to hold the generic asymmetric
  session header, max size for a device private session data, and user data size.
  The session structure was moved to ``cryptodev_pmd.h``,
  hiding it from applications.
  The API ``rte_cryptodev_asym_session_init`` was removed as the initialization
  is now moved to ``rte_cryptodev_asym_session_create``, which was updated to
  return an integer value to indicate initialisation errors.


ABI Changes
-----------

* No ABI change that would break compatibility with 21.11.


Known Issues
------------

* **Possible reduced power saving with PMD Power Management.**

  Users may see reduced power savings when using PMD Power Management.
  This issue occurs when compiling DPDK applications with GCC-9
  on platforms with TSX enabled.
  The function ``rte_power_monitor_multi()`` may return
  without successfully starting the RTM transaction (``_xbegin()`` fails).

  There are three workarounds for this issue.
  Either build DPDK with GCC-11 or newer, build with shared libraries,
  or build DPDK with fewer drivers.


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
       * OpenWRT 21.02.1
       * FreeBSD 13.0
       * Red Hat Enterprise Linux Server release 8.4
       * Red Hat Enterprise Linux Server release 8.5
       * Suse 15 SP3
       * Ubuntu 20.04.3
       * Ubuntu 21.10

     * NICs:

       * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

         * Firmware version: 3.22 0x8000d83c 1.3146.0
         * Device id (pf/vf): 8086:1593 / 8086:1889
         * Driver version: 1.8.3_2_g5c2ff303 (ice)
         * OS Default DDP: 1.3.28.0
         * COMMS DDP: 1.3.35.0
         * Wireless Edge DDP: 1.3.8.0

       * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

         * Firmware version: 3.20 0x8000d83e 1.3146.0
         * Device id (pf/vf): 8086:1592 / 8086:1889
         * Driver version: 1.8.3_2_g5c2ff303 (ice)
         * OS Default DDP: 1.3.28.0
         * COMMS DDP: 1.3.35.0
         * Wireless Edge DDP: 1.3.8.0

       * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

         * Firmware version: 0x61bf0001
         * Device id (pf/vf): 8086:10fb / 8086:10ed
         * Driver version(in-tree): 5.1.0-k (ixgbe)
         * Driver version(out-tree): 5.13.4 (ixgbe)

       * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

         * Firmware version(PF): 8.30 0x8000a49d 1.2926.0
         * Firmware version(VF): 8.50 0x8000b6d9 1.3082.0
         * Device id (pf/vf): 8086:1572 / 8086:154c
         * Driver version: 2.17.15 (i40e)

       * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

         * Firmware version: 5.50 0x80003327 1.3082.0
         * Device id (pf/vf): 8086:37d0 / 8086:37cd
         * Driver version(out-tree): 2.17.15 (i40e)
         * Driver version(in-tree): 2.8.20-k (i40e)

       * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T

         * Firmware version: 5.50 0x800032e0 1.2935.0
         * Device id (pf/vf): 8086:37d2 / 8086:37cd
         * Driver version(out-tree): 2.17.15 (i40e)
         * Driver version(in-tree): 2.8.20-k (i40e)

       * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

         * Firmware version(PF): 8.30 0x8000a483 1.2926.0
         * Firmware version(VF): 8.50 0x8000b703 1.3082.0
         * Device id (pf/vf): 8086:158b / 8086:154c
         * Driver version: 2.17.15 (i40e)

       * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

         * Firmware version(PF): 8.30 0x8000a4ae 1.2926.0
         * Firmware version(VF): 8.50 0x8000b6c7 1.3082.0
         * Device id (pf/vf): 8086:1583 / 8086:154c
         * Driver version: 2.17.15 (i40e)

       * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

         * Firmware version: 8.30 0x8000a489 1.2879.0
         * Device id (pf): 8086:15ff
         * Driver version: 2.17.15 (i40e)

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

    * MLNX_OFED 5.5-1.0.3.2 and above
    * MLNX_OFED 5.4-3.1.0.0

  * upstream kernel:

    * Linux 5.17.0-rc4 and above

  * rdma-core:

    * rdma-core-39.0 and above

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
      * Firmware version: 14.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.32.1010 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.32.1010 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.32.2004 and above

  * Embedded software:

    * Ubuntu 20.04.3
    * MLNX_OFED 5.5-2.1.7.0 and above
    * DPDK application running on Arm cores

* IBM Power 9 platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Red Hat Enterprise Linux Server release 8.2

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.1010

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.32.1010

  * OFED:

    * MLNX_OFED 5.5-1.0.3.2
