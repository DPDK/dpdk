..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 19.08
==================

New Features
------------

* **EAL will now pick IOVA as VA mode as the default in most cases.**

  Previously, the preferred default IOVA mode was selected to be IOVA as PA. The
  behavior has now been changed to handle IOVA mode detection in a more complex
  manner, and will default to IOVA as VA in most cases.

* **Added MCS lock.**

  MCS lock provides scalability by spinning on a CPU/thread local variable
  which avoids expensive cache bouncing.
  It provides fairness by maintaining a list of acquirers and passing
  the lock to each CPU/thread in the order they acquired the lock.

* **Updated the EAL Pseudo-random Number Generator.**

  The ``lrand48()`` based ``rte_rand()`` function is replaced with a
  DPDK-native combined Linear Feedback Shift Register (LFSR)
  pseudo-random number generator (PRNG).

  This new PRNG implementation is multi-thread safe, provides
  higher-quality pseudo-random numbers (including full 64 bit
  support) and improved performance.

  In addition, ``<rte_random.h>`` is extended with a new function
  ``rte_rand_max()`` which supplies unbiased, bounded pseudo-random
  numbers.

* **Updated the Broadcom bnxt PMD.**

  Updated the Broadcom bnxt PMD. The major enhancements include:

  * Performance optimizations in non-vector Tx path.
  * Added support for SSE vector mode.
  * Updated HWRM API to version 1.10.0.91.

* **Added support for Broadcom NetXtreme-E BCM57500 Ethernet controllers.**

  Added support to the Broadcom bnxt PMD for the BCM57500 (a.k.a. "Thor") family
  of Ethernet controllers. These controllers support link speeds up to
  200Gbps, 50G PAM-4, and PCIe 4.0.

* **Added Huawei hinic PMD.**

  Added the new ``hinic`` net driver for Huawei Intelligent PCIE Network
  Adapters based on the Huawei Ethernet Controller Hi1822.
  See the :doc:`../nics/hinic` guide for more details on this new driver.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Enabled Tx outer/inner L3/L4 checksum offload.
  * Enabled generic filter framework and supported switch filter.
  * Supported UDP tunnel port add.

* **Updated the Intel i40e driver.**

  Updated tje Intel i40e driver with new features and improvements, including:

  * Added support for MARK + RSS action in rte_flow (non-vector RX path only)

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Updated the packet header modification feature. Added support of TCP header
    sequence number and acknowledgment number modification.
  * Added support for match on ICMP/ICMP6 code and type.
  * Added support for matching on GRE's key and C,K,S present bits.
  * Added support for IP-in-IP tunnel.
  * Accelerated flows with count action creation and destroy.
  * Accelerated flows counter query.
  * Improved Tx datapath performance with enabled HW offloads.
  * Added support for LRO.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added support for Rx interrupts.

* **Added memif PMD.**

  Added a new Shared Memory Packet Interface (``memif``) PMD.
  See the :doc:`../nics/memif` guide for more details on this new driver.

* **Updated the AF_XDP PMD.**

  Updated the AF_XDP PMD. The new features include:

  * Enabled zero copy through mbuf's external memory mechanism to achieve
    higher performance.
  * Added multi-queue support to allow one af_xdp vdev with multiple netdev
    queues.
  * Enabled "need_wakeup" feature which can provide efficient support for the
    use case where the application and driver executing on the same core.

* **Enabled infinite Rx in the PCAP PMD.**

  Added an infinite Rx feature to the PCAP PMD which allows packets in the Rx
  PCAP to be received repeatedly at a high rate. This can be useful for quick
  performance testing of DPDK apps.

* **Enabled receiving no packet in the PCAP PMD.**

  Added function to allow users to run the PCAP PMD without receiving any
  packets on PCAP Rx. When the function is called, a dummy queue is created
  for each Tx queue argument passed.

* **Added a FPGA_LTE_FEC bbdev PMD.**

  Added a new ``fpga_lte_fec`` bbdev driver for the Intel\ |reg| FPGA PAC
  (Programmable  Acceleration Card) N3000.  See the
  :doc:`../bbdevs/fpga_lte_fec` BBDEV guide for more details on this new driver.

* **Updated the TURBO_SW bbdev PMD.**

  Updated the ``turbo_sw`` bbdev driver with changes including:

  * Added option to build the driver with or without dependency of external
    SDK libraries.
  * Added support for 5GNR encode/decode operations.

* **Updated the Intel QuickAssist Technology (QAT) symmetric crypto PMD.**

  Added support for digest-encrypted cases where digest is appended
  to the data.

* **Added the Intel QuickData Technology PMD.**

  Added a PMD for the Intel\ |reg|  QuickData Technology, part of
  Intel\ |reg|  I/O Acceleration Technology `(Intel I/OAT)
  <https://www.intel.com/content/www/us/en/wireless-network/accel-technology.html>`_,
  which allows data copies to be done by hardware instead
  of via software, reducing cycles spent copying large blocks of data in
  applications.

* **Added Marvell OCTEON TX2 drivers.**

  Added the new ``ethdev``, ``eventdev``, ``mempool``, ``eventdev Rx adapter``,
  ``eventdev Tx adapter``, ``eventdev Timer adapter`` and ``rawdev DMA``
  drivers for various HW co-processors available in ``OCTEON TX2`` SoC.

  See :doc:`../platform/octeontx2` and driver information:

  * :doc:`../nics/octeontx2`
  * :doc:`../mempool/octeontx2`
  * :doc:`../eventdevs/octeontx2`
  * ``rawdevs/octeontx2_dma``

* **Introduced the Intel NTB PMD.**

  Added a PMD for Intel NTB (Non-transparent Bridge). This PMD implements
  a handshake between two separate hosts and can share local memory for peer
  host to directly access.

* **Updated the IPSec library and IPsec Security Gateway application.**

  Added the following features to ``librte_ipsec``. Corresponding changes are
  also added in the ``ipsec-secgw`` sample application.

  * ECN and DSCP field header reconstruction as per RFC4301.
  * Transport mode with IPv6 extension headers.
  * Support packets with multiple segments.

* **Updated telemetry library for global metrics support.**

  Updated ``librte_telemetry`` to fetch the global metrics from the
  ``librte_metrics`` library.

* **Added new telemetry mode for l3fwd-power application.**

  Added a telemetry mode to the ``l3fwd-power`` application to report
  application level busyness, empty and full polls of ``rte_eth_rx_burst()``.

* **Updated the pdump application.**

  Add support for pdump to exit with primary process.

* **Updated test-compress-perf tool application.**

  Added a multiple cores feature to the compression perf tool application.


Removed Items
-------------

* Removed KNI ethtool, ``CONFIG_RTE_KNI_KMOD_ETHTOOL``, support.

* build: armv8 crypto extension is disabled.


API Changes
-----------

* The ``rte_mem_config`` structure has been made private. New accessor
  ``rte_mcfg_*`` functions were introduced to provide replacement for direct
  access to the shared mem config.

* The network structures, definitions and functions have
  been prefixed by ``rte_`` to resolve conflicts with libc headers.

* malloc: The function ``rte_malloc_set_limit()`` was never implemented.
  It is deprecated and will be removed in a future release.

* cryptodev: the ``uint8_t *data`` member of the ``key`` structure in the xforms
  structure (``rte_crypto_cipher_xform``, ``rte_crypto_auth_xform``, and
  ``rte_crypto_aead_xform``) have been changed to ``const uint8_t *data``.

* eventdev: No longer marked as experimental.

  The eventdev functions are no longer marked as experimental, and have
  become part of the normal DPDK API and ABI. Any future ABI changes will be
  announced at least one release before the ABI change is made. There are no
  ABI breaking changes planned.

* ip_frag: The IP fragmentation library converts input mbuf into fragments
  using input MTU size via the ``rte_ipv4_fragment_packet()`` interface.
  Once fragmentation is done, each ``mbuf->ol_flags`` are set to enable IP
  checksum H/W offload irrespective of the platform capability.
  Cleared IP checksum H/W offload flag from the library. The application must
  set this flag if it is supported by the platform and application wishes to
  use it.

* ip_frag: IP reassembly library converts the list of fragments into a
  reassembled packet via ``rte_ipv4_frag_reassemble_packet()`` interface.
  Once reassembly is done, ``mbuf->ol_flags`` are set to enable IP checksum H/W
  offload irrespective of the platform capability. Cleared IP checksum H/W
  offload flag from the library. The application must set this flag if it is
  supported by the platform and application wishes to use it.

* sched: Macros ``RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS`` and
  ``RTE_SCHED_PIPE_PROFILES_PER_PORT`` are removed for flexible configuration
  of pipe traffic classes and their queues size, and for runtime configuration
  of the maximum number of pipe profiles, respectively. In addition, the
  ``wrr_weights`` field of struct ``rte_sched_pipe_params`` is modified to be
  used only for best-effort tc, and the ``qsize`` field of struct
  ``rte_sched_port_params`` is changed to allow different sizes for each
  queue.


ABI Changes
-----------

* eventdev: Event based Rx adapter callback

  The mbuf pointer array in the event eth Rx adapter callback
  has been replaced with an event array. Using
  an event array allows the application to change attributes
  of the events enqueued by the SW adapter.

  The callback can drop packets and populate
  a callback argument with the number of dropped packets.
  Add a Rx adapter stats field to keep track of the total
  number of dropped packets.

* cryptodev: New member in ``rte_cryptodev_config`` to allow applications to
  disable features supported by the crypto device. Only the following features
  would be allowed to be disabled this way,

  - ``RTE_CRYPTODEV_FF_SYMMETRIC_CRYPTO``.
  - ``RTE_CRYPTODEV_FF_ASYMMETRIC_CRYPTO``.
  - ``RTE_CRYPTODEV_FF_SECURITY``.

  Disabling unused features would facilitate efficient usage of HW/SW offload.

* bbdev: New operations and parameters have been added to support new 5GNR
  operations. The bbdev ABI is still kept experimental.

* rawdev: The driver names have been changed to ``librte_rawdev_*``.
  Now they all have the same prefix, and same name with make and meson builds.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bpf.so.1
     librte_bus_dpaa.so.2
     librte_bus_fslmc.so.2
     librte_bus_ifpga.so.2
     librte_bus_pci.so.2
     librte_bus_vdev.so.2
     librte_bus_vmbus.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_compressdev.so.1
   + librte_cryptodev.so.8
     librte_distributor.so.1
   + librte_eal.so.11
     librte_efd.so.1
     librte_ethdev.so.12
   + librte_eventdev.so.7
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ipsec.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.5
     librte_member.so.1
     librte_mempool.so.5
     librte_meter.so.3
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.3
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
     librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_rcu.so.1
     librte_reorder.so.1
     librte_ring.so.2
   + librte_sched.so.3
     librte_security.so.2
     librte_stack.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

* **Unsuitable IOVA mode may be picked as the default.**

  Not all kernel drivers and not all devices support all IOVA modes. EAL will
  attempt to pick a reasonable default based on a number of factors, but
  there may be cases where the default is unsuitable.

  It is recommended to use the `--iova-mode` command-line parameter if the
  default is not suitable.


Tested Platforms
----------------

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C3758 @ 2.20GHz
     * Intel(R) Xeon(R) CPU D-1541 @ 2.10GHz
     * Intel(R) Xeon(R) CPU D-1553N @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2699 v4 @ 2.20GHz
     * Intel(R) Xeon(R) Gold 6139 CPU @ 2.30GHz
     * Intel(R) Xeon(R) Platinum 8180 CPU @ 2.50GHz
     * Intel(R) Xeon(R) Platinum 8280M CPU @ 2.70GHz

   * OS:

     * CentOS 7.6
     * Fedora 30
     * FreeBSD 12.0
     * Red Hat Enterprise Linux Server release 8.0
     * Red Hat Enterprise Linux Server release 7.6
     * Suse12SP3
     * Ubuntu 16.04
     * Ubuntu 16.10
     * Ubuntu 18.04
     * Ubuntu 19.04

   * NICs:

     * Intel(R) 82599ES 10 Gigabit Ethernet Controller

       * Firmware version: 0x61bf0001
       * Device id (pf/vf): 8086:10fb / 8086:10ed
       * Driver version: 5.6.1 (ixgbe)

     * Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

       * Firmware version: 0x800003e7
       * Device id (pf/vf): 8086:15ad / 8086:15a8
       * Driver version: 5.1.0 (ixgbe)

     * Intel Corporation Ethernet Controller 10G X550T

       * Firmware version: 0x80000482
       * Device id (pf): 8086:1563
       * Driver version: 5.6.1 (ixgbe)

     * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

       * Firmware version: 7.00 0x80004cdb
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 2.9.21 (i40e)

     * Intel(R) Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

       * Firmware version: 4.10 0x80001a3c
       * Device id (pf/vf): 8086:37d0 / 8086:37cd
       * Driver version: 2.9.21 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

       * Firmware version: 7.00 0x80004cf8
       * Device id (pf/vf): 8086:158b / 8086:154c
       * Driver version: 2.9.21 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 7.00 0x80004c97
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 2.9.21 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.63, 0x80000cbc
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.4.0-k (igb)

     * Intel Corporation I210 Gigabit Network Connection

       * Firmware version: 3.25, 0x800006eb
       * Device id (pf): 8086:1533
       * Driver version: 5.4.0-k(igb)

* Intel(R) platforms with Mellanox(R) NICs combinations

  * CPU:

    * Intel(R) Xeon(R) Gold 6154 CPU @ 3.00GHz
    * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
    * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
    * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
    * Intel(R) Xeon(R) CPU E5-2650 v4 @ 2.20GHz
    * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz
    * Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Red Hat Enterprise Linux Server release 7.3 (Maipo)
    * Red Hat Enterprise Linux Server release 7.2 (Maipo)
    * Ubuntu 19.04
    * Ubuntu 18.10
    * Ubuntu 18.04
    * Ubuntu 16.04
    * SUSE Linux Enterprise Server 15

  * OFED:

    * MLNX_OFED 4.6-1.0.1.1
    * MLNX_OFED 4.6-4.1.2.0

  * NICs:

    * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.25.6406 and above

    * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.25.6406 and above

    * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.25.6406 and above

    * Mellanox(R) ConnectX(R)-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.25.6406 and above

* Mellanox(R) BlueField SmartNIC

  * Mellanox(R) BlueField SmartNIC MT416842 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 18.25.6600

  * SoC Arm cores running OS:

    * CentOS Linux release 7.5.1804 (AltArch)
    * MLNX_OFED 4.6-3.5.8.0

  * DPDK application running on Arm cores inside SmartNIC

* IBM Power 9 platforms with Mellanox(R) NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Ubuntu 18.04.1 LTS (Bionic Beaver)

  * NICs:

    * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.25.6406

  * OFED:

    * MLNX_OFED 4.6-4.1.2.0

* ARMv8 SoC combinations from Marvell (with integrated NICs)

   * SoC:

     * CN83xx, CN96xx, CNF95xx, CN93xx

   * OS (Based on Marvell OCTEON TX SDK 10.0):

     * Arch Linux
     * Buildroot 2018.11
     * Ubuntu 16.04.1 LTS
     * Ubuntu 16.10
     * Ubuntu 18.04.1
     * Ubuntu 19.04
