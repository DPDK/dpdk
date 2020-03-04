..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 19.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_19_11.html


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
     * Device abstraction libs and PMDs
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added support for --base-virtaddr EAL option to FreeBSD.**

  The FreeBSD version of DPDK now also supports setting base virtual address
  for mapping pages and resources into its address space.

* **Added Lock-free Stack for aarch64.**

  Enabled the lock-free stack implementation for aarch64 platforms.

* **Extended pktmbuf mempool private structure.**

  rte_pktmbuf_pool_private structure was extended to include flags field
  for future compatibility.
  As per 19.11 release this field is reserved and should be set to 0
  by the user.

+* **Changed mempool allocation behavior.**

  Changed the mempool allocation behaviour so that objects no longer cross
  pages by default. Note, this may consume more memory when using small memory
  pages.

* **Added support for dynamic fields and flags in mbuf.**

  This new feature adds the ability to dynamically register some room
  for a field or a flag in the mbuf structure. This is typically used
  for specific offload features, where adding a static field or flag
  in the mbuf is not justified.

* **Added support for hairpin queues.**

  On supported NICs, we can now setup hairpin queues which will offload packets
  from the wire, back to the wire.

* **Added flow tag in rte_flow.**

  The ``SET_TAG`` action and ``TAG`` item have been added to support transient
  flow tag.

* **Extended metadata support in rte_flow.**

  Flow metadata has been extended to both Rx and Tx.

  * Tx metadata can also be set by SET_META action of rte_flow.
  * Rx metadata is delivered to the host via a dynamic field of ``rte_mbuf``
    with ``PKT_RX_DYNF_METADATA``.

* **Added ethdev API to set supported packet types.**

  * Added new API ``rte_eth_dev_set_ptypes`` which allows an application to
    inform a PMD about a reduced range of packet types to handle.
  * This scheme will allow PMDs to avoid lookup of internal ptype table on Rx
    and thereby improve Rx performance if the application wishes to do so.

* **Added Rx offload flag to enable or disable RSS update.**

  * Added new Rx offload flag ``DEV_RX_OFFLOAD_RSS_HASH`` which can be used to
    enable/disable PMDs write to ``rte_mbuf::hash::rss``.
  * PMDs notify the validity of ``rte_mbuf::hash:rss`` to the application
    by enabling ``PKT_RX_RSS_HASH`` flag in ``rte_mbuf::ol_flags``.

* **Added Rx/Tx packet burst mode "get" API.**

  Added two new functions ``rte_eth_rx_burst_mode_get`` and
  ``rte_eth_tx_burst_mode_get`` that allow an application
  to retrieve the mode information about Rx/Tx packet burst
  such as Scalar or Vector, and Vector technology like AVX2.

* **Added Hisilicon hns3 PMD.**

  Added the new ``hns3`` net driver for the inbuilt Hisilicon Network
  Subsystem 3 (HNS3) network engine found in the Hisilicon Kunpeng 920 SoC.
  See the :doc:`../nics/hns3` guide for more details on this new driver.

* **Added NXP PFE PMD.**

  Added the new PFE driver for the NXP LS1012A platform. See the
  :doc:`../nics/pfe` NIC driver guide for more details on this new driver.

* **Updated Broadcom bnxt driver.**

  Updated Broadcom bnxt driver with new features and improvements, including:

  * Added support for hot firmware upgrade.
  * Added support for error recovery.
  * Added support for querying and using COS classification in hardware.
  * Added LRO support Thor devices.
  * Update HWRM API to version 1.10.1.6

* **Updated the enic driver.**

  * Added support for Geneve with options offload.
  * Added flow API implementation based on VIC Flow Manager API.

* **Updated iavf PMD.**

  Enable AVX2 data path for iavf PMD.

* **Updated the Intel e1000 driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel ixgbe driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel i40e driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel fm10k driver.**

  Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for device-specific DDP package loading.
  * Added support for handling Receive Flex Descriptor.
  * Added support for protocol extraction on per Rx queue.
  * Added support for Flow Director filter based on generic filter framework.
  * Added support for the ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.
  * Generic filter enhancement
    - Supported pipeline mode.
    - Supported new packet type like PPPoE for switch filter.
  * Supported input set change and symmetric hash by rte_flow RSS action.
  * Added support for GTP Tx checksum offload.
  * Added new device IDs to support E810_XXV devices.

* **Updated the Huawei hinic driver.**

  Updated the Huawei hinic driver with new features and improvements, including:

  * Enabled SR-IOV - Partially supported at this point, VFIO only.
  * Supported VLAN filter and VLAN offload.
  * Supported Unicast MAC filter and Multicast MAC filter.
  * Supported Flow API for LACP, VRRP, BGP and so on.
  * Supported FW version get.

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added support for VLAN pop flow offload command.
  * Added support for VLAN push flow offload command.
  * Added support for VLAN set PCP offload command.
  * Added support for VLAN set VID offload command.
  * Added support for matching on packets withe Geneve tunnel header.
  * Added hairpin support.
  * Added ConnectX-6 Dx support.
  * Flow engine selected based on RDMA Core library version.
    DV flow engine selected if version is rdma-core-24.0 or higher.
    Verbs flow engine selected otherwise.

* **Updated the AF_XDP PMD.**

  Updated the AF_XDP PMD. The new features include:

  * Enabled zero copy between application mempools and UMEM by enabling the
    ``XDP_UMEM_UNALIGNED_CHUNKS UMEM`` flag.

* **Added cryptodev asymmetric session-less operation.**

  Added a session-less option to the cryptodev asymmetric structure. It works
  the same way as symmetric crypto, and the corresponding transform is used
  directly by the crypto operation.

* **Added Marvell NITROX symmetric crypto PMD.**

  Added a symmetric crypto PMD for Marvell NITROX V security processor.
  See the :doc:`../cryptodevs/nitrox` guide for more details on this new PMD.

* **Added asymmetric support to Marvell OCTEON TX crypto PMD.**

  Added support for asymmetric operations to Marvell OCTEON TX crypto PMD.
  Supports RSA and modexp operations.

* **Added Marvell OCTEON TX2 crypto PMD.**

  Added a new PMD driver for hardware crypto offload block on ``OCTEON TX2``
  SoC.

  See :doc:`../cryptodevs/octeontx2` for more details

* **Updated NXP crypto PMDs for PDCP support.**

  Added PDCP support to the DPAA_SEC and DPAA2_SEC PMDs using rte_security
  APIs.  Support has been added for all sequence number sizes for control and
  user plane.  Test and test-crypto-perf applications have been updated for
  unit testing.

* **Updated the AESNI-MB PMD.**

  * Added support for intel-ipsec-mb version 0.53.

* **Updated the AESNI-GCM PMD.**

  * Added support for intel-ipsec-mb version 0.53.
  * Added support for in-place chained mbufs with AES-GCM algorithm.

* **Enabled Single Pass GCM acceleration on QAT GEN3.**

  Added support for Single Pass GCM, available on QAT GEN3 only (Intel
  QuickAssist Technology P5xxx). It is automatically chosen instead of the
  classic 2-pass mode when running on QAT GEN3, significantly improving
  the performance of AES GCM operations.

* **Updated the Intel QuickAssist Technology (QAT) asymmetric crypto PMD.**

  * Added support for asymmetric session-less operations.
  * Added support for RSA algorithm with pair ``(n, d)`` private key
    representation.
  * Added support for RSA algorithm with quintuple private key representation.

* **Updated the Intel QuickAssist Technology (QAT) compression PMD.**

  Added stateful decompression support in the Intel QuickAssist Technology PMD.
  Please note that stateful compression is not supported.

* **Added external buffers support for dpdk-test-compress-perf tool.**

  Added a command line option to the ``dpdk-test-compress-perf`` tool to
  allocate and use memory zones as external buffers instead of keeping the
  data directly in mbuf areas.

* **Updated the IPSec library.**

  * Added Security Associations (SA) Database API to ``librte_ipsec``. A new
    test-sad application has also been introduced to evaluate and perform
    custom functional and performance tests for an IPsec SAD implementation.

  * Support fragmented packets in inline crypto processing mode with fallback
    ``lookaside-none`` session. Corresponding changes are also added in the
    IPsec Security Gateway application.

* **Introduced FIFO for NTB PMD.**

  Introduced FIFO for NTB (Non-transparent Bridge) PMD to support
  packet based processing.

* **Added eBPF JIT support for arm64.**

  Added eBPF JIT support for arm64 architecture to improve the eBPF program
  performance.

* **Added RIB and FIB (Routing/Forwarding Information Base) libraries.**

  Added Routing and Forwarding Information Base (RIB/FIB) libraries. RIB and
  FIB can replace the LPM (Longest Prefix Match) library with better control
  plane (RIB) performance. The data plane (FIB) can be extended with new
  algorithms.

* **Updated testpmd with a command for ptypes.**

  * Added a console command to testpmd app, ``show port (port_id) ptypes`` which
    gives ability to print port supported ptypes in different protocol layers.
  * Packet type detection disabled by default for the supported PMDs.

* **Added new l2fwd-event sample application.**

  Added an example application ``l2fwd-event`` that adds event device support to
  the traditional l2fwd example. It demonstrates usage of poll and event mode IO
  mechanism under a single application.

* **Added build support for Link Time Optimization.**

  LTO is an optimization technique used by the compiler to perform whole
  program analysis and optimization at link time.  In order to do that
  compilers store their internal representation of the source code that
  the linker uses at the final stage of the compilation process.

  See :doc:`../prog_guide/lto` for more information:

* **Added IOVA as VA support for KNI.**

  * Added IOVA = VA support for KNI. KNI can operate in IOVA = VA mode when
    ``iova-mode=va`` EAL option is passed to the application or when bus IOVA
    scheme is selected as RTE_IOVA_VA. This mode only works on Linux Kernel
    versions 4.10.0 and above.

  * Due to IOVA to KVA address translations, based on the KNI use case there
    can be a performance impact. For mitigation, forcing IOVA to PA via EAL
    ``--iova-mode=pa`` option can be used, IOVA_DC bus iommu scheme can also
    result in IOVA as PA.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Removed library-level ABI versions. These have been replaced with a single
  project-level ABI version for non-experimental libraries and an ABI version of
  ``0`` for experimental libraries. Review the :doc:`../contributing/abi_policy`
  and :doc:`../contributing/abi_versioning` guides for more information.

* Removed duplicated set of commands for Rx offload configuration from testpmd::

    port config all crc-strip|scatter|rx-cksum|rx-timestamp|
                    hw-vlan|hw-vlan-filter|hw-vlan-strip|hw-vlan-extend on|off

  The testpmd command set that can be used instead in order to enable or
  disable Rx offloading on all Rx queues of a port is::

    port config <port_id> rx_offload crc_strip|scatter|
                                     ipv4_cksum|udp_cksum|tcp_cksum|timestamp|
                                     vlan_strip|vlan_filter|vlan_extend on|off

* Removed AF_XDP pmd_zero copy vdev argument. Support is now auto-detected.

* The following sample applications have been removed in this release:

  * Exception Path
  * L3 Forwarding in a Virtualization Environment
  * Load Balancer
  * Netmap Compatibility
  * Quota and Watermark
  * vhost-scsi

* Removed arm64-dpaa2-* build config. arm64-dpaa-* can now build for both
  dpaa and dpaa2 platforms.


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
   =========================================================

* eal: made the ``lcore_config`` struct and global symbol private.

* eal: removed the ``rte_cpu_check_supported`` function, replaced by
  ``rte_cpu_is_supported`` since dpdk v17.08.

* eal: removed the ``rte_malloc_virt2phy`` function, replaced by
  ``rte_malloc_virt2iova`` since v17.11.

* eal: made the ``rte_config`` struct and ``rte_eal_get_configuration``
  function private.

* mem: hid the internal ``malloc_heap`` structure and the
  ``rte_malloc_heap.h`` header.

* vfio: removed ``rte_vfio_dma_map`` and ``rte_vfio_dma_unmap`` that have
  been marked as deprecated in release 19.05.
  ``rte_vfio_container_dma_map`` and ``rte_vfio_container_dma_unmap`` can
  be used as substitutes.

* pci: removed the following functions deprecated since dpdk v17.11:

  - ``eal_parse_pci_BDF`` replaced by ``rte_pci_addr_parse``
  - ``eal_parse_pci_DomBDF`` replaced by ``rte_pci_addr_parse``
  - ``rte_eal_compare_pci_addr`` replaced by ``rte_pci_addr_cmp``

* The network structure ``esp_tail`` has been prefixed by ``rte_``.

* The network definitions of PPPoE ethertypes have been prefixed by ``RTE_``.

* The network structure for MPLS has been prefixed by ``rte_``.

* ethdev: changed ``rte_eth_dev_infos_get`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_promiscuous_enable`` and
  ``rte_eth_promiscuous_disable`` return value from ``void`` to ``int`` to
  provide a way to report various error conditions.

* ethdev: changed ``rte_eth_allmulticast_enable`` and
  ``rte_eth_allmulticast_disable`` return value from ``void`` to ``int`` to
  provide a way to report various error conditions.

* ethdev: changed ``rte_eth_dev_xstats_reset`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_link_get`` and ``rte_eth_link_get_nowait``
  return value from ``void`` to ``int`` to provide a way to report various
  error conditions.

* ethdev: changed ``rte_eth_macaddr_get`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: changed ``rte_eth_dev_owner_delete`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: The deprecated function ``rte_eth_dev_count`` was removed.
  The function ``rte_eth_dev_count_avail`` is a drop-in replacement.
  If the intent is to iterate over ports, ``RTE_ETH_FOREACH_*`` macros
  are better port iterators.

* ethdev: ``RTE_FLOW_ITEM_TYPE_META`` data endianness altered to host one.
  Due to the new dynamic metadata field in mbuf is host-endian either, there
  is a minor compatibility issue for applications in case of 32-bit values
  supported.

* ethdev: the tx_metadata mbuf field is moved to dynamic one.
  ``PKT_TX_METADATA`` flag is replaced with ``PKT_TX_DYNF_METADATA``.
  ``DEV_TX_OFFLOAD_MATCH_METADATA`` offload flag is removed, now metadata
  support in PMD is engaged on dynamic field registration.

* event: The function ``rte_event_eth_tx_adapter_enqueue`` takes an additional
  input as ``flags``. Flag ``RTE_EVENT_ETH_TX_ADAPTER_ENQUEUE_SAME_DEST`` which
  has been introduced in this release is used when all the packets enqueued in
  the Tx adapter are destined for the same Ethernet port and Tx queue.

* sched: The pipe nodes configuration parameters such as number of pipes,
  pipe queue sizes, pipe profiles, etc., are moved from port level structure
  to subport level. This allows different subports of the same port to
  have different configuration for the pipe nodes.


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
   =========================================================

* policy: Please note the revisions to the :doc:`../contributing/abi_policy`
  introducing major ABI versions, with DPDK 19.11 becoming the first major
  version ``v20``. ABI changes to add new features continue to be permitted in
  subsequent releases, with the condition that ABI compatibility with the major
  ABI version is maintained.

* net: The Ethernet address and other header definitions have changed
  attributes. They have been modified to be aligned on 2-byte boundaries.
  These changes should not impact normal usage because drivers naturally
  align the Ethernet header on receive and all known encapsulations
  preserve the alignment of the header.

* security: The field ``replay_win_sz`` has been moved from the ipsec library
  based ``rte_ipsec_sa_prm`` structure to security library based structure
  ``rte_security_ipsec_xform``, which specify the anti-replay window size
  to enable sequence replay attack handling.

* ipsec: The field ``replay_win_sz`` has been removed from the structure
  ``rte_ipsec_sa_prm`` as it has been added to the security library.

* ethdev: Added 32-bit fields for maximum LRO aggregated packet size, in
  struct ``rte_eth_dev_info`` for the port capability and in struct
  ``rte_eth_rxmode`` for the port configuration.
  Application should use the new field in struct ``rte_eth_rxmode`` to configure
  the requested size.
  PMD should use the new field in struct ``rte_eth_dev_info`` to report the
  supported port capability.


Shared Library Versions
-----------------------

.. Update any library version updated in this release
   and prepend with a ``+`` sign, like this:

     libfoo.so.1
   + libupdated.so.2
     libbar.so.1

   This section is a comment. Do not overwrite or remove it.
   =========================================================

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
     librte_cryptodev.so.8
     librte_distributor.so.1
   + librte_eal.so.12
     librte_efd.so.1
   + librte_ethdev.so.13
   + librte_eventdev.so.8
   + librte_fib.so.1
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
   + librte_ipsec.so.2
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
   + librte_pci.so.2
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
   + librte_rib.so.1
     librte_rcu.so.1
     librte_reorder.so.1
     librte_ring.so.2
   + librte_sched.so.4
   + librte_security.so.3
     librte_stack.so.1
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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
   =========================================================

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
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 7.6
    * Fedora 30
    * FreeBSD 12.0
    * Red Hat Enterprise Linux Server release 8.0
    * Red Hat Enterprise Linux Server release 7.6
    * Suse12SP3
    * Ubuntu 14.04
    * Ubuntu 16.04
    * Ubuntu 16.10
    * Ubuntu 18.04
    * Ubuntu 19.04

  * NICs:

    * Intel\ |reg| Corporation Ethernet Controller E810-C for SFP (2x25G)

      * Firmware version: 1.02 0x80002084 1.2538.0/1.02 0x80002082 1.2538.0
      * Device id (pf): 8086:1593
      * Driver version: 0.12.25 (ice)

    * Intel\ |reg| Corporation Ethernet Controller E810-C for SFP (2x100G)

      * Firmware version: 1.02 0x80002081 1.2538.0
      * Device id (pf): 8086:1592
      * Driver version: 0.12.25 (ice)

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.6.1 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.6.1 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 7.00 0x80004cdb
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.9.21 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 4.10 0x80001a3c
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.9.21 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 7.00 0x80004cf8
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.9.21 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 7.00 0x80004c97
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.9.21 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000cbc
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.4.0-k(igb)

* ARMv8 SoC combinations from Marvell (with integrated NICs)

  * SoC:

    * CN83xx, CN96xx, CN93xx

  * OS (Based on Marvell OCTEON TX SDK-10.1.2.0):

    * Arch Linux
    * Buildroot 2018.11
    * Ubuntu 16.04.1 LTS
    * Ubuntu 16.10
    * Ubuntu 18.04.1
    * Ubuntu 19.04

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux Server release 8.0 (Maipo)
    * Red Hat Enterprise Linux Server release 7.7 (Maipo)
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
    * MLNX_OFED 4.7-1.0.0.1
    * MLNX_OFED 4.7-3.1.9.0 and above

  * upstream kernel:

    * Linux 5.3 and above

  * rdma-core:

    * rdma-core-24.1-1 and above

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-4 10G MCX4111A-XCAT (1x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 25G MCX4111A-ACAT (1x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 40G MCX415A-BCAT (1x40G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 50G MCX414A-BCAT (2x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 50G MCX415A-CCAT (1x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 100G MCX416A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1013
      * Firmware version: 12.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 10G MCX4121A-XCAT (2x10G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.26.2032 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.26.2032 and above

* IBM Power 9 platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Ubuntu 18.04.1 LTS (Bionic Beaver)

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.26.1040

  * OFED:

    * MLNX_OFED 4.7-1.0.0.2
