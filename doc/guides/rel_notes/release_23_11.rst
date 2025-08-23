.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 23.11
==================

New Features
------------

* **Build requirements increased for C11.**

  From DPDK 23.11 onwards,
  building DPDK will require a C compiler which supports the C11 standard,
  including support for C11 standard atomics.

  More specifically, the requirements will be:

  * Support for flag ``-std=c11`` (or similar)
  * ``__STDC_NO_ATOMICS__`` is *not defined* when using c11 flag

  Please note:

  * C11, including standard atomics, is supported from GCC version 5 onwards,
    and is the default language version in that release
    (Ref: https://gcc.gnu.org/gcc-5/changes.html)
  * C11 is the default compilation mode in Clang from version 3.6,
    which also added support for standard atomics
    (Ref: https://releases.llvm.org/3.6.0/tools/clang/docs/ReleaseNotes.html)

* **Updated dependencies when linking against libarchive.**

  When the libarchive development package is present on the system,
  DPDK will use libarchive and register a dependency on it.
  However, on a number of Linux distributions, including, for example, Fedora and Ubuntu,
  installing the libarchive dev package does not cause all required dependencies for static linking to be automatically installed too.
  These additional dev packages, such as ``liblz4-dev`` and ``libacl1-dev`` on Ubuntu,
  will need to be installed manually (if not already present)
  to prevent errors with linking against DPDK static libraries.

* **Added new build options.**

  * Enabling deprecated libraries is now done using
    the new ``enable_deprecated_libraries`` build option.
  * Optional libraries can now be selected with the new ``enable_libs``
    build option similarly to the existing ``enable_drivers`` build option.

* **Introduced a new API for atomic operations.**

  This new API serves as a wrapper for transitioning
  to standard atomic operations as described in the C11 standard.
  This API implementation points at the compiler intrinsics by default.
  The implementation using C11 standard atomic operations is enabled
  via the ``enable_stdatomic`` build option.

* **Added support for power intrinsics with AMD processors.**

  Added AMD ``MONITORX``/``MWAITX`` instructions in EAL for power optimisation.

* **Added support for allow/block list in vmbus bus driver.***

  The ``vmbus`` bus driver now supports ``-a`` and ``-b`` EAL options
  for selecting devices.

* **Added mbuf recycling support.**

  Added ``rte_eth_recycle_rx_queue_info_get`` and ``rte_eth_recycle_mbufs``
  functions which allow the user to copy used mbufs from the Tx mbuf ring
  into the Rx mbuf ring. This feature supports the case that the Rx Ethernet
  device is different from the Tx Ethernet device with respective driver
  callback functions in ``rte_eth_recycle_mbufs``.

* **Added amd-pstate driver support to the power management library.**

  Added support for amd-pstate driver which works on AMD EPYC processors.

* **Added maximum Rx buffer size to report.**

  Introduced the ``max_rx_bufsize`` field, representing
  the maximum Rx buffer size per descriptor supported by the HW,
  in the structure ``rte_eth_dev_info`` to avoid wasting mempool space.

* **Improved support of RSS hash algorithm.**

  * Added support to query RSS hash algorithm capability via ``rte_eth_dev_info_get()``,
    and set RSS hash algorithm via ``rte_eth_dev_configure()``
    or ``rte_eth_dev_rss_hash_update()``.

  * Added new function ``rte_eth_dev_rss_algo_name``
    to get name of RSS hash algorithm.

* **Added packet type flow matching criteria.**

  Added ``RTE_FLOW_ITEM_TYPE_PTYPE`` to allow matching on L2/L3/L4
  and tunnel information as defined in mbuf packet type.

* **Added a flow action type for P4-defined actions.**

  For P4-programmable devices, hardware pipeline can be configured through
  a new "PROG" action type and its associated custom arguments.
  Such P4 pipelines, not using the standard blocks of the flow API,
  can be managed with ``RTE_FLOW_ITEM_TYPE_FLEX`` and ``RTE_FLOW_ACTION_TYPE_PROG``.

* **Added flow group set miss actions.**

  Introduced ``rte_flow_group_set_miss_actions()`` API to explicitly set
  a group's miss actions, which are the actions to be performed on packets
  that didn't match any of the flow rules in the group.

* **Updated Amazon ena (Elastic Network Adapter) net driver.**

  * Upgraded ENA HAL to latest version.
  * Added support for connection tracking allowance utilization metrics.
  * Added support for reporting Rx overrun errors in xstats.
  * Added support for ENA-express metrics.

* **Added a new vDPA PMD for Corigine NFP devices.**

  Added a new Corigine NFP vDPA (``nfp_vdpa``) PMD.
  See the :doc:`../vdpadevs/nfp` guide for more details on this driver.

* **Updated Corigine/Netronome nfp driver.**

  * Added inline IPsec offload based on the security framework.

* **Updated Intel cpfl driver.**

  * Added support for port representor.
  * Added support for flow offload (including P4-defined pipeline).

* **Updated Intel iavf driver.**

  * Added support for iavf auto-reset.

* **Updated Intel i40e driver.**

  * Added support for new X722 devices.

* **Updated Marvell cnxk net driver.**

  * Added support for ``RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT`` flow item.
  * Added support for ``RTE_FLOW_ACTION_TYPE_AGE`` flow action.

* **Updated NVIDIA mlx5 net driver.**

  * Added support for multi-port E-Switch.
  * Added support for Network Service Header (NSH) flow matching.
  * Added support for ``RTE_FLOW_ITEM_TYPE_PTYPE`` flow item.
  * Added support for ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH`` flow action.
  * Added support for ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE`` flow action.
  * Added support for ``RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR`` flow action and mirror.
  * Added support for ``RTE_FLOW_ACTION_TYPE_INDIRECT_LIST`` flow action.

* **Updated Solarflare net driver.**

  * Added support for transfer flow action ``INDIRECT`` with subtype ``VXLAN_ENCAP``.
  * Supported packet replay (multi-count / multi-delivery) in transfer flows.

* **Updated Wangxun ngbe driver.**

  * Added 100M and auto-neg support in YT PHY fiber mode.

* **Added support for TLS and DTLS record processing.**

  Added TLS and DTLS record transform for security session
  and added enhancements to ``rte_crypto_op`` fields
  to handle all datapath requirements of TLS and DTLS.
  The support was added for TLS 1.2, TLS 1.3 and DTLS 1.2.

* **Added out of place processing support for inline ingress security session.**

  Similar to out of place processing support for lookaside security session,
  added the same support for inline ingress security session.

* **Added security Rx inject API.**

  Added Rx inject API to allow applications to submit packets
  for protocol offload and have them injected back to ethdev Rx
  so that further ethdev Rx actions (IP reassembly, packet parsing and flow lookups)
  can happen based on the inner packet.

  When using the API implemented by an ethdev the application would be able to process
  packets that are received without/failed inline offload processing
  (such as fragmented ESP packets with inline IPsec offload).
  When using the API implemented by a cryptodev it can be used for injecting packets
  to ethdev Rx after IPsec processing and take advantage of ethdev Rx actions
  for the inner packet which cannot be accelerated in inline protocol offload mode.

* **Updated cryptodev scheduler driver.**

  * Added support for DOCSIS security protocol
    through the ``rte_security`` API callbacks.

* **Updated ipsec_mb crypto driver.**

  * Added Intel IPsec MB v1.5 library support for x86 platform.
  * Added support for digest encrypted to the AESNI_MB asynchronous crypto driver.

* **Updated Intel QuickAssist Technology driver.**

  * Enabled support for QAT 2.0c (4944) devices in QAT crypto driver.
  * Added support for SM2 ECDSA algorithm.

* **Updated Marvell cnxk crypto driver.**

  * Added SM2 algorithm support in asymmetric crypto operations.
  * Added asymmetric crypto ECDH support.

* **Updated Marvell Nitrox symmetric crypto driver.**

  * Added support for AES-CCM algorithm.

* **Updated Intel vRAN Boost baseband driver.**

  * Added support for the new Intel vRAN Boost v2 device variant (GNR-D)
    within the unified driver.

* **Added support for models with multiple I/O in mldev library.**

  Added support in mldev library for models with multiple inputs and outputs.

* **Updated Marvell cnxk mldev driver.**

  * Added support for models compiled using TVM framework.

* **Added new eventdev Ethernet Rx adapter create API.**

  Added new function ``rte_event_eth_rx_adapter_create_ext_with_params()``
  for creating Rx adapter instance for the applications desire to
  control both the event port allocation and event buffer size.

* **Added eventdev DMA adapter.**

  * Added the Event DMA Adapter Library. This library extends the event-based
    model by introducing APIs that allow applications to enqueue/dequeue DMA
    operations to/from dmadev as events scheduled by an event device.

* **Added eventdev support to link queues to port with link profile.**

  Introduced event link profiles that can be used to associate links between
  event queues and an event port with a unique identifier termed the "link profile".
  The profile can be used to switch between the associated links in fast-path
  without the additional overhead of linking/unlinking and waiting for unlinking.

  * Added ``rte_event_port_profile_links_set``, ``rte_event_port_profile_unlink``
    ``rte_event_port_profile_links_get`` and ``rte_event_port_profile_switch``
    functions to enable this feature.

* **Updated Marvell cnxk eventdev driver.**

  * Added support for ``remaining_ticks_get`` timer adapter PMD callback
    to get the remaining ticks to expire for a given event timer.
  * Added link profiles support, up to two link profiles are supported.

* **Updated Marvell cnxk dmadev driver.**

  * Added support for source buffer auto free for memory to device DMA.

* **Added dispatcher library.**

  Added dispatcher library which purpose is to help decouple different
  parts (modules) of an eventdev-based application.

* **Added CLI based graph application.**

  Added CLI based graph application which exercises different use cases.
  The application provides a framework so that each use case can be added via a file.
  Each CLI will further be translated into a graph representing the use case.

* **Added layer 2 MACsec forwarding example application.**

  Added a new example layer 2 forwarding application to benchmark
  MACsec encryption/decryption using rte_security based inline sessions.


Removed Items
-------------

* eal: Removed deprecated ``RTE_FUNC_PTR_OR_*`` macros.

* ethdev: Removed deprecated macro ``RTE_ETH_DEV_BONDED_SLAVE``.

* flow_classify: Removed flow classification library and examples.

* kni: Removed the Kernel Network Interface (KNI) library and driver.

* cryptodev: Removed the arrays of algorithm strings ``rte_crypto_cipher_algorithm_strings``,
  ``rte_crypto_auth_algorithm_strings``, ``rte_crypto_aead_algorithm_strings`` and
  ``rte_crypto_asym_xform_strings``.

* cryptodev: Removed explicit SM2 xform parameter in asymmetric xform.

* security: Removed deprecated field ``reserved_opts``
  from struct ``rte_security_ipsec_sa_options``.

* mldev: Removed functions ``rte_ml_io_input_size_get`` and ``rte_ml_io_output_size_get``.

* cmdline: Removed broken and unused function ``cmdline_poll``.


API Changes
-----------

* eal: The thread API has changed.
  The function ``rte_thread_create_control()`` does not take attributes anymore.
  The whole thread API was promoted to stable level,
  except ``rte_thread_setname()`` and ``rte_ctrl_thread_create()`` which are
  replaced with ``rte_thread_set_name()`` and ``rte_thread_create_control()``.

* eal: Removed ``RTE_CPUFLAG_NUMFLAGS`` to avoid misusage and theoretical ABI
  compatibility issue when adding new cpuflags.

* power: Updated the x86 Uncore power management API so that it is vendor agnostic.

* ethdev: When ``rte_eth_dev_configure`` or ``rte_eth_dev_rss_hash_update`` are called,
  the ``rss_key_len`` of structure ``rte_eth_rss_conf`` should be provided
  by the user for the case ``rss_key != NULL``,
  it won't be taken as default 40 bytes anymore.

* bonding: Replaced master/slave to main/member. The data structure
  ``struct rte_eth_bond_8023ad_slave_info`` was renamed to
  ``struct rte_eth_bond_8023ad_member_info`` in DPDK 23.11.
  The following functions were removed in DPDK 23.11.
  The old functions:
  ``rte_eth_bond_8023ad_slave_info``,
  ``rte_eth_bond_active_slaves_get``,
  ``rte_eth_bond_slave_add``,
  ``rte_eth_bond_slave_remove``, and
  ``rte_eth_bond_slaves_get``
  will be replaced by:
  ``rte_eth_bond_8023ad_member_info``,
  ``rte_eth_bond_active_members_get``,
  ``rte_eth_bond_member_add``,
  ``rte_eth_bond_member_remove``, and
  ``rte_eth_bond_members_get``.

* cryptodev: The elliptic curve asymmetric private and public keys can be maintained
  per session. These keys are moved from per packet ``rte_crypto_ecdsa_op_param`` and
  ``rte_crypto_sm2_op_param`` to generic EC xform ``rte_crypto_ec_xform``.

* security: Structures ``rte_security_ops`` and ``rte_security_ctx`` were moved to
  internal library headers not visible to application.

* mldev: Updated the structure ``rte_ml_model_info`` to support input and output
  with arbitrary shapes.
  Updated ``rte_ml_op``, ``rte_ml_io_quantize`` and ``rte_ml_io_dequantize``
  to support an array of ``rte_ml_buff_seg``.

* pcapng: The time parameters were removed
  from the functions ``rte_pcapng_copy`` and ``rte_pcapng_write_stats``.


ABI Changes
-----------

* ethdev: Added ``recycle_tx_mbufs_reuse`` and ``recycle_rx_descriptors_refill``
  fields to ``rte_eth_dev`` structure.

* ethdev: Structure ``rte_eth_fp_ops`` was affected to add
  ``recycle_tx_mbufs_reuse`` and ``recycle_rx_descriptors_refill``
  fields, to move ``rxq`` and ``txq`` fields, to change the size of
  ``reserved1`` and ``reserved2`` fields.

* ethdev: Added ``algorithm`` field to ``rte_eth_rss_conf`` structure
  for RSS hash algorithm.

* ethdev: Added ``rss_algo_capa`` field to ``rte_eth_dev_info`` structure
  for reporting RSS hash algorithm capability.

* security: struct ``rte_security_ipsec_sa_options`` was updated
  due to inline out-of-place feature addition.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel Atom\ |reg| P5342 processor
    * Intel\ |reg| Atom\ |trade| x74xxRE
    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-1747NTE CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H
    * GENUINE INTEL\ |reg| XEON\ |reg|

  * OS:

    * CBL Mariner 2.0
    * Fedora 38
    * FreeBSD 13.2
    * OpenAnolis OS 8.8
    * Red Hat Enterprise Linux Server release 8.7
    * Red Hat Enterprise Linux Server release 9.2
    * SUSE Linux Enterprise Server 15 SP5
    * Ubuntu 22.04.3

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.40 0x8001c301 1.3492.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.13.1_1_g565e8ce94_dirty (ice)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)/ 5.14.0-284.11.1.rt14.296.el9_2.x86_64 (RHEL9.2)/ 5.15.129-rt67 (Ubuntu22.04.3)(ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.40 0x8001c2f1 1.3492.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.13.1_1_g565e8ce94_dirty (ice)
      * Driver version(in-tree): 5.15.55.1-1.cm2-5464b22cac7+ (CBL Mariner 2.0) (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.40 0x8001c2f5 1.3492.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.13.1_1_g565e8ce94_dirty (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0

    * Intel\ |reg| Ethernet Connection E823-C for QSFP

      * Firmware version: 3.33 0x8001b295 1.3443.0
      * Device id (pf/vf): 8086:188b / 8086:1889
      * Driver version: 1.13.1_1_g565e8ce94_dirty (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E823-L for QSFP

      * Firmware version: 3.33 0x8001b4b0 1.3429.0
      * Device id (pf/vf): 8086:124c / 8086:1889
      * Driver version: 1.13.1_1_g565e8ce94_dirty (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| Ethernet Connection E822-L for backplane

      * Firmware version: 3.33 0x8001b4b6 1.3429.0
      * Device id (pf/vf): 8086:1897 / 8086:1889
      * Driver version: 1.13.1_1_g565e8ce94_dirty (ice)
      * OS Default DDP: 1.3.36.0
      * COMMS DDP: 1.3.46.0
      * Wireless Edge DDP: 1.3.14.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x000161bf
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.19.6 (ixgbe)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)/ 5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.30 0x8000e606 1.3429.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.23.17 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.20 0x80003d82 1.3353.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.23.17 (i40e)
      * Driver version(in-tree): 5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T

      * Firmware version: 6.20 0x80003d3e 1.2935.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version(out-tree): 2.23.17 (i40e)
      * Driver version(in-tree): 5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 9.30 0x8000e5f5 1.3429.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.23.17 (i40e)
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)/5.14.0-284.11.1.el9_2.x86_64 (RHEL9.2)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.30 0x8000e5ee 1.3429.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.23.17 (i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 5.15.0-82-generic (Ubuntu22.04.3)(igc)

    * Intel Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80001001
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version(in-tree): 5.15.0-60-generic (Ubuntu22.04.3)(igb)

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

    * MLNX_OFED 23.07-0.5.1.2 and above

  * upstream kernel:

    * Linux 6.7.0-rc1 and above

  * rdma-core:

    * rdma-core-48.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.32.1010 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.32.1010 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.35.2000 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.38.1900 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.38.1002 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.38.1002 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 23.07-0.5.0.0 and above
    * DOCA_2.2.0_BSP_4.2.0_Ubuntu_22.04-2.23-07
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
      * Firmware version: 22.38.1900 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.38.1900 and above

  * OFED:

    * MLNX_OFED 23.07-0.5.1.2

23.11.1 Release Notes
---------------------


23.11.1 Fixes
~~~~~~~~~~~~~

* 23.11.1-rc1
* app/crypto-perf: add missing op resubmission
* app/crypto-perf: fix copy segment size
* app/crypto-perf: fix data comparison
* app/crypto-perf: fix encrypt operation verification
* app/crypto-perf: fix next segment mbuf
* app/crypto-perf: fix out-of-place mbuf size
* app/crypto-perf: verify strdup return
* app/dma-perf: verify strdup return
* app/dumpcap: verify strdup return
* app/graph: fix build reason
* app/pdump: verify strdup return
* app/testpmd: fix --stats-period option check
* app/testpmd: fix GRO packets flush on timeout
* app/testpmd: fix async flow create failure handling
* app/testpmd: fix async indirect action list creation
* app/testpmd: fix auto-completion for indirect action list
* app/testpmd: fix burst option parsing
* app/testpmd: fix crash in multi-process forwarding
* app/testpmd: fix error message for invalid option
* app/testpmd: fix flow modify tag typo
* app/testpmd: hide --bitrate-stats in help if disabled
* app/testpmd: return if no packets in GRO heavy weight mode
* app/testpmd: verify strdup return
* build: fix linker warnings about undefined symbols
* build: fix reasons conflict
* build: link static libs with whole-archive in subproject
* build: pass cflags in subproject
* buildtools/cmdline: fix IP address initializer
* buildtools/cmdline: fix generated code for IP addresses
* bus/dpaa: verify strdup return
* bus/fslmc: verify strdup return
* bus/vdev: fix devargs in secondary process
* bus/vdev: verify strdup return
* ci: update versions of actions in GHA
* common/cnxk: fix RSS RETA configuration
* common/cnxk: fix Tx MTU configuration
* common/cnxk: fix VLAN check for inner header
* common/cnxk: fix inline device pointer check
* common/cnxk: fix link config for SDP
* common/cnxk: fix mbox region copy
* common/cnxk: fix mbox struct attributes
* common/cnxk: fix memory leak in CPT init
* common/cnxk: fix possible out-of-bounds access
* common/cnxk: remove CN9K inline IPsec FP opcodes
* common/cnxk: remove dead code
* common/mlx5: fix calloc parameters
* common/mlx5: fix duplicate read of general capabilities
* common/mlx5: fix query sample info capability
* common/qat: fix legacy flag
* common/sfc_efx/base: use C11 static assert
* config: fix CPU instruction set for cross-build
* crypto/cnxk: fix CN9K ECDH public key verification
* crypto/qat: fix crash with CCM null AAD pointer
* cryptodev: remove unused extern variable
* dma/dpaa2: fix logtype register
* dma/idxd: verify strdup return
* dmadev: fix calloc parameters
* doc: add --latencystats option in testpmd guide
* doc: add link speeds configuration in features table
* doc: add traffic manager in features table
* doc: fix aging poll frequency option in cnxk guide
* doc: fix commands in eventdev test tool guide
* doc: fix configuration in baseband 5GNR driver guide
* doc: fix default IP fragments maximum in programmer guide
* doc: fix typo in packet framework guide
* doc: fix typo in profiling guide
* doc: fix typos in cryptodev overview
* doc: remove cmdline polling mode deprecation notice
* doc: update link to Windows DevX in mlx5 guide
* drivers/net: fix buffer overflow for packet types list
* dts: fix smoke tests driver regex
* dts: strip whitespaces from stdout and stderr
* eal/x86: add AMD vendor check for TSC calibration
* eal: verify strdup return
* ethdev: fix NVGRE encap flow action description
* event/cnxk: fix dequeue timeout configuration
* event/cnxk: verify strdup return
* event/dlb2: remove superfluous memcpy
* eventdev/crypto: fix enqueueing
* eventdev: fix Doxygen processing of vector struct
* eventdev: fix calloc parameters
* eventdev: improve Doxygen comments on configure struct
* examples/ipsec-secgw: fix Rx queue ID in Rx callback
* examples/ipsec-secgw: fix cryptodev to SA mapping
* examples/ipsec-secgw: fix typo in error message
* examples/ipsec-secgw: fix width of variables
* examples/l3fwd: fix Rx over not ready port
* examples/l3fwd: fix Rx queue configuration
* examples/packet_ordering: fix Rx with reorder mode disabled
* examples/qos_sched: fix memory leak in args parsing
* examples/vhost: verify strdup return
* gro: fix reordering of packets
* hash: remove some dead code
* kernel/freebsd: fix module build on FreeBSD 14
* lib: add newline in logs
* lib: remove redundant newline from logs
* lib: use dedicated logtypes and macros
* ml/cnxk: fix xstats calculation
* net/af_xdp: fix leak on XSK configuration failure
* net/af_xdp: fix memzone leak on config failure
* net/bnx2x: fix calloc parameters
* net/bnx2x: fix warnings about memcpy lengths
* net/bnxt: fix 50G and 100G forced speed
* net/bnxt: fix array overflow
* net/bnxt: fix backward firmware compatibility
* net/bnxt: fix deadlock in ULP timer callback
* net/bnxt: fix null pointer dereference
* net/bnxt: fix number of Tx queues being created
* net/bnxt: fix speed change from 200G to 25G on Thor
* net/bnxt: modify locking for representor Tx
* net/bonding: fix flow count query
* net/cnxk: add cookies check for multi-segment offload
* net/cnxk: fix MTU limit
* net/cnxk: fix Rx packet format check condition
* net/cnxk: fix aged flow query
* net/cnxk: fix buffer size configuration
* net/cnxk: fix flow RSS configuration
* net/cnxk: fix indirect mbuf handling in Tx
* net/cnxk: fix mbuf fields in multi-segment Tx
* net/cnxk: improve Tx performance for SW mbuf free
* net/ena/base: fix metrics excessive memory consumption
* net/ena/base: limit exponential backoff
* net/ena/base: restructure interrupt handling
* net/ena: fix fast mbuf free
* net/ena: fix mbuf double free in fast free mode
* net/failsafe: fix memory leak in args parsing
* net/gve: fix DQO for chained descriptors
* net/hns3: enable PFC for all user priorities
* net/hns3: fix VF multiple count on one reset
* net/hns3: fix disable command with firmware
* net/hns3: fix reset level comparison
* net/hns3: refactor PF mailbox message struct
* net/hns3: refactor VF mailbox message struct
* net/hns3: refactor handle mailbox function
* net/hns3: refactor send mailbox function
* net/hns3: remove QinQ insert support for VF
* net/hns3: support new device
* net/i40e: remove incorrect 16B descriptor read block
* net/i40e: remove redundant judgment in flow parsing
* net/iavf: fix crash on VF start
* net/iavf: fix memory leak on security context error
* net/iavf: fix no polling mode switching
* net/iavf: remove error logs for VLAN offloading
* net/iavf: remove incorrect 16B descriptor read block
* net/ice: fix link update
* net/ice: fix memory leaks
* net/ice: fix tunnel TSO capabilities
* net/ice: remove incorrect 16B descriptor read block
* net/igc: fix timesync disable
* net/ionic: fix RSS query
* net/ionic: fix device close
* net/ionic: fix missing volatile type for cqe pointers
* net/ixgbe: fix memoy leak after device init failure
* net/ixgbe: increase VF reset timeout
* net/ixgbevf: fix RSS init for x550 NICs
* net/mana: fix memory leak on MR allocation
* net/mana: handle MR cache expansion failure
* net/mana: prevent values overflow returned from RDMA layer
* net/memif: fix crash with Tx burst larger than 255
* net/memif: fix extra mbuf refcnt update in zero copy Tx
* net/mlx5/hws: check not supported fields in VXLAN
* net/mlx5/hws: enable multiple integrity items
* net/mlx5/hws: fix ESP flow matching validation
* net/mlx5/hws: fix VLAN inner type
* net/mlx5/hws: fix VLAN item in non-relaxed mode
* net/mlx5/hws: fix direct index insert on depend WQE
* net/mlx5/hws: fix memory access in L3 decapsulation
* net/mlx5/hws: fix port ID for root table
* net/mlx5/hws: fix tunnel protocol checks
* net/mlx5/hws: skip item when inserting rules by index
* net/mlx5: fix DR context release ordering
* net/mlx5: fix GENEVE TLV option management
* net/mlx5: fix GENEVE option item translation
* net/mlx5: fix HWS meter actions availability
* net/mlx5: fix HWS registers initialization
* net/mlx5: fix IP-in-IP tunnels recognition
* net/mlx5: fix VLAN ID in flow modify
* net/mlx5: fix VLAN handling in meter split
* net/mlx5: fix age position in hairpin split
* net/mlx5: fix async flow create error handling
* net/mlx5: fix condition of LACP miss flow
* net/mlx5: fix connection tracking action validation
* net/mlx5: fix conntrack action handle representation
* net/mlx5: fix counters map in bonding mode
* net/mlx5: fix drop action release timing
* net/mlx5: fix error packets drop in regular Rx
* net/mlx5: fix flow action template expansion
* net/mlx5: fix flow configure validation
* net/mlx5: fix flow counter cache starvation
* net/mlx5: fix flow tag modification
* net/mlx5: fix indirect action async job initialization
* net/mlx5: fix jump action validation
* net/mlx5: fix meter policy priority
* net/mlx5: fix modify flex item
* net/mlx5: fix non-masked indirect list meter translation
* net/mlx5: fix parameters verification in HWS table create
* net/mlx5: fix rollback on failed flow configure
* net/mlx5: fix stats query crash in secondary process
* net/mlx5: fix sync flow meter action
* net/mlx5: fix sync meter processing in HWS
* net/mlx5: fix template clean up of FDB control flow rule
* net/mlx5: fix use after free when releasing Tx queues
* net/mlx5: fix warning about copy length
* net/mlx5: prevent ioctl failure log flooding
* net/mlx5: prevent querying aged flows on uninit port
* net/mlx5: remove GENEVE options length limitation
* net/mlx5: remove device status check in flow creation
* net/mlx5: remove duplication of L3 flow item validation
* net/netvsc: fix VLAN metadata parsing
* net/nfp: fix IPsec data endianness
* net/nfp: fix NFD3 metadata process
* net/nfp: fix NFDk metadata process
* net/nfp: fix Rx descriptor
* net/nfp: fix Rx memory leak
* net/nfp: fix calloc parameters
* net/nfp: fix device close
* net/nfp: fix device resource freeing
* net/nfp: fix getting firmware VNIC version
* net/nfp: fix initialization failure flow
* net/nfp: fix resource leak for CoreNIC firmware
* net/nfp: fix resource leak for PF initialization
* net/nfp: fix resource leak for VF
* net/nfp: fix resource leak for device initialization
* net/nfp: fix resource leak for exit of CoreNIC firmware
* net/nfp: fix resource leak for exit of flower firmware
* net/nfp: fix resource leak for flower firmware
* net/nfp: fix switch domain free check
* net/nfp: fix uninitialized variable
* net/nfp: free switch domain ID on close
* net/nfp: verify strdup return
* net/sfc: fix calloc parameters
* net/softnic: fix include of log library
* net/tap: do not overwrite flow API errors
* net/tap: fix traffic control handle calculation
* net/thunderx: fix DMAC control register update
* net/virtio: fix vDPA device init advertising control queue
* net/virtio: remove duplicate queue xstats
* net/vmxnet3: fix initialization on FreeBSD
* net/vmxnet3: ignore Rx queue interrupt setup on FreeBSD
* net: add macros for VLAN metadata parsing
* net: fix TCP/UDP checksum with padding data
* pipeline: fix calloc parameters
* rawdev: fix calloc parameters
* rcu: fix acked token in debug log
* rcu: use atomic operation on acked token
* regexdev: fix logtype register
* telemetry: fix connected clients count
* telemetry: fix empty JSON dictionaries
* test/cfgfile: fix typo in error messages
* test/event: fix crash in Tx adapter freeing
* test/event: skip test if no driver is present
* test/mbuf: fix external mbuf case with assert enabled
* test/power: fix typo in error message
* test: assume C source files are UTF-8 encoded
* test: do not count skipped tests as executed
* test: fix probing in secondary process
* test: verify strdup return
* vdpa/mlx5: fix queue enable drain CQ
* version: 23.11.1-rc2
* vhost: fix VDUSE device destruction failure
* vhost: fix deadlock during vDPA SW live migration
* vhost: fix memory leak in Virtio Tx split path
* vhost: fix virtqueue access check in VDUSE setup
* vhost: fix virtqueue access check in datapath
* vhost: fix virtqueue access check in vhost-user setup

23.11.1 Validation
~~~~~~~~~~~~~~~~~~

* RedHat Testing:

    * Test scenarios:

        * VM with device assignment(PF) throughput testing(1G hugepage size)
        * VM with device assignment(PF) throughput testing(2M hugepage size)
        * VM with device assignment(VF) throughput testing
        * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
        * PVP vhost-user 2Q throughput testing
        * PVP vhost-user 1Q - cross numa node throughput testing
        * VM with vhost-user 2 queues throughput testing
        * vhost-user reconnect with dpdk-client, qemu-server qemu reconnect
        * vhost-user reconnect with dpdk-client, qemu-server ovs reconnect
        * PVP  reconnect with dpdk-client, qemu-server
        * PVP 1Q live migration testing
        * PVP 1Q cross numa node live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing (2M)
        * VM with ovs+dpdk+vhost-user 2Q live migration testing
        * VM with ovs+dpdk+vhost-user 4Q live migration testing
        * Host PF + DPDK testing
        * Host VF + DPDK testing

    * Test Versions and device:

        * RHEL 9.4
        * qemu-kvm-8.2.0
        * kernel 5.14
        * libvirt 10.0
        * X540-AT2 NIC(ixgbe, 10G)

* Nvidia(R) Testing:

    * Test scenarios:

        * Send and receive multiple types of traffic.
        * testpmd xstats counter test.
        * testpmd timestamp test.
        * Changing/checking link status through testpmd.
        * rte_flow tests (https://doc.dpdk.org/guides/nics/mlx5.html#supported-hardware-offloads)
        * RSS tests.
        * VLAN filtering, stripping, and insertion tests.
        * Checksum and TSO tests.
        * ptype tests.
        * link_status_interrupt example application tests.
        * l3fwd-power example application tests.
        * Multi-process example applications tests.
        * Hardware LRO tests.
        * Buffer Split tests.
        * Tx scheduling tests.

    * Test platform:

        * NIC: ConnectX-6 Dx / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-24.04-0.6.6.0 / Firmware: 22.41.1000
        * NIC: ConnectX-7 / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-24.04-0.6.6.0 / Firmware: 28.41.1000
        * DPU: BlueField-2 / DOCA SW version: 2.7.0 / Firmware: 24.41.1000

    * OS/driver combinations:

        * Debian 12 with MLNX_OFED_LINUX-24.01-0.3.3.1.
        * Ubuntu 20.04.6 with MLNX_OFED_LINUX-24.01-0.3.3.1.
        * Ubuntu 22.04.4 with MLNX_OFED_LINUX-24.04-0.6.6.0.
        * Ubuntu 20.04.6 with rdma-core master (311c591).
        * Ubuntu 20.04.6 with rdma-core v28.0.
        * Fedora 40 with rdma-core v48.0.
        * Fedora 41 (Rawhide) with rdma-core v51.0.
        * OpenSUSE Leap 15.5 with rdma-core v42.0.
        * Windows Server 2019 with Clang 16.0.6.

* Intel(R) Testing:

    * Basic NIC testing

        * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu23.10, Ubuntu22.04, Fedora39, RHEL8.9, RHEL9.2, FreeBSD14.0, SUSE15, CentOS7.9, openEuler22.03-SP2，OpenAnolis8.8 etc.
        * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
        * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
        * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

    * Basic cryptodev and virtio testing

        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev Function test: Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * Cryptodev Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

23.11.1 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Start dpdk-pdump in VM with virtio-0.95 protocol failed

    Fix available in upstream.

23.11.2 Release Notes
---------------------


23.11.2 Fixes
~~~~~~~~~~~~~

* app/bbdev: fix MLD output size computation
* app/bbdev: fix TB logic
* app/bbdev: fix interrupt tests
* app/crypto-perf: fix result for asymmetric
* app/crypto-perf: remove redundant local variable
* app/dumpcap: handle SIGTERM and SIGHUP
* app/pdump: handle SIGTERM and SIGHUP
* app/testpmd: add postpone option to async flow destroy
* app/testpmd: fix build on signed comparison
* app/testpmd: fix help string of BPF load command
* app/testpmd: fix indirect action flush
* app/testpmd: fix lcore ID restriction
* app/testpmd: fix outer IP checksum offload
* app/testpmd: fix parsing for connection tracking item
* app/testpmd: handle IEEE1588 init failure
* baseband/acc: fix memory barrier
* baseband/la12xx: forbid secondary process
* bpf: disable on 32-bit x86
* bpf: fix MOV instruction evaluation
* bpf: fix load hangs with six IPv6 addresses
* build: use builtin helper for python dependencies
* buildtools: fix build with clang 17 and ASan
* bus/dpaa: fix bus scan for DMA devices
* bus/dpaa: fix memory leak in bus scan
* bus/dpaa: remove redundant file descriptor check
* bus/pci: fix FD in secondary process
* bus/pci: fix UIO resource mapping in secondary process
* bus/pci: fix build with musl 1.2.4 / Alpine 3.19
* bus/vdev: fix device reinitialization
* common/cnxk: fix flow aging cleanup
* common/cnxk: fix flow aging on application exit
* common/cnxk: fix integer overflow
* common/cnxk: fix segregation of logs based on module
* common/dpaax/caamflib: fix PDCP AES-AES watchdog error
* common/dpaax/caamflib: fix PDCP-SDAP watchdog error
* common/dpaax: fix IOVA table cleanup
* common/dpaax: fix node array overrun
* common/idpf: fix PTP message validation
* common/idpf: fix flex descriptor mask
* common/mlx5: fix PRM structs
* common/mlx5: fix unsigned/signed mismatch
* common/mlx5: remove unneeded field when modify RQ table
* config: fix warning for cross build with meson >= 1.3.0
* crypto/cnxk: fix ECDH public key verification
* crypto/cnxk: fix minimal input normalization
* crypto/cnxk: fix out-of-bound access
* crypto/dpaa2_sec: fix event queue user context
* crypto/dpaa_sec: fix IPsec descriptor
* crypto/ipsec_mb: fix function comment
* crypto/openssl: fix GCM and CCM thread unsafe contexts
* crypto/openssl: make per-QP auth context clones
* crypto/openssl: make per-QP cipher context clones
* crypto/openssl: optimize 3DES-CTR context init
* crypto/openssl: set cipher padding once
* crypto/qat: fix GEN4 write
* crypto/qat: fix log message typo
* crypto/qat: fix placement of OOP offset
* cryptodev: fix build without crypto callbacks
* cryptodev: validate crypto callbacks from next node
* devtools: fix symbol listing
* dma/hisilicon: remove support for HIP09 platform
* dma/idxd: fix setup with Ubuntu 24.04
* dmadev: fix structure alignment
* doc: add baseline mode in l3fwd-power guide
* doc: add power uncore in API index
* doc: describe mlx5 HWS actions order
* doc: fix AF_XDP device plugin howto
* doc: fix DMA performance test invocation
* doc: fix link to hugepage mapping from Linux guide
* doc: fix mbuf flags
* doc: fix testpmd ring size command
* doc: fix typo in l2fwd-crypto guide
* doc: remove empty section from testpmd guide
* doc: remove reference to mbuf pkt field
* doc: update AF_XDP device plugin repository
* doc: update metadata description in nfp guide
* eal/linux: lower log level on allocation attempt failure
* eal/unix: support ZSTD compression for firmware
* eal/windows: install sched.h file
* eal: fix type in destructor macro for MSVC
* ethdev: fix GENEVE option item conversion
* ethdev: fix device init without socket-local memory
* ethdev: fix strict aliasing in link up
* event/sw: fix warning from useless snprintf
* eventdev/crypto: fix opaque field handling
* examples/fips_validation: fix dereference and out-of-bound
* examples/ipsec-secgw: fix SA salt endianness
* examples/ipsec-secgw: revert SA salt endianness
* examples/l3fwd: fix crash in ACL mode for mixed traffic
* examples/l3fwd: fix crash on multiple sockets
* examples: fix lcore ID restriction
* examples: fix port ID restriction
* examples: fix queue ID restriction
* fbarray: fix finding for unaligned length
* fbarray: fix incorrect lookahead behavior
* fbarray: fix incorrect lookbehind behavior
* fbarray: fix lookahead ignore mask handling
* fbarray: fix lookbehind ignore mask handling
* graph: fix ID collisions
* graph: fix mcore dispatch walk
* graph: fix stats retrieval while destroying a graph
* hash: check name when creating a hash
* hash: fix RCU reclamation size
* hash: fix return code description in Doxygen
* latencystats: fix literal float suffix
* malloc: fix multi-process wait condition handling
* mbuf: fix dynamic fields copy
* net/af_packet: align Rx/Tx structs to cache line
* net/af_xdp: count mbuf allocation failures
* net/af_xdp: fix multi-interface support for k8s
* net/af_xdp: fix port ID in Rx mbuf
* net/af_xdp: fix stats reset
* net/af_xdp: remove unused local statistic
* net/ark: fix index arithmetic
* net/axgbe: check only minimum speed for cables
* net/axgbe: delay AN timeout during KR training
* net/axgbe: disable RRC for yellow carp devices
* net/axgbe: disable interrupts during device removal
* net/axgbe: enable PLL control for fixed PHY modes only
* net/axgbe: fix MDIO access for non-zero ports and CL45 PHYs
* net/axgbe: fix SFP codes check for DAC cables
* net/axgbe: fix Tx flow on 30H HW
* net/axgbe: fix connection for SFP+ active cables
* net/axgbe: fix fluctuations for 1G Bel Fuse SFP
* net/axgbe: fix linkup in PHY status
* net/axgbe: reset link when link never comes back
* net/axgbe: update DMA coherency values
* net/bonding: fix failover time of LACP with mode 4
* net/cnxk: fix RSS config
* net/cnxk: fix extbuf handling for multisegment packet
* net/cnxk: fix outbound security with higher packet burst
* net/cnxk: fix promiscuous state after MAC change
* net/cnxk: update SA userdata and keep original cookie
* net/cpfl: add checks on control queue messages
* net/cpfl: fix 32-bit build
* net/dpaa: forbid MTU configuration for shared interface
* net/e1000/base: fix link power down
* net/ena: fix bad checksum handling
* net/ena: fix checksum handling
* net/ena: fix return value check
* net/fm10k: fix cleanup during init failure
* net/gve: fix RSS hash endianness in DQO format
* net/gve: fix Tx queue state on queue start
* net/hns3: check Rx DMA address alignmnent
* net/hns3: disable SCTP verification tag for RSS hash input
* net/hns3: fix Rx timestamp flag
* net/hns3: fix double free for Rx/Tx queue
* net/hns3: fix offload flag of IEEE 1588
* net/hns3: fix uninitialized variable in FEC query
* net/hns3: fix variable overflow
* net/i40e: fix outer UDP checksum offload for X710
* net/iavf: fix VF reset when using DCF
* net/iavf: remove outer UDP checksum offload for X710 VF
* net/ice/base: fix GCS descriptor field offsets
* net/ice/base: fix board type definition
* net/ice/base: fix check for existing switch rule
* net/ice/base: fix masking when reading context
* net/ice/base: fix memory leak in firmware version check
* net/ice/base: fix pointer to variable outside scope
* net/ice/base: fix potential TLV length overflow
* net/ice/base: fix preparing PHY for timesync command
* net/ice/base: fix resource leak
* net/ice/base: fix return type of bitmap hamming weight
* net/ice/base: fix sign extension
* net/ice/base: fix size when allocating children arrays
* net/ice/base: fix temporary failures reading NVM
* net/ice: fix VLAN stripping in double VLAN mode
* net/ice: fix check for outer UDP checksum offload
* net/ice: fix memory leaks in raw pattern parsing
* net/ice: fix return value for raw pattern parsing
* net/ionic: fix mbuf double-free when emptying array
* net/ixgbe/base: fix 5G link speed reported on VF
* net/ixgbe/base: fix PHY ID for X550
* net/ixgbe/base: revert advertising for X550 2.5G/5G
* net/ixgbe: do not create delayed interrupt handler twice
* net/ixgbe: do not update link status in secondary process
* net/mana: fix uninitialized return value
* net/mlx5/hws: add template match none flag
* net/mlx5/hws: decrease log level for creation failure
* net/mlx5/hws: extend tag saving for match and jumbo
* net/mlx5/hws: fix action template dump
* net/mlx5/hws: fix check of range templates
* net/mlx5/hws: fix deletion of action vport
* net/mlx5/hws: fix function comment
* net/mlx5/hws: fix matcher reconnect
* net/mlx5/hws: fix memory leak in modify header
* net/mlx5/hws: fix port ID on root item convert
* net/mlx5/hws: fix spinlock release on context open
* net/mlx5/hws: remove unused variable
* net/mlx5/hws: set default miss when replacing table
* net/mlx5: break flow resource release loop
* net/mlx5: fix Arm build with GCC 9.1
* net/mlx5: fix MTU configuration
* net/mlx5: fix access to flow template operations
* net/mlx5: fix crash on counter pool destroy
* net/mlx5: fix disabling E-Switch default flow rules
* net/mlx5: fix end condition of reading xstats
* net/mlx5: fix flow template indirect action failure
* net/mlx5: fix hash Rx queue release in flow sample
* net/mlx5: fix indexed pool with invalid index
* net/mlx5: fix shared Rx queue data access race
* net/mlx5: fix start without duplicate flow patterns
* net/mlx5: fix uplink port probing in bonding mode
* net/mlx5: support jump in meter hierarchy
* net/netvsc: fix MTU set
* net/netvsc: use ethdev API to set VF MTU
* net/nfp: adapt reverse sequence card
* net/nfp: disable ctrl VNIC queues on close
* net/nfp: fix IPv6 TTL and DSCP flow action
* net/nfp: fix allocation of switch domain
* net/nfp: fix configuration BAR
* net/nfp: fix dereference of null pointer
* net/nfp: fix disabling 32-bit build
* net/nfp: fix firmware abnormal cleanup
* net/nfp: fix flow mask table entry
* net/nfp: fix getting firmware version
* net/nfp: fix repeat disable port
* net/nfp: fix representor port queue release
* net/nfp: fix resource leak in secondary process
* net/nfp: fix xstats for multi PF firmware
* net/nfp: forbid offload flow rules with empty action list
* net/nfp: remove redundant function call
* net/nfp: remove unneeded logic for VLAN layer
* net/ngbe: add special config for YT8531SH-CA PHY
* net/ngbe: fix MTU range
* net/ngbe: fix hotplug remove
* net/ngbe: fix memory leaks
* net/ngbe: keep PHY power down while device probing
* net/tap: fix file descriptor check in isolated flow
* net/txgbe: fix MTU range
* net/txgbe: fix Rx interrupt
* net/txgbe: fix Tx hang on queue disable
* net/txgbe: fix VF promiscuous and allmulticast
* net/txgbe: fix flow filters in VT mode
* net/txgbe: fix hotplug remove
* net/txgbe: fix memory leaks
* net/txgbe: fix tunnel packet parsing
* net/txgbe: reconfigure more MAC Rx registers
* net/txgbe: restrict configuration of VLAN strip offload
* net/virtio-user: add memcpy check
* net/virtio-user: fix control queue allocation
* net/virtio-user: fix control queue allocation for non-vDPA
* net/virtio-user: fix control queue destruction
* net/virtio-user: fix shadow control queue notification init
* net/virtio: fix MAC table update
* net/vmxnet3: add missing register command
* net/vmxnet3: fix init logs
* net: fix outer UDP checksum in Intel prepare helper
* pcapng: add memcpy check
* power: fix number of uncore frequencies
* telemetry: fix connection parameter parsing
* telemetry: lower log level on socket error
* test/crypto: fix RSA cases in QAT suite
* test/crypto: fix allocation comment
* test/crypto: fix asymmetric capability test
* test/crypto: fix enqueue/dequeue callback case
* test/crypto: fix modex comparison
* test/crypto: remove unused stats in setup
* test/crypto: validate modex from first non-zero
* v23.11.2-rc1
* vdpa/sfc: remove dead code
* version: 23.11.2-rc2
* vhost: cleanup resubmit info before inflight setup
* vhost: fix build with GCC 13

23.11.2 Validation
~~~~~~~~~~~~~~~~~~

* RedHat Testing:

    * Test scenarios:

        * VM with device assignment(PF) throughput testing(1G hugepage size)
        * VM with device assignment(PF) throughput testing(2M hugepage size)
        * VM with device assignment(VF) throughput testing
        * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
        * PVP vhost-user 2Q throughput testing
        * PVP vhost-user 1Q - cross numa node throughput testing
        * VM with vhost-user 2 queues throughput testing
        * vhost-user reconnect with dpdk-client, qemu-server qemu reconnect
        * vhost-user reconnect with dpdk-client, qemu-server ovs reconnect
        * PVP  reconnect with dpdk-client, qemu-server
        * PVP 1Q live migration testing
        * PVP 1Q cross numa node live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing (2M)
        * VM with ovs+dpdk+vhost-user 2Q live migration testing
        * VM with ovs+dpdk+vhost-user 4Q live migration testing
        * Host PF + DPDK testing
        * Host VF + DPDK testing

    * Test Versions and device:

        * RHEL 9.4
        * qemu-kvm-8.2.0
        * kernel 5.14
        * libvirt 10.0
        * openvswitch 3.3
        * X540-AT2 NIC(ixgbe, 10G)

* Nvidia(R) Testing:

    * Test scenarios:

        * Send and receive multiple types of traffic.
        * testpmd xstats counter test.
        * testpmd timestamp test.
        * Changing/checking link status through testpmd.
        * rte_flow tests (https://doc.dpdk.org/guides/nics/mlx5.html#supported-hardware-offloads)
        * RSS tests.
        * VLAN filtering, stripping, and insertion tests.
        * Checksum and TSO tests.
        * ptype tests.
        * link_status_interrupt example application tests.
        * l3fwd-power example application tests.
        * Multi-process example applications tests.
        * Hardware LRO tests.
        * Buffer Split tests.
        * Tx scheduling tests.

    * Test platform:

        * NIC: ConnectX-6 Dx / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-24.07-0.6.1.0 / Firmware: 22.42.1000
        * NIC: ConnectX-7 / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-24.07-0.6.1.0 / Firmware: 28.42.1000
        * DPU: BlueField-2 / DOCA SW version: 2.8.0 / Firmware: 24.42.1000

    * OS/driver combinations:

        * Debian 12 with MLNX_OFED_LINUX-24.04-0.7.0.0.
        * Ubuntu 20.04.6 with MLNX_OFED_LINUX-24.07-0.6.1.0.
        * Ubuntu 20.04.6 with rdma-core master (dd9c687).
        * Ubuntu 20.04.6 with rdma-core v28.0.
        * Fedora 40 with rdma-core v48.0.
        * Fedora 42 (Rawhide) with rdma-core v51.0.
        * OpenSUSE Leap 15.6 with rdma-core v49.1.

* Intel(R) Testing:

    * Basic NIC testing

        * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu23.10, Ubuntu22.04, Fedora39, RHEL8.9, RHEL9.2, FreeBSD14.0, SUSE15, CentOS7.9, openEuler22.03-SP2，OpenAnolis8.8 etc.
        * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
        * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
        * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

    * Basic cryptodev and virtio testing

        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev Function test: Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * Cryptodev Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

23.11.2 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Start dpdk-pdump in VM with virtio-0.95 protocol failed

    Fix available in upstream.

* Failed to add vdev when launch dpdk-pdump with vdev secondary process

    Fix available in upstream.

23.11.3 Release Notes
---------------------


23.11.3 Fixes
~~~~~~~~~~~~~

* 23.11.3-rc1
* Revert "test/bonding: fix loop on members"
* app/dumpcap: fix handling of jumbo frames
* app/dumpcap: remove unused struct array
* app/procinfo: fix leak on exit
* app/testpmd: avoid potential outside of array reference
* app/testpmd: fix aged flow destroy
* app/testpmd: remove flex item init command leftover
* app/testpmd: remove redundant policy action condition
* app/testpmd: remove unnecessary cast
* baseband/acc: fix access to deallocated mem
* baseband/acc: fix ring memory allocation
* baseband/acc: fix soft output bypass RM
* baseband/la12xx: fix use after free in modem config
* bpf: fix free function mismatch if convert fails
* build: remove version check on compiler links function
* bus/dpaa: fix PFDRs leaks due to FQRNIs
* bus/dpaa: fix VSP for 1G fm1-mac9 and 10
* bus/dpaa: fix lock condition during error handling
* bus/dpaa: fix the fman details status
* bus/fslmc: fix Coverity warnings in QBMAN
* bus/vdev: revert fix devargs in secondary process
* common/cnxk: fix CPT HW word size for outbound SA
* common/cnxk: fix IRQ reconfiguration
* common/cnxk: fix MAC address change with active VF
* common/cnxk: fix base log level
* common/cnxk: fix build on Ubuntu 24.04
* common/cnxk: fix double free of flow aging resources
* common/cnxk: fix inline CTX write
* common/dpaax/caamflib: enable fallthrough warnings
* common/dpaax/caamflib: fix PDCP SNOW-ZUC watchdog
* common/idpf: fix AVX-512 pointer copy on 32-bit
* common/idpf: fix use after free in mailbox init
* common/mlx5: fix error CQE handling for 128 bytes CQE
* common/mlx5: fix misalignment
* common/qat: fix use after free in device probe
* crypto/bcmfs: fix free function mismatch
* crypto/dpaa2_sec: fix memory leak
* crypto/openssl: fix 3DES-CTR with big endian CPUs
* crypto/openssl: fix potential string overflow
* crypto/qat: fix ECDSA session handling
* crypto/qat: fix modexp/inv length
* crypto/scheduler: fix session size computation
* dev: fix callback lookup when unregistering device
* devtools: fix check of multiple commits fixed at once
* dma/idxd: fix free function mismatch in device probe
* dmadev: fix potential null pointer access
* doc: correct definition of stats per queue feature
* drivers: remove redundant newline from logs
* eal/unix: optimize thread creation
* eal/x86: fix 32-bit write combining store
* ethdev: fix overflow in descriptor count
* ethdev: verify queue ID in Tx done cleanup
* event/cnxk: fix OOP handling in event mode
* event/cnxk: fix Rx timestamp handling
* event/cnxk: fix free function mismatch in port config
* event/octeontx: fix possible integer overflow
* eventdev: fix possible array underflow/overflow
* examples/eventdev: fix queue crash with generic pipeline
* examples/ipsec-secgw: fix dequeue count from cryptodev
* examples/l2fwd-event: fix spinlock handling
* examples/l3fwd-power: fix options parsing overflow
* examples/l3fwd: fix read beyond boundaries
* examples/ntb: check info query return
* examples/vhost: fix free function mismatch
* fib6: add runtime checks in AVX512 lookup
* fib: fix AVX512 lookup
* hash: fix thash LFSR initialization
* log: remove per line log helper
* member: fix choice of bucket for displacement
* ml/cnxk: fix handling of TVM model I/O
* net/bnx2x: fix always true expression
* net/bnx2x: fix duplicate branch
* net/bnx2x: fix possible infinite loop at startup
* net/bnx2x: remove dead conditional
* net/bnxt/tf_core: fix TCAM manager data corruption
* net/bnxt/tf_core: fix Thor TF EM key size check
* net/bnxt/tf_core: fix WC TCAM multi-slice delete
* net/bnxt/tf_core: fix slice count in case of HA entry move
* net/bnxt: fix TCP and UDP checksum flags
* net/bnxt: fix bad action offset in Tx BD
* net/bnxt: fix reading SFF-8436 SFP EEPROMs
* net/cnxk: fix OOP handling for inbound packets
* net/cnxk: fix Rx offloads to handle timestamp
* net/cnxk: fix Rx timestamp handling for VF
* net/cnxk: fix build on Ubuntu 24.04
* net/cnxk: fix use after free in mempool create
* net/cpfl: add checks for flow action types
* net/cpfl: fix forwarding to physical port
* net/cpfl: fix invalid free in JSON parser
* net/cpfl: fix parsing protocol ID mask field
* net/dpaa2: fix memory corruption in TM
* net/dpaa2: remove unnecessary check for null before free
* net/dpaa: fix reallocate mbuf handling
* net/dpaa: fix typecasting channel ID
* net/e1000/base: fix fallthrough in switch
* net/e1000: fix link status crash in secondary process
* net/e1000: fix use after free in filter flush
* net/ena: revert redefining memcpy
* net/gve/base: fix build with Fedora Rawhide
* net/gve: add IO memory barriers before reading descriptors
* net/gve: always attempt Rx refill on DQ
* net/gve: fix Tx for chained mbuf
* net/gve: fix mbuf allocation memory leak for DQ Rx
* net/gve: fix queue setup and stop
* net/gve: fix refill logic causing memory corruption
* net/hns3: fix dump counter of registers
* net/hns3: fix error code for repeatedly create counter
* net/hns3: fix fully use hardware flow director table
* net/hns3: register VLAN flow match mode parameter
* net/hns3: remove ROH devices
* net/hns3: remove some basic address dump
* net/hns3: restrict tunnel flow rule to one header
* net/hns3: verify reset type from firmware
* net/i40e/base: add missing X710TL device check
* net/i40e/base: fix DDP loading with reserved track ID
* net/i40e/base: fix blinking X722 with X557 PHY
* net/i40e/base: fix loop bounds
* net/i40e/base: fix misleading debug logs and comments
* net/i40e/base: fix repeated register dumps
* net/i40e/base: fix setting flags in init function
* net/i40e/base: fix unchecked return value
* net/i40e: check register read for outer VLAN
* net/i40e: fix AVX-512 pointer copy on 32-bit
* net/iavf: add segment-length check to Tx prep
* net/iavf: delay VF reset command
* net/iavf: fix AVX-512 pointer copy on 32-bit
* net/iavf: fix crash when link is unstable
* net/iavf: preserve MAC address with i40e PF Linux driver
* net/ice/base: add bounds check
* net/ice/base: fix VLAN replay after reset
* net/ice/base: fix iteration of TLVs in Preserved Fields Area
* net/ice/base: fix link speed for 200G
* net/ice: detect stopping a flow director queue twice
* net/ice: fix AVX-512 pointer copy on 32-bit
* net/igc: fix Rx buffers when timestamping enabled
* net/ionic: fix build with Fedora Rawhide
* net/ixgbe/base: fix unchecked return value
* net/ixgbe: fix link status delay on FreeBSD
* net/mana: support rdma-core via pkg-config
* net/memif: fix buffer overflow in zero copy Rx
* net/mlx5/hws: fix allocation of STCs
* net/mlx5/hws: fix flex item as tunnel header
* net/mlx5/hws: fix range definer error recovery
* net/mlx5: add flex item query for tunnel mode
* net/mlx5: fix GRE flow item translation for root table
* net/mlx5: fix Rx queue control management
* net/mlx5: fix Rx queue reference count in flushing flows
* net/mlx5: fix SQ flow item size
* net/mlx5: fix SWS meter state initialization
* net/mlx5: fix Tx tracing to use single clock source
* net/mlx5: fix counter query loop getting stuck
* net/mlx5: fix default RSS flows creation order
* net/mlx5: fix flex item header length field translation
* net/mlx5: fix flex item tunnel mode
* net/mlx5: fix indirect list flow action callback invocation
* net/mlx5: fix memory leak in metering
* net/mlx5: fix miniCQEs number calculation
* net/mlx5: fix next protocol validation after flex item
* net/mlx5: fix non full word sample fields in flex item
* net/mlx5: fix non-template flow action validation
* net/mlx5: fix number of supported flex parsers
* net/mlx5: fix real time counter reading from PCI BAR
* net/mlx5: fix reported Rx/Tx descriptor limits
* net/mlx5: fix shared Rx queue control release
* net/mlx5: fix shared queue port number in vector Rx
* net/mlx5: fix trace script for multiple burst completion
* net/mlx5: workaround list management of Rx queue control
* net/mvneta: fix possible out-of-bounds write
* net/netvsc: fix using Tx queue higher than Rx queues
* net/netvsc: force Tx VLAN offload on 801.2Q packet
* net/nfb: fix use after free
* net/nfp: do not set IPv6 flag in transport mode
* net/nfp: fix double free in flow destroy
* net/nfp: fix link change return value
* net/nfp: fix pause frame setting check
* net/nfp: fix representor port link status update
* net/nfp: fix type declaration of some variables
* net/nfp: notify flower firmware about PF speed
* net/ngbe: fix driver load bit to inform firmware
* net/ngbe: fix interrupt lost in legacy or MSI mode
* net/ngbe: reconfigure more MAC Rx registers
* net/ngbe: restrict configuration of VLAN strip offload
* net/pcap: fix blocking Rx
* net/pcap: set live interface as non-blocking
* net/sfc: fix use after free in debug logs
* net/tap: avoid memcpy with null argument
* net/tap: restrict maximum number of MP FDs
* net/txgbe: fix SWFW mbox
* net/txgbe: fix VF-PF mbox interrupt
* net/txgbe: fix a mass of interrupts
* net/txgbe: fix driver load bit to inform firmware
* net/txgbe: remove outer UDP checksum capability
* net/virtio-user: reset used index counter
* net/virtio: fix Rx checksum calculation
* net/vmxnet3: fix crash after configuration failure
* net/vmxnet3: fix potential out of bounds stats access
* net/vmxnet3: support larger MTU with version 6
* pcapng: avoid potential unaligned data
* pcapng: fix handling of chained mbufs
* power: enable CPPC
* power: fix log message when checking lcore ID
* power: fix mapped lcore ID
* raw/ifpga/base: fix use after free
* raw/ifpga: fix free function mismatch in interrupt config
* rcu: fix implicit conversion in bit shift
* test/bonding: fix MAC address comparison
* test/bonding: fix loop on members
* test/bonding: remove redundant info query
* test/crypto: fix synchronous API calls
* test/eal: fix lcore check
* test/eal: fix loop coverage for alignment macros
* test/event: avoid duplicate initialization
* test/event: fix schedule type
* test/event: fix target event queue
* test/security: fix IPv6 extension loop
* vdpa/nfp: fix hardware initialization
* vdpa/nfp: fix reconfiguration
* vdpa: update used flags in used ring relay
* version: 23.11.3-rc1
* vhost: fix deadlock in Rx async path
* vhost: fix offset while mapping log base address
* vhost: restrict set max queue pair API to VDUSE

23.11.3 Validation
~~~~~~~~~~~~~~~~~~

* RedHat Testing:

    * Test scenarios:

        * VM with device assignment(PF) throughput testing(1G hugepage size)
        * VM with device assignment(PF) throughput testing(2M hugepage size)
        * VM with device assignment(VF) throughput testing
        * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
        * PVP vhost-user 2Q throughput testing
        * PVP vhost-user 1Q - cross numa node throughput testing
        * VM with vhost-user 2 queues throughput testing
        * vhost-user reconnect with dpdk-client, qemu-server(qemu reconnect)
        * vhost-user reconnect with dpdk-client, qemu-server(ovs reconnect)
        * PVP  reconnect with dpdk-client, qemu-server
        * PVP 1Q live migration testing
        * PVP 1Q cross numa node live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing (2M)
        * VM with ovs+dpdk+vhost-user 2Q live migration testing
        * VM with ovs+dpdk+vhost-user 4Q live migration testing
        * Host PF + DPDK testing
        * Host VF + DPDK testing

    * Test Versions and device:

        * RHEL 9.4
        * qemu-kvm-8.2.0
        * kernel 5.14
        * libvirt 10.0
        * openvswitch 3.1
        * X540-AT2 NIC(ixgbe, 10G)

* Nvidia(R) Testing:

    * Test scenarios:

        * Send and receive multiple types of traffic.
        * testpmd xstats counter test.
        * testpmd timestamp test.
        * Changing/checking link status through testpmd.
        * rte_flow tests (https://doc.dpdk.org/guides/nics/mlx5.html#supported-hardware-offloads)
        * RSS tests.
        * VLAN filtering, stripping, and insertion tests.
        * Checksum and TSO tests.
        * ptype tests.
        * link_status_interrupt example application tests.
        * l3fwd-power example application tests.
        * Multi-process example applications tests.
        * Hardware LRO tests.
        * Buffer Split tests.
        * Tx scheduling tests.

    * Test platform:

        * NIC: ConnectX-6 Dx / OS: Ubuntu 22.04 / Driver: MLNX_OFED_LINUX-24.10-1.1.4.0 / Firmware: 22.43.2026
        * NIC: ConnectX-7 / OS: Ubuntu 22.04 / Driver: MLNX_OFED_LINUX-24.10-1.1.4.0 / Firmware: 28.43.2026
        * DPU: BlueField-2 / DOCA SW version: 2.9.1 / Firmware: 24.43.2026

    * OS/driver combinations:

        * Debian 12 with MLNX_OFED_LINUX-24.10-1.1.4.0.
        * Ubuntu 22.04 with MLNX_OFED_LINUX-24.10-1.1.4.0.
        * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-1.1.4.0.
        * Ubuntu 24.04 with rdma-core v50.0.
        * Fedora 40 with rdma-core v48.0.
        * Fedora 42 (Rawhide) with rdma-core v51.0.
        * OpenSUSE Leap 15.6 with rdma-core v49.1.

* Intel(R) Testing:

    * Basic NIC testing

        * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.10, Ubuntu22.04, Fedora40, RHEL8.10, RHEL9.4, FreeBSD14.1, SUSE15, openEuler22.03-SP2, OpenAnolis8.9 etc.
        * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
        * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
        * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
        * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

    * Basic cryptodev and virtio testing

        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev Function test: Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * Cryptodev Performance test: test scenarios including Throughput Performance/Cryptodev Latency, etc.

23.11.3 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Start dpdk-pdump in VM with virtio-0.95 protocol failed

    Fix available in upstream.

* Failed to add vdev when launch dpdk-pdump with vdev secondary process

    Fix available in upstream.

23.11.4 Release Notes
---------------------


23.11.4 Fixes
~~~~~~~~~~~~~

* app/testpmd: avoid crash in DCB config
* app/testpmd: fix out-of-bound reference in offload config
* app/testpmd: show all DCB priority TC map
* build: force GCC 15 to initialize padding bits
* buildtools: fix some Python regex syntax warnings
* bus/pci: fix registered device name
* ci: build with MSVC in GHA
* common/cnxk: fix DPI mailbox structure
* common/cnxk: fix atomic load in batch ops
* common/cnxk: fix inbound IPsec SA setup
* common/cnxk: fix null check
* common/idpf: fix void function returning a value
* crypto/cnxk: fix asymmetric operation status code
* crypto/cnxk: fix build with GCC 15
* crypto/dpaa2_sec: fix bitmask truncation
* crypto/dpaa_sec: fix bitmask truncation
* crypto/openssl: fix CMAC auth context update
* crypto/openssl: validate incorrect RSA signature
* crypto/qat: fix SM3 state size
* crypto/virtio: fix data queues iteration
* crypto/virtio: fix redundant queue free
* doc: fix feature flags for queue start/stop
* doc: update ionic driver guide
* eal/linux: fix memseg length in legacy mem init
* eal/linux: remove useless assignments
* eal/x86: fix some intrinsics header include for Windows
* eal: fix devargs layers parsing out of bounds
* eal: fix undetected NUMA nodes
* ethdev: fix functions available in new device event
* event/dpaa: fix bitmask truncation
* eventdev: fix format string data type in log messages
* examples/ipsec-secgw: fix IV length in CTR 192/256
* examples/ipsec-secgw: fix cryptodev and eventdev IDs
* examples/l3fwd: fix socket ID check
* examples/ptpclient: fix message parsing
* examples/vhost_crypto: fix user callbacks
* fix ptp
* gro: fix unprocessed IPv4 packets
* mempool: fix errno in empty create
* net/af_packet: fix socket close on device stop
* net/bnxt: fix crash when representor is re-attached
* net/bnxt: fix indication of allocation
* net/bonding: fix dedicated queue setup
* net/cnxk: fix NIX send header L3 type
* net/cpfl: fix representor parsing log
* net/dpaa2: fix bitmask truncation
* net/dpaa: fix bitmask truncation
* net/e1000/base: correct mPHY access logic
* net/e1000/base: fix MAC address hash bit shift
* net/e1000/base: fix NVM data type in bit shift
* net/e1000/base: fix bitwise operation type
* net/e1000/base: fix data type in MAC hash
* net/e1000/base: fix iterator type
* net/e1000/base: fix reset for 82580
* net/e1000/base: fix semaphore timeout value
* net/e1000/base: fix unchecked return
* net/e1000/base: fix uninitialized variable
* net/e1000/base: skip management check for 82575
* net/e1000: fix crashes in secondary processes
* net/enetfec: remove useless assignment
* net/gve: allocate Rx QPL pages using malloc
* net/hinic: fix flow type bitmask overflow
* net/hns3: fix copper port initialization
* net/hns3: fix mbuf freeing in simple Tx path
* net/hns3: fix reset timeout
* net/hns3: remove PVID info dump for VF
* net/hns3: rename RAS module
* net/iavf: check interrupt registration failure
* net/iavf: fix crash on app exit on FreeBSD
* net/iavf: fix mbuf release in Arm multi-process
* net/iavf: remove reset of Tx prepare function pointer
* net/ice: fix dropped packets when using VRRP
* net/ice: fix flow engines order
* net/ice: fix flows handling
* net/ice: fix memory leak in scalar Rx
* net/igc/base: fix LTR for i225
* net/igc/base: fix MAC address hash bit shift
* net/igc/base: fix NVM data type in bit shift
* net/igc/base: fix bitwise operation type
* net/igc/base: fix data type in MAC hash
* net/igc/base: fix deadlock when writing i225 register
* net/igc/base: fix infinite loop
* net/igc/base: fix iterator type
* net/igc/base: fix semaphore timeout value
* net/igc/base: fix typo in LTR calculation
* net/igc/base: fix unused value
* net/igc/base: increase PHY power up delay
* net/igc/base: reset loop variable
* net/intel: fix build with icx
* net/intel: fix void functions returning a value
* net/ixgbe: fix crashes in secondary processes
* net/ixgbe: fix minimum Rx/Tx descriptors
* net/mana: fix multi-process tracking
* net/mlx5/hws: fix DV FT type convert
* net/mlx5/hws: fix GTP flags matching
* net/mlx5/hws: fix crash using represented port without ID
* net/mlx5/hws: fix fragmented packet type matching
* net/mlx5: adjust actions per rule limitation
* net/mlx5: fix GRE flow match with SWS
* net/mlx5: fix GRE matching on root table
* net/mlx5: fix IPIP tunnel verification
* net/mlx5: fix LACP packet handling in isolated mode
* net/mlx5: fix Netlink socket leak
* net/mlx5: fix actions translation error overwrite
* net/mlx5: fix flush of non-template flow rules
* net/mlx5: fix hairpin queue release
* net/mlx5: fix hardware packet type translation
* net/mlx5: fix leak in HWS flow counter action
* net/mlx5: fix polling CQEs
* net/netvsc: remove device if its net devices removed
* net/netvsc: scan all net devices under the PCI device
* net/nfp: fix VF link speed problem
* net/nfp: fix misuse of function return values
* net/nfp: fix multi-PF control flag
* net/nfp: fix multiple PFs check from NSP
* net/octeon_ep: remove useless assignment
* net/qede: fix debug messages array
* net/qede: fix nested loops
* net/sfc: remove unnecessary assignment
* net/thunderx/base: fix build with GCC 15
* net/txgbe: remove useless condition for SW-FW sync
* pdump: clear statistics when enabled
* raw/cnxk_gpio: fix file descriptor leak
* stack: fix pop in C11 implementation
* test/bbdev: update FFT test vectors
* test/bonding: fix active backup receive test
* test/crypto: fix AES-ECB test lengths
* test/crypto: fix backport merge
* test/crypto: fix callback setup tests
* test/crypto: fix check for OOP header data
* test/crypto: remove unused variable
* test/dma: fix pointers in IOVA as PA mode
* test/event: fix number of queues in eventdev conf
* test/ring: fix init with custom number of lcores
* update version to v23.11.4
* use Python raw string notation
* version: 23.11.4-rc4
* version: 23.11.4-rc5
* vhost/crypto: skip fetch before vring init
* vhost: add null callback checks
* vhost: check GSO size validity
* vhost: check descriptor chains length
* vhost: clear ring addresses when getting vring base
* vhost: fix log when setting max queue num
* vhost: reset packets count when not ready

23.11.4 Validation
~~~~~~~~~~~~~~~~~~

* RedHat Testing:

    * Test scenarios:

        * VM with device assignment(PF) throughput testing(1G hugepage size)
        * VM with device assignment(PF) throughput testing(2M hugepage size)
        * VM with device assignment(VF) throughput testing
        * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
        * PVP vhost-user 2Q throughput testing
        * PVP vhost-user 1Q - cross numa node throughput testing
        * VM with vhost-user 2 queues throughput testing
        * vhost-user reconnect with dpdk-client, qemu-server(qemu reconnect)
        * vhost-user reconnect with dpdk-client, qemu-server(ovs reconnect)
        * PVP  reconnect with dpdk-client, qemu-server
        * PVP 1Q live migration testing
        * PVP 1Q cross numa node live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing (2M)
        * VM with ovs+dpdk+vhost-user 2Q live migration testing
        * VM with ovs+dpdk+vhost-user 4Q live migration testing
        * Host PF + DPDK testing
        * Host VF + DPDK testing

    * Test Versions and device:

        * RHEL 9.4
        * qemu-kvm-8.2.0
        * kernel 5.14
        * libvirt 10.0
        * openvswitch 3.3
        * X540-AT2 NIC(ixgbe, 10G)

* Nvidia(R) Testing:

    * Test scenarios:

        * Send and receive multiple types of traffic.
        * testpmd xstats counter test.
        * testpmd timestamp test.
        * Changing/checking link status through testpmd.
        * rte_flow tests (https://doc.dpdk.org/guides/nics/mlx5.html#supported-hardware-offloads)
        * RSS tests.
        * VLAN filtering, stripping, and insertion tests.
        * Checksum and TSO tests.
        * ptype tests.
        * link_status_interrupt example application tests.
        * l3fwd-power example application tests.
        * Multi-process example applications tests.
        * Hardware LRO tests.
        * Buffer Split tests.
        * Tx scheduling tests.

    * Test platform:

        * NIC: ConnectX-6 Dx / OS: Ubuntu 22.04 / Driver: MLNX_OFED_LINUX-24.10-2.1.8.0 / Firmware: 22.43.2566
        * DPU: BlueField-2 / DOCA SW version: 2.10.0 / Firmware: 24.44.1036

    * OS/driver combinations:

        * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-2.1.8.0.
        * Ubuntu 24.04 with rdma-core master (324c42e).
        * Ubuntu 24.04 with rdma-core v50.0.
        * Fedora 41 with rdma-core v51.0.
        * Fedora 43 (Rawhide) with rdma-core v56.0.
        * OpenSUSE Leap 15.6 with rdma-core v49.1.
        * Windows Server 2022 with Clang 16.0.6.

* Intel(R) Testing:

    * Basic NIC testing

        * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.10, Ubuntu22.04.1, Fedora41, RHEL9.4, RHEL9.5, RHEL10.0 beta, FreeBSD14.2, SUSE15.6, AzureLinux3.0, OpenAnolis8.9 etc.
        * i40E-(XXV710, X722) PF/VF: test scenarios including basic/RTE_FLOW/TSO/Jumboframe/checksum offload/mac_filter/VLAN/VXLAN/RSS, etc.
        * IXGBE-(82599) PF/VF: test scenarios including basic/RTE_FLOW/TSO/Jumboframe/checksum offload/mac_filter/VLAN/VXLAN/RSS, etc.
        * ICE-(E810, E2100) PF/VF: test scenarios including basic/Switch/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
        * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.
        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * DLB: test scenarios including DLB2.0 and DLB2.5
        * Other: test scenarios including AF_XDP, Power, CBDMA, DSA

    * Basic cryptodev and virtio testing

        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev Function test: Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * Cryptodev Performance test: test scenarios including Throughput Performance/Cryptodev Latency, etc.

    * Performance test

        * Throughput Performance
        * Cryptodev Latency
        * PF/VF NIC single core
        * XXV710/E810 NIC Performance

23.11.4 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Start dpdk-pdump in VM with virtio-0.95 protocol failed

    Fix available in upstream.

* compilation failed with gcc 15.0.1

23.11.5 Release Notes
---------------------


23.11.5 Fixes
~~~~~~~~~~~~~

* Revert "net/ice: fix inconsistency in Rx queue VLAN tag placement"
* acl: fix build with GCC 15 on aarch64
* app/crypto-perf: fix AAD offset alignment
* app/eventdev: fix number of releases sent during cleanup
* app/testpmd: fix RSS hash key update
* app/testpmd: relax number of TCs in DCB command
* buildtools/test: scan muti-line registrations
* bus/auxiliary: fix crash in cleanup
* bus/fslmc: fix use after free
* bus/pci/bsd: fix device existence check
* bus/vmbus: align ring buffer data to page boundary
* bus/vmbus: use Hyper-V page size
* bus: cleanup device lists
* common/cnxk: fix CQ tail drop
* common/cnxk: fix E-tag pattern parsing
* common/cnxk: fix null pointer checks
* common/cnxk: fix qsize in CPT iq enable
* common/dpaax: fix PDCP AES only 12-bit SN
* common/dpaax: fix PDCP key command race condition
* common/mlx5: fix extraction of auxiliary device name
* config/arm: add grace build config
* crypto/dpaa2_sec: fix uninitialized variable
* crypto/openssl: include private exponent in RSA session
* crypto/qat: fix out-of-place chain/cipher/auth headers
* crypto/qat: fix out-of-place header bytes in AEAD raw API
* crypto/qat: fix size calculation for memset
* crypto/virtio: add request check on request side
* crypto/virtio: fix cipher data source length
* crypto/virtio: fix driver ID
* crypto/virtio: fix driver cleanup
* doc: add kernel options required for mlx5
* doc: remove reference to deprecated --use-device option
* dts: fix deterministic doc
* eal/freebsd: unregister alarm callback before free
* eal/linux: improve ASLR check
* eal/linux: unregister alarm callback before free
* eal/unix: fix log message for madvise failure
* eal: add description of service corelist in usage
* eal: fix return value of lcore role
* eal: warn if no lcore is available
* ethdev: convert string initialization
* ethdev: fix error struct in flow configure
* ethdev: keep promiscuous/allmulti value before disabling
* event/dlb2: fix QID depth xstat
* event/dlb2: fix default credits based on HW version
* event/dlb2: fix dequeue with CQ depth <= 16
* event/dlb2: fix num single link ports for DLB2.5
* event/dlb2: fix public symbol namespace
* event/dlb2: fix validaton of LDB port COS ID arguments
* examples/flow_filtering: fix make clean
* examples/ipsec-secgw: fix crash in event vector mode
* examples/ipsec-secgw: fix crash with IPv6
* examples/ipsec-secgw: fix number of queue pairs
* examples/multi_process: fix ports cleanup on exit
* examples/multi_process: revert ports cleanup on exit
* examples/ntb: check more heap allocations
* latencystats: fix receive sample race
* mem: fix lockup on address space shortage
* net/af_xdp: fix use after free in zero-copy Tx
* net/bonding: avoid RSS RETA update in flow isolation mode
* net/cnxk: fix descriptor count update on reconfig
* net/e1000: fix EEPROM dump
* net/e1000: fix igb Tx queue offloads capability
* net/e1000: fix xstats name
* net/fm10k/base: fix compilation warnings
* net/hns3: allow Rx vector mode with VLAN filter
* net/hns3: allow Tx vector when fast free not enabled
* net/hns3: check requirement for hardware GRO
* net/hns3: fix CRC data segment
* net/hns3: fix Rx packet without CRC data
* net/hns3: fix divide by zero
* net/hns3: fix extra wait for link up
* net/hns3: fix integer overflow in interrupt unmap
* net/hns3: fix interrupt rollback
* net/hns3: fix memory leak for indirect flow action
* net/hns3: fix memory leak on failure
* net/hns3: fix queue TC configuration on VF
* net/hns3: fix resources release on reset
* net/i40e/base: fix compiler warnings
* net/i40e/base: fix unused value warnings
* net/i40e: fix RSS on plain IPv4
* net/iavf: fix VLAN strip setting after enabling filter
* net/ice/base: fix integer overflow
* net/ice/base: fix typo in device ID description
* net/ice: fix flow creation failure
* net/ice: fix handling empty DCF RSS hash
* net/ice: fix inconsistency in Rx queue VLAN tag placement
* net/ice: fix querying RSS hash for DCF
* net/ixgbe/base: correct definition of endianness macro
* net/ixgbe/base: fix compilation warnings
* net/ixgbe: enable ethertype filter for E610
* net/ixgbe: fix indentation
* net/ixgbe: fix port mask default value in filter
* net/mana: check vendor ID when probing RDMA device
* net/mlx5/hws: fix send queue drain on FW WQE destroy
* net/mlx5: align PF and VF/SF MAC address handling
* net/mlx5: avoid setting kernel MTU if not needed
* net/mlx5: fix VLAN stripping on hairpin queue
* net/mlx5: fix WQE size calculation for Tx queue
* net/mlx5: fix counter service cleanup on init failure
* net/mlx5: fix crash in HWS counter pool destroy
* net/mlx5: fix crash on age query with indirect conntrack
* net/mlx5: fix header modify action on group 0
* net/mlx5: fix hypervisor detection in VLAN workaround
* net/mlx5: fix mark action with shared Rx queue
* net/mlx5: fix masked indirect age action validation
* net/mlx5: fix maximal queue size query
* net/mlx5: fix out-of-order completions in ordinary Rx burst
* net/mlx5: remove unsupported flow meter action in HWS
* net/mlx5: validate GTP PSC QFI width
* net/netvsc: add stats counters from VF
* net/netvsc: use Hyper-V page size
* net/nfp: fix crash with null RSS hash key
* net/nfp: fix hash key length logic
* net/nfp: standardize NFD3 Tx descriptor endianness
* net/nfp: standardize NFDk Tx descriptor endianness
* net/ngbe: fix MAC control frame forwarding
* net/ngbe: fix device statistics
* net/null: fix packet copy
* net/octeon_ep: increase mailbox timeout
* net/qede: fix use after free
* net/sfc: fix action order on start failure
* net/tap: fix qdisc add failure handling
* net/txgbe: add LRO flag in mbuf when enabled
* net/txgbe: fix MAC control frame forwarding
* net/txgbe: fix device statistics
* net/txgbe: fix ntuple filter parsing
* net/txgbe: fix packet type for FDIR filter
* net/txgbe: fix raw pattern match for FDIR rule
* net/txgbe: fix reserved extra FDIR headroom
* net/txgbe: fix to create FDIR filter for SCTP packet
* net/txgbe: restrict VLAN strip configuration on VF
* net/virtio: fix check of threshold for Tx freeing
* net/virtio: revert Tx free threshold fix
* power/intel_uncore: fix crash closing uninitialized driver
* test/crypto: fix RSA decrypt validation
* test/crypto: fix auth and cipher case IV length
* test/crypto: set to null after freeing operation
* test/lcore: fix race in per-lcore test
* test/malloc: improve resiliency
* trace: fix overflow in per-lcore trace buffer
* version: 23.11.5-rc1
* vhost/crypto: fix cipher data length
* vhost: fix net control virtqueue used length
* vhost: fix wrapping on control virtqueue rings
* vhost: search virtqueues driver data in read-only area

23.11.5 Validation
~~~~~~~~~~~~~~~~~~

* Nvidia(R) Testing:

    * Test scenarios:

        * Send and receive multiple types of traffic.
        * testpmd xstats counter test.
        * testpmd timestamp test.
        * Changing/checking link status through testpmd.
        * rte_flow tests (https://doc.dpdk.org/guides/nics/mlx5.html#supported-hardware-offloads)
        * RSS tests.
        * VLAN filtering, stripping, and insertion tests.
        * Checksum and TSO tests.
        * ptype tests.
        * link_status_interrupt example application tests.
        * l3fwd-power example application tests.
        * Multi-process example applications tests.
        * Hardware LRO tests.
        * Buffer Split tests.
        * Tx scheduling tests.

    * Test platform:

        * NIC: ConnectX-6 Dx / OS: Ubuntu 22.04 / Driver: MLNX_OFED_LINUX-23.10-5.1.4.0 / Firmware: 22.39.5050
        * NIC: ConnectX-7 / OS: Ubuntu 22.04 / Driver: MLNX_OFED_LINUX-23.10-5.1.4.0 / Firmware: 28.39.5050
        * DPU: BlueField-2 / DOCA SW version: 2.5.4 / Firmware: 24.39.5050

    * OS/driver build:

        * Debian 12 with MLNX_OFED_LINUX-24.10-3.2.5.0.
        * Ubuntu 22.04 with MLNX_OFED_LINUX-24.10-3.2.5.0.
        * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-3.2.5.0.
        * Ubuntu 22.04 with rdma-core master (091ddb5).
        * Ubuntu 24.04 with rdma-core v50.0.
        * Fedora 42 with rdma-core v55.0.
        * Fedora 43 (Rawhide) with rdma-core v58.0.
        * OpenSUSE Leap 15.6 with rdma-core v49.1.
        * Windows Server 2022 with Clang 18.1.8.

* RedHat Testing:

    * Test scenarios:

        * VM with device assignment(PF) throughput testing(1G hugepage size)
        * VM with device assignment(PF) throughput testing(2M hugepage size)
        * VM with device assignment(VF) throughput testing
        * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
        * PVP vhost-user 2Q throughput testing
        * PVP vhost-user 1Q - cross numa node throughput testing
        * VM with vhost-user 2 queues throughput testing
        * vhost-user reconnect with dpdk-client, qemu-server(qemu reconnect)
        * vhost-user reconnect with dpdk-client, qemu-server(ovs reconnect)
        * PVP  reconnect with dpdk-client, qemu-server
        * PVP 1Q live migration testing
        * PVP 1Q cross numa node live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing
        * VM with ovs+dpdk+vhost-user 1Q live migration testing (2M)
        * VM with ovs+dpdk+vhost-user 2Q live migration testing
        * VM with ovs+dpdk+vhost-user 4Q live migration testing
        * Host PF + DPDK testing
        * Host VF + DPDK testing

    * Test Versions and device:

        * RHEL 10.0
        * qemu-kvm-9.1.0
        * kernel 6.12
        * libvirt 10.10
        * openvswitch 3.3
        * X540-AT2 NIC(ixgbe, 10G)

* Intel(R) Testing:

    * Basic NIC testing

        * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu25.04/Ubuntu24.04.2, RHEL9.6/RHEL10, Fedora42, FreeBSD14.2, SUSE15.6, OpenAnolis8.10, AzureLinux3.0 etc.
        * i40E-(XXV710, X722) PF/VF: test scenarios including basic/RTE_FLOW/TSO/Jumboframe/checksum offload/mac_filter/VLAN/VXLAN/RSS, etc.
        * IXGBE-(82599) PF/VF: test scenarios including basic/RTE_FLOW/TSO/Jumboframe/checksum offload/mac_filter/VLAN/VXLAN/RSS, etc.
        * ICE-(E810, E2100) PF/VF: test scenarios including basic/Switch/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
        * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.
        * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
        * Cryptodev: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
        * DLB: test scenarios including DLB2.0 and DLB2.5
        * Other: test scenarios including AF_XDP, Power, CBDMA, DSA

    * Performance test

        * Throughput Performance
        * Cryptodev Latency
        * PF/VF NIC single core
        * XXV710/E810 NIC Performance

23.11.5 Known Issues
~~~~~~~~~~~~~~~~~~~~

* compilation failed with gcc 15.0.1
