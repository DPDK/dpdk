.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.11
==================

New Features
------------

* **Added initial LoongArch architecture support.**

  Added EAL implementation for LoongArch architecture.
  The initial port was tested on Loongson 3A5000,
  Loongson 3C5000 and Loongson 3C5000L.
  In theory this implementation should work with any target based on
  ``LoongArch`` ISA.

* **Added support for multiple mbuf pools per ethdev Rx queue.**

  Added a capability which allows an application to provide many mempools
  of different size, and PMDs and/or NICs to choose a memory pool
  based on the packet's length and/or Rx buffer availability.

* **Added support for congestion management in ethdev.**

  Added new API functions ``rte_eth_cman_config_init()``,
  ``rte_eth_cman_config_get()``, ``rte_eth_cman_config_set()``
  and ``rte_eth_cman_info_get()`` to support congestion management.

* **Added protocol header based buffer split.**

  * Added ``rte_eth_buffer_split_get_supported_hdr_ptypes()`` to get supported
    header protocols to split at.
  * Added support for protocol-based buffer split using new ``proto_hdr``
    field in structure ``rte_eth_rxseg_split``.

* **Added proactive error handling mode for ethdev.**

  Added proactive error handling mode for ethdev,
  and introduced three new events: ``RTE_ETH_EVENT_ERR_RECOVERING``
  to report that the port is recovering from an error,
  ``RTE_ETH_EVENT_RECOVER_SUCCESS`` and ``RTE_ETH_EVENT_RECOVER_FAILED``.

* **Added ethdev Rx/Tx descriptor dump API.**

  Added the ethdev Rx/Tx descriptor dump API which provides functions
  for querying descriptor from device.
  The descriptor information differs in different NICs.
  The information demonstrates I/O process which is important for debug.
  The dump format is vendor-specific.

* **Added ethdev hairpin memory configuration options.**

  Added new configuration flags for hairpin queues in ``rte_eth_hairpin_conf``:

  * ``use_locked_device_memory``
  * ``use_rte_memory``
  * ``force_memory``

  Each flag has a corresponding capability flag
  in ``struct rte_eth_hairpin_queue_cap``.

* **Added strict queue to pre-configuration flow hints.**

  * Added flags option to ``rte_flow_configure`` and ``rte_flow_info_get``.
  * Added ``RTE_FLOW_PORT_FLAG_STRICT_QUEUE`` flag to indicate all operations
    for a given flow rule will strictly happen on the same flow queue.

* **Added configuration for asynchronous flow connection tracking.**

  Added connection tracking action number hint to ``rte_flow_configure``
  and ``rte_flow_info_get``.
  The PMD can prepare the connection tracking resources according to the hint.

* **Added support for queue-based async query in flow API.**

  Added new function ``rte_flow_async_action_handle_query()``
  to query the action asynchronously.

* **Extended metering and marking support in the flow API.**

  * Added ``METER_COLOR`` item to match color marker set by a meter.
  * Added ability to set color marker via modify field flow API.
  * Added meter API to get a pointer to the profile/policy by their ID.
  * Added ``METER_MARK`` action for metering with lockless profile/policy access.

* **Added flow offload action to route packets to kernel.**

  Added new flow action which allows an application to re-route packets
  directly to the kernel without software involvement.

* **Updated AF_XDP driver.**

  * Updated AF_XDP driver to make it compatible with libbpf v0.8.0
    (when used with libxdp).

* **Updated AMD Pensando ionic driver.**

  * Updated ionic driver to reflect that Pensando has been acquired by AMD.
  * Enhanced data path to provide substantial performance improvements.
  * Added support for mbuf fast free.
  * Added support for advertising packet types.
  * Added support for descriptor status functions.
  * Added Q-in-CMB feature controlled by device option ``ionic_cmb``.
  * Added optimized handlers for non-scattered Rx and Tx.

* **Added GVE net PMD.**

  * Added the new ``gve`` net driver for Google Virtual Ethernet devices.
  * See the :doc:`../nics/gve` NIC guide for more details on this new driver.

* **Updated Intel iavf driver.**

  * Added flow subscription support.

* **Updated Intel ice driver.**

  * Added protocol based buffer split support in scalar path.

* **Added Intel idpf driver.**

  Added the new ``idpf`` net driver
  for Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100.
  See the :doc:`../nics/idpf` NIC guide for more details on this new driver.

* **Updated Marvell cnxk driver.**

  * Added support for flow action ``REPRESENTED_PORT``.
  * Added support for congestion management.

* **Added Microsoft mana driver.**

  The driver has been disabled by default because of a, currently, missing dependency.

* **Updated Netronome nfp driver.**

  Added flow API support:

  * Added support for the flower firmware.
  * Added the flower service infrastructure.
  * Added the control message interactive channels between PMD and firmware.
  * Added support for a representor port.

* **Updated NVIDIA mlx5 driver.**

  * Added full support for queue-based async hardware steering.

    - Support of FDB.
    - Support of control flow and isolate mode.
    - Support of conntrack.
    - Support of counter.
    - Support of aging.
    - Support of meter.
    - Support of modify fields.

* **Updated NXP dpaa2 driver.**

  * Added support for flow action ``REPRESENTED_PORT``.

* **Updated Wangxun ngbe driver.**

  * Added support to set device link down/up.

* **Added DMA vChannel unconfiguration for async vhost.**

  Added support to unconfigure DMA vChannel that is no longer used
  by the vhost library.

* **Added non-blocking notify API to vhost library.**

  Added ``rte_vhost_vring_call_nonblock`` API to notify the guest that
  used descriptors have been added to the vring in n aon-blocking way.
  The user should check the return value of this API and try again if needed.

* **Added support for MACsec in rte_security.**

  Added MACsec transform for rte_security session and added new API
  to configure security associations (SA) and secure channels (SC).

* **Added new algorithms to cryptodev.**

  * Added symmetric hash algorithm ShangMi 3 (SM3).
  * Added symmetric cipher algorithm ShangMi 4 (SM4) in ECB, CBC and CTR modes.

* **Updated Intel QuickAssist Technology (QAT) crypto driver.**

  * Added support for SM3 hash algorithm.
  * Added support for SM4 encryption algorithm in ECB, CBC and CTR modes.
  * Added support for ECDH key exchange algorithm.

* **Updated Marvell cnxk crypto driver.**

  * Added AES-CCM support in lookaside protocol (IPsec) for CN9K and  CN10K.
  * Added AES & DES DOCSIS algorithm support in lookaside crypto for CN9K.

* **Updated aesni_mb crypto driver.**

  * Added support for 8-byte and 16-byte tags for ZUC-EIA3-256.
  * Added support for in-place SGL, out-of-place SGL in SGL out,
    out-of-place LB in SGL out, and out-of-place SGL in LB out.

* **Updated ipsec_mb crypto driver.**

  * Added SNOW-3G and ZUC support for ARM platform.
  * Added Intel IPsec MB v1.3 library support for x86 platform.

* **Added UADK crypto driver.**

  Added a new crypto driver for the UADK library. See the
  :doc:`../cryptodevs/uadk` guide for more details on this new driver.

* **Added bbdev operation for FFT processing.**

  Added a new operation type in bbdev for FFT processing with new functions
  ``rte_bbdev_enqueue_fft_ops`` and ``rte_bbdev_dequeue_fft_ops``,
  and related structures.

* **Added Intel ACC200 bbdev driver.**

  Added a new ``acc200`` bbdev driver for the Intel\ |reg| ACC200 accelerator
  integrated on SPR-EE.  See the
  :doc:`../bbdevs/acc200` guide for more details on this new driver.

* **Added eventdev adapter instance get API.**

  * Added ``rte_event_eth_rx_adapter_instance_get`` to get Rx adapter
    instance ID for specified ethernet device ID and Rx queue index.

  * Added ``rte_event_eth_tx_adapter_instance_get`` to get Tx adapter
    instance ID for specified ethernet device ID and Tx queue index.

* **Added eventdev Tx adapter queue start/stop API.**

  * Added ``rte_event_eth_tx_adapter_queue_start`` to start
    enqueueing packets to the Tx queue by Tx adapter.
  * Added ``rte_event_eth_tx_adapter_queue_stop`` to stop the Tx Adapter
    from enqueueing any packets to the Tx queue.

* **Added event crypto adapter vectorization support.**

  Added support for aggregating crypto operations processed by event crypto adapter
  into a single event containing ``rte_event_vector``
  whose event type is ``RTE_EVENT_TYPE_CRYPTODEV_VECTOR``.

* **Added NitroSketch in membership library.**

  Added a new data structure called sketch into the membership library,
  to profile the traffic efficiently.
  NitroSketch provides high-fidelity approximate measurements
  and appears as a promising alternative to traditional approaches
  such as packet sampling.

* **Added Intel uncore frequency control API to the power library.**

  Added API to allow uncore frequency adjustment.
  This is done through manipulating related uncore frequency control
  sysfs entries to adjust the minimum and maximum uncore frequency values,
  which works on Linux with Intel hardware only.

* **Added security performance test application.**

  Added new application to test ``rte_security`` session create/destroy
  performance.
  See the :doc:`../tools/securityperf` for more details.

* **Updated IPsec sample application.**

  Added support for lookaside sessions in event mode.
  See the :doc:`../sample_app_ug/ipsec_secgw` for more details.

* **Updated FIPS validation sample application.**

  Added support for asymmetric crypto algorithms.
  See the :doc:`../sample_app_ug/fips_validation` for more details.

* **Rewrote pmdinfo script.**

  The ``dpdk-pmdinfo.py`` script was rewritten to produce valid JSON only.
  PCI-IDs parsing has been removed.
  To get a similar output to the (now removed) ``-r/--raw`` flag,
  the following command may be used:

  .. code-block:: sh

     strings $dpdk_binary_or_driver | sed -n 's/^PMD_INFO_STRING= //p'


Removed Items
-------------

* mem: Removed not implemented and deprecated ``rte_malloc_set_limit``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT`` instead.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_VF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT`` instead.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT`` instead.

* ethdev: removed ``RTE_FLOW_ACTION_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT`` instead.

* ethdev: removed ``OF_SET_MPLS_TTL``, ``OF_DEC_MPLS_TTL``,
  ``OF_SET_NW_TTL``, ``OF_COPY_TTL_OUT`` and ``OF_COPY_TTL_IN``
  which are not actually supported by any PMD.
  ``MODIFY_FIELD`` action should be used to do packet edits via flow API.

* vhost: Removed deprecated ``rte_vhost_gpa_to_vva`` and
  ``rte_vhost_get_queue_num`` helpers.


API Changes
-----------

* eal: ``RTE_FUNC_PTR_OR_*`` macros have been marked deprecated and will be removed
  in the future. Applications can use ``devtools/cocci/func_or_ret.cocci``
  to update their code.

* eal: Updated ``rte_eal_remote_launch`` so it returns ``-EPIPE`` in case of
  a read or write error on the pipe, instead of calling ``rte_panic``.

* eal: Updated return types for ``rte_{bsf,fls}`` inline functions
  to be consistently ``uint32_t``.

* mempool: Deprecated helper macro ``MEMPOOL_HEADER_SIZE()`` has been removed.
  The replacement macro ``RTE_MEMPOOL_HEADER_SIZE()`` is internal only.

* mempool: Deprecated macro to register mempool driver
  ``MEMPOOL_REGISTER_OPS()`` has been removed. Use replacement macro
  ``RTE_MEMPOOL_REGISTER_OPS()`` instead.

* mempool: Deprecated macros ``MEMPOOL_PG_NUM_DEFAULT`` and
  ``MEMPOOL_PG_SHIFT_MAX`` have been removed. These macros are not used and
  not required any more.

* mbuf: Removed deprecated ``PKT_*`` flags.
  Use corresponding flags with ``RTE_MBUF_F_`` prefix instead.
  Applications can use ``devtools/cocci/prefix_mbuf_offload_flags.cocci``
  to replace all occurrences of old mbuf flags in C code.

* bus: Changed the device numa node to -1 when NUMA information is unavailable.
  The ``dev->device.numa_node`` field is set by each bus driver for
  every device it manages to indicate on which NUMA node this device lies.
  When this information is unknown, the assigned value was not consistent
  across the bus drivers. This similarly impacts ``rte_eth_dev_socket_id()``.

* bus: Registering a bus has been marked as an internal API.
  External users may still register their bus using the ``bus_driver.h``
  driver header (see ``enable_driver_sdk`` meson option).
  The ``rte_bus`` object is now opaque and must be manipulated through added
  accessors.

* drivers: Registering a driver on the ``auxiliary``, ``ifpga``, ``pci``,
  ``vdev``, ``vmbus`` buses has been marked as an internal API.
  External users may still register their driver using the associated driver
  headers (see ``enable_driver_sdk`` meson option).
  The ``rte_driver`` and ``rte_device`` objects are now opaque and must be
  manipulated through added accessors.

* ethdev: Removed deprecated macros. Applications can use ``devtools/cocci/namespace_ethdev.cocci``
  to update their code.

  * Removed deprecated ``ETH_LINK_SPEED_*``, ``ETH_SPEED_NUM_*`` and ``ETH_LINK_*``
    (duplex-related) defines.  Use corresponding defines with ``RTE_`` prefix
    instead.

  * Removed deprecated ``ETH_MQ_RX_*`` and ``ETH_MQ_TX_*`` defines.
    Use corresponding defines with ``RTE_`` prefix instead.

  * Removed deprecated ``ETH_RSS_*`` defines for hash function and
    RETA size specification. Use corresponding defines with ``RTE_`` prefix
    instead.

  * Removed deprecated ``DEV_RX_OFFLOAD_*`` and ``DEV_TX_OFFLOAD_``
    defines. Use corresponding defines with ``RTE_ETH_RX_OFFLOAD_`` and
    ``RTE_ETH_TX_OFFLOAD_`` prefix instead.

  * Removed deprecated ``ETH_DCB_*``, ``ETH_VMDQ_``, ``ETH_*_TCS``,
    ``ETH_*_POOLS`` and ``ETH_MAX_VMDQ_POOL`` defines. Use corresponding
    defines with ``RTE_`` prefix instead.

  * Removed deprecated ``RTE_TUNNEL_*`` defines. Use corresponding
    defines with ``RTE_ETH_TUNNEL_`` prefix instead.

  * Removed deprecated ``RTE_FC_*`` defines. Use corresponding
    defines with ``RTE_ETH_FC_`` prefix instead.

  * Removed deprecated ``ETH_VLAN_*`` and ``ETH_QINQ_`` defines.
    Use corresponding defines with ``RTE_`` prefix instead.

  * Removed deprecated ``ETH_NUM_RECEIVE_MAC_ADDR`` define.
    Use corresponding define with ``RTE_`` prefix instead.

  * Removed deprecated ``PKT_{R,T}X_DYNF_METADATA`` defines.
    Use corresponding defines ``RTE_MBUF_DYNFLAG_{R,T}X_METADATA`` instead.

* ethdev: Removed deprecated Flow Director configuration from device
  configuration (``dev_conf.fdir_conf``). Moved corresponding structures
  to internal API since some drivers still use it internally.

* ethdev: Removed the Rx offload flag ``RTE_ETH_RX_OFFLOAD_HEADER_SPLIT``
  and field ``split_hdr_size`` from the structure ``rte_eth_rxmode``
  used to configure header split.
  Instead, user can still use ``RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT``
  for per-queue packet split offload,
  which is configured by ``rte_eth_rxseg_split``.

* ethdev: The ``reserved`` field in the ``rte_eth_rxseg_split`` structure is
  replaced with ``proto_hdr`` to support protocol header based buffer split.
  User can choose length or protocol header to configure buffer split
  according to NIC's capability.

* ethdev: Changed the type of the parameter ``rate`` of the function
  ``rte_eth_set_queue_rate_limit()`` from ``uint16_t`` to ``uint32_t``
  to support more than 64 Gbps.
  Changed the type of the parameter ``tx_rate`` of the functions
  ``rte_pmd_bnxt_set_vf_rate_limit()`` and
  ``rte_pmd_ixgbe_set_vf_rate_limit()`` in the same way for consistency.

* ethdev: Promoted ``rte_eth_rx_metadata_negotiate()``
  from experimental to stable.

* ethdev: Promoted the following flow primitives
  from experimental to stable:

  - ``RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR``
  - ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT``
  - ``RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR``
  - ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``

* ethdev: Promoted ``rte_flow_pick_transfer_proxy()``
  from experimental to stable.

* ethdev: Banned the use of attributes ``ingress``/``egress`` in "transfer"
  flows, as the final step of the deprecation process that had been started
  in DPDK 21.11. See items ``PORT_REPRESENTOR``, ``REPRESENTED_PORT``.

* vhost: Promoted ``rte_vhost_vring_stats_get()``,
  ``rte_vhost_vring_stats_get_names()`` and ``rte_vhost_vring_stats_reset()``
  from experimental to stable.

* cryptodev: The structure ``rte_cryptodev_sym_session`` was made internal.
  The API ``rte_cryptodev_sym_session_init`` and ``rte_cryptodev_sym_session_clear``
  were removed and user would only need to call ``rte_cryptodev_sym_session_create``
  and ``rte_cryptodev_sym_session_free`` to create/destroy sessions.
  The API ``rte_cryptodev_sym_session_create`` was updated to take a single mempool
  with element size big enough to hold session data and session private data.
  All sample applications were updated to attach an opaque pointer for the session
  to the ``rte_crypto_op`` while enqueuing.

* security: The structure ``rte_security_session`` was made internal
  and corresponding functions were updated to take/return an opaque session pointer.
  The API ``rte_security_session_create`` was updated to take only one mempool
  which has enough space to hold session and driver private data.

* security: MACsec support has been added which resulted in updates
  to structures ``rte_security_macsec_xform``, ``rte_security_macsec_stats``
  and security capability structure ``rte_security_capability``
  to accommodate MACsec capabilities.

* security: The experimental API ``rte_security_get_userdata`` was being unused
  by most of the drivers and it was retrieving userdata from mbuf dynamic field.
  The API is now removed and the application can directly get the userdata from
  mbuf dynamic field.

* eventdev: The function ``rte_event_crypto_adapter_queue_pair_add`` was updated
  to accept configuration of type ``rte_event_crypto_adapter_queue_conf``
  instead of ``rte_event``,
  similar to ``rte_event_eth_rx_adapter_queue_add`` signature.
  Event will be one of the configuration fields,
  together with additional vector parameters.

* eventdev: The function pointer definition ``eventdev_stop_flush_t``
  is renamed to ``rte_eventdev_stop_flush_t``
  to avoid conflicts with application symbols.

* eventdev: The data type of the ID parameter in the functions
  ``rte_event_dev_xstats_names_get``, ``rte_event_dev_xstats_get``,
  ``rte_event_dev_xstats_by_name_get`` and ``rte_event_dev_xstats_reset``
  is changed to ``uint64_t`` from ``unsigned int`` and ``uint32_t``.

* metrics: Updated ``rte_metrics_init`` so it returns an error code instead
  of calling ``rte_exit``.

* telemetry: The allowed characters in names for dictionary values
  are now limited to alphanumeric characters and a small subset of additional
  printable characters.
  This will ensure that all dictionary parameter names can be output
  without escaping in JSON - or in any future output format used.
  Names for the telemetry commands are now similarly limited.
  The parameters for telemetry commands are unaffected by this change.

* raw/ifgpa: The function ``rte_pmd_ifpga_get_pci_bus`` has been removed.


ABI Changes
-----------

* eal: Updated EAL thread names from ``lcore-worker-<lcore_id>`` to
  ``rte-worker-<lcore_id>`` so that DPDK can accommodate lcores higher than 99.

* mbuf: Replaced ``buf_iova`` field with ``next`` field and added a new field
  ``dynfield2`` at its place in second cacheline if ``RTE_IOVA_AS_PA`` is 0.

* ethdev: enum ``RTE_FLOW_ITEM`` was affected by deprecation procedure.

* ethdev: enum ``RTE_FLOW_ACTION`` was affected by deprecation procedure.

* bbdev: enum ``rte_bbdev_op_type`` was affected to remove ``RTE_BBDEV_OP_TYPE_COUNT``
  and to allow for futureproof enum insertion a padded ``RTE_BBDEV_OP_TYPE_SIZE_MAX``
  macro is added.

* bbdev: Structure ``rte_bbdev_driver_info`` was updated to add new parameters
  for queue topology, device status using ``rte_bbdev_device_status``.

* bbdev: Structure ``rte_bbdev_queue_data`` was updated to add new parameter
  for enqueue status using ``rte_bbdev_enqueue_status``.

* eventdev: Added ``evtim_drop_count`` field
  to ``rte_event_timer_adapter_stats`` structure.

* eventdev: Added ``weight`` and ``affinity`` fields
  to ``rte_event_queue_conf`` structure.

* eventdev: The field ``*u64s`` in the structure ``rte_event_vector`` is replaced
  with ``u64s`` as the field is supposed to hold an array of ``uint64_t`` values.

* eventdev: The structure ``rte_event_vector`` was updated to include a new bit
  field ``elem_offset:12``. The bits are taken from the bitfield ``rsvd:15``.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180M CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz

  * OS:

    * Fedora 36
    * FreeBSD 13.1
    * Red Hat Enterprise Linux Server release 8.6
    * Red Hat Enterprise Linux Server release 9
    * CentOS 7.9
    * Ubuntu 20.04.5
    * Ubuntu 22.04.1
    * Ubuntu 22.10
    * SUSE Linux Enterprise Server 15 SP4

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.10 0x800151d8 1.3310.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.10.6 (ice)
      * Driver version(in-tree): 5.15.0-46-generic / 4.18.0-372.9.1.rt7.166.el8.x86_64 (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0
      * Wireless Edge DDP: 1.3.10.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.10 0x8001518e 1.3310.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.10.6 (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0
      * Wireless Edge DDP: 1.3.10.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.10 0x80015188 1.3310.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.10.6 (ice)
      * OS Default DDP: 1.3.30.0
      * COMMS DDP: 1.3.37.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.16.5 (ixgbe)
      * Driver version(in-tree): 5.15.0-46-generic (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.00 0x8000cead 1.3179.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.20.12 (i40e)
      * Driver version(in-tree): 5.15.0-46-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.00 0x800039ec 1.3179.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.20.12 (i40e)
      * Driver version(in-tree): 5.15.0-46-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T

      * Firmware version: 6.00 0x800039aa 1.2935.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version(out-tree): 2.20.12 (i40e)
      * Driver version(in-tree): 5.15.0-46-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 9.00 0x8000ce90 1.3179.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.20.12 (i40e)
      * Driver version(in-tree): 5.15.0-46-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.00 0x8000ce86 1.3179.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.20.12 (i40e)
      * Driver version(in-tree): 5.15.0-46-generic (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 9.00 0x8000ce67 1.3179.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.20.12 (i40e)

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

    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Red Hat Enterprise Linux release 8.4 (Ootpa)
    * Red Hat Enterprise Linux release 8.2 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.9 (Maipo)
    * Red Hat Enterprise Linux Server release 7.8 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Ubuntu 22.04
    * Ubuntu 20.04
    * Ubuntu 18.04
    * SUSE Enterprise Linux 15 SP2

  * OFED:

    * MLNX_OFED 5.8-1.0.1.1 and above
    * MLNX_OFED 5.7-1.0.2.0

  * upstream kernel:

    * Linux 6.1.0-rc3 and above

  * rdma-core:

    * rdma-core-43.0 and above

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
      * Firmware version: 14.32.1010 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.32.1010 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.35.1012 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.35.1012 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.35.1012 and above

  * Embedded software:

    * Ubuntu 20.04.3
    * MLNX_OFED 5.8-1.0.1.1 and above
    * DOCA 1.5 with BlueField 3.9.3
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.35.1012

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.35.1012

  * OFED:

    * MLNX_OFED 5.8-1.0.1.1

22.11.1 Release Notes
---------------------


22.11.1 Fixes
~~~~~~~~~~~~~

* drivers: fix symbol exports when map is omitted

22.11.2 Release Notes
---------------------


22.11.2 Fixes
~~~~~~~~~~~~~

* acl: fix crash on PPC64 with GCC 11
* app/bbdev: add allocation checks
* app/bbdev: check statistics failure
* app/bbdev: fix build with optional flag
* app/bbdev: fix build with optional flag
* app/compress-perf: fix remaining data for ops
* app/compress-perf: fix some typos
* app/compress-perf: fix testing single operation
* app/crypto-perf: fix IPsec direction
* app/crypto-perf: fix number of segments
* app/crypto-perf: fix session freeing
* app/crypto-perf: fix SPI zero
* app/crypto-perf: fix test file memory leak
* app/dumpcap: fix storing port identifier
* app/flow-perf: fix division or module by zero
* app/testpmd: cleanup cleanly from signal
* app/testpmd: fix crash on cleanup
* app/testpmd: fix encap/decap size calculation
* app/testpmd: fix forwarding stats for Tx dropped
* app/testpmd: fix interactive mode on Windows
* app/testpmd: fix interactive mode with no ports
* app/testpmd: fix link check condition on port start
* app/testpmd: fix packet count in IEEE 1588 engine
* app/testpmd: fix packet transmission in noisy VNF engine
* app/testpmd: fix secondary process packet forwarding
* app/testpmd: fix Tx preparation in checksum engine
* baseband/acc: add explicit mbuf append for soft output
* baseband/acc: fix acc100 iteration counter in TB
* baseband/acc: fix acc100 queue mapping to 64 bits
* baseband/acc: fix check after deref and dead code
* baseband/acc: fix iteration counter in TB mode
* baseband/acc: fix memory leak on acc100 close
* baseband/acc: fix multiplexing acc100 operations
* baseband/acc: prevent to dequeue more than requested
* baseband/acc: protect from TB negative scenario
* build: detect backtrace availability
* build: fix dependencies lookup
* build: fix toolchain definition
* bus/fslmc: fix deadlock on MC send command timeout
* bus/ifpga: fix devargs handling
* cmdline: handle EOF as quit
* cmdline: make rdline status not private
* common/cnxk: add memory clobber to steor and ldeor
* common/cnxk: fix aura ID handling
* common/cnxk: fix auth key length
* common/cnxk: fix channel mask for SDP interfaces
* common/cnxk: fix dual VLAN parsing
* common/cnxk: fix IPv6 extension header parsing
* common/cnxk: fix IPv6 extension matching
* common/cnxk: fix second pass flow rule layer type
* common/cnxk: reduce channel count per LMAC
* common/mlx5: fix offset of a field
* common/mlx5: improve AES-XTS tweak capability check
* common/mlx5: use just sufficient barrier for Arm
* common/sfc_efx/base: add MAE mark reset action
* compressdev: fix empty devargs parsing
* compressdev: fix end of driver list
* compress/mlx5: fix decompress xform validation
* compress/mlx5: fix output Adler-32 checksum offset
* compress/mlx5: fix queue setup for partial transformations
* crypto/ccp: fix IOVA handling
* crypto/ccp: fix PCI probing
* crypto/ccp: remove some dead code for UIO
* crypto/ccp: remove some printf
* crypto/cnxk: fix digest for empty input data
* cryptodev: fix empty devargs parsing
* cryptodev: fix sym session mempool creation description
* cryptodev: fix telemetry data truncation
* crypto/ipsec_mb: fix ZUC-256 maximum tag length
* crypto/ipsec_mb: relax multi-process requirement
* crypto/ipsec_mb: remove unnecessary null check
* crypto/openssl: fix freeing in RSA EVP
* crypto/openssl: fix warning on copy length
* crypto/qat: fix build
* crypto/qat: fix build for generic x86 with GCC 12
* crypto/qat: fix SM3 auth mode
* crypto/qat: fix stream cipher direction
* devtools: fix escaped space in grep pattern
* devtools: fix name check with mbox files
* devtools: move mailmap check after patch applied
* dma/ioat: fix device stop if no copies done
* dma/ioat: fix error reporting on restart
* dma/ioat: fix indexes after restart
* dma/skeleton: fix empty devargs parsing
* doc: add gpudev to the Doxygen index
* doc: add Linux capability to access physical addresses
* doc: fix code blocks in cryptodev guide
* doc: fix DCF instructions in ice guide
* doc: fix dependency setup in l2fwd-cat example guide
* doc: fix description of L2TPV2 flow item
* doc: fix firmware list in bnxt guide
* doc: fix LPM support in l3forward guide
* doc: fix pipeline example path in user guide
* doc: fix reference to event timer header
* drivers/bus: fix leak for devices without driver
* drivers: fix symbol exports when map is omitted
* eal: cleanup alarm and hotplug before memory detach
* eal/freebsd: fix lock in alarm callback
* eal/linux: fix hugetlbfs sub-directories discovery
* eal/unix: fix thread creation
* eal: use same atomic intrinsics for GCC and clang
* eal/windows: fix pedantic build
* eal/windows: fix thread creation
* eal/windows: mark memory config as complete
* ethdev: fix build with LTO
* ethdev: fix telemetry data truncation
* ethdev: remove telemetry Rx mbuf alloc failed field
* event/cnxk: fix burst timer arm
* event/cnxk: fix SSO cleanup
* event/cnxk: fix timer operations in secondary process
* event/cnxk: wait for CPT flow control on WQE path
* eventdev/crypto: fix enqueue count
* eventdev/crypto: fix failed events
* eventdev/crypto: fix function symbol export
* eventdev/crypto: fix offset used while flushing events
* eventdev/crypto: fix overflow in circular buffer
* eventdev/eth_rx: fix getting adapter instance
* eventdev/eth_tx: fix devices loop
* eventdev: fix memory size for telemetry
* eventdev/timer: fix overflow
* examples/cmdline: fix build with GCC 12
* examples/fips_validation: add extra space in JSON buffer
* examples/fips_validation: fix AES-GCM tests
* examples/fips_validation: fix AES-XTS sequence number
* examples/fips_validation: fix integer parsing
* examples/fips_validation: fix MCT output for SHA
* examples/ipsec-secgw: fix auth IV length
* examples/ipsec-secgw: fix offload variable init
* examples/l2fwd-event: fix worker cleanup
* examples/l3fwd: remove hash entry number
* examples/qos_sched: fix config entries in wrong sections
* examples/qos_sched: fix debug mode
* examples/qos_sched: fix Tx port config when link down
* fbarray: fix metadata dump
* gpudev: export header file for external drivers
* gpudev: fix deadlocks when registering callback
* graph: fix node shrink
* hash: fix GFNI implementation build with GCC 12
* kni: fix build on RHEL 9.1
* kni: fix possible starvation when mbufs are exhausted
* kvargs: add API documentation for process callback
* mem: fix heap ID in telemetry
* mem: fix hugepage info mapping
* mem: fix telemetry data truncation
* mempool: fix telemetry data truncation
* net/bnxt: fix link state change interrupt config
* net/bnxt: fix RSS hash in mbuf
* net/bnxt: fix Rx queue stats after queue stop and start
* net/bnxt: fix Tx queue stats after queue stop and start
* net/cnxk: fix deadlock in security session creation
* net/cnxk: fix LBK BPID usage
* net/cnxk: fix packet type for IPv6 packets post decryption
* net/cnxk: validate RED threshold config
* net/e1000: fix saving of stripped VLAN TCI
* net/ena: fix deadlock in RSS RETA update
* net/gve: fix offloading capability
* net/hns3: add debug info for Rx/Tx dummy function
* net/hns3: add verification of RSS types
* net/hns3: allow adding queue buffer size hash rule
* net/hns3: declare flow rule keeping capability
* net/hns3: extract common functions to set Rx/Tx
* net/hns3: extract common function to query device
* net/hns3: fix burst mode query with dummy function
* net/hns3: fix clearing RSS configuration
* net/hns3: fix config struct used for conversion
* net/hns3: fix duplicate RSS rule check
* net/hns3: fix empty devargs parsing
* net/hns3: fix inaccurate RTC time to read
* net/hns3: fix log about indirection table size
* net/hns3: fix possible truncation of hash key when config
* net/hns3: fix possible truncation of redirection table
* net/hns3: fix RSS key size compatibility
* net/hns3: fix warning on flush or destroy rule
* net/hns3: make getting Tx function static
* net/hns3: refactor set RSS hash algorithm and key interface
* net/hns3: reimplement hash flow function
* net/hns3: remove debug condition for Tx prepare
* net/hns3: remove useless code when destroy valid RSS rule
* net/hns3: save hash algo to RSS filter list node
* net/hns3: separate flow RSS config from RSS conf
* net/hns3: separate setting and clearing RSS rule
* net/hns3: separate setting hash algorithm
* net/hns3: separate setting hash key
* net/hns3: separate setting redirection table
* net/hns3: separate setting RSS types
* net/hns3: separate Tx prepare from getting Tx function
* net/hns3: use hardware config to report hash key
* net/hns3: use hardware config to report hash types
* net/hns3: use hardware config to report redirection table
* net/hns3: use new RSS rule to configure hardware
* net/hns3: use RSS filter list to check duplicated rule
* net/i40e: fix AVX512 fast-free path
* net/i40e: fix MAC loopback on X722
* net/i40e: fix maximum frame size configuration
* net/i40e: fix validation of flow transfer attribute
* net/i40e: reduce interrupt interval in multi-driver mode
* net/i40e: revert link status check on device start
* net/iavf: add lock for VF commands
* net/iavf: fix building data desc
* net/iavf: fix device stop during reset
* net/iavf: fix outer UDP checksum offload
* net/iavf: fix VLAN offload with AVX2
* net/iavf: protect insertion in flow list
* net/ice: fix Rx timestamp
* net/ice: fix validation of flow transfer attribute
* net/idpf: fix driver infos
* net/idpf: fix mbuf leak in split Tx
* net/idpf: reset queue flag when queue is stopped
* net/ipn3ke: fix representor name
* net/ipn3ke: fix thread exit
* net/ixgbe: enable IPv6 mask in flow rules
* net/ixgbe: fix firmware version consistency
* net/ixgbe: fix IPv6 mask in flow director
* net/mana: enable driver by default
* net/mana: fix stats counters
* net/mlx5: check compressed CQE opcode in vectorized Rx
* net/mlx5: fix available tag registers calculation for HWS
* net/mlx5: fix build with GCC 12 and ASan
* net/mlx5: fix CQE dump for Tx
* net/mlx5: fix crash on action template failure
* net/mlx5: fix egress group translation in HWS
* net/mlx5: fix error CQE dumping for vectorized Rx
* net/mlx5: fix flow sample with ConnectX-5
* net/mlx5: fix GENEVE resource overwrite
* net/mlx5: fix hairpin Tx queue reference count
* net/mlx5: fix isolated mode if no representor matching
* net/mlx5: fix read device clock in real time mode
* net/mlx5: fix sysfs port name translation
* net/mlx5: fix wait descriptor opcode for ConnectX-7
* net/mlx5: fix warning for Tx scheduling option
* net/mlx5: fix Windows build with MinGW GCC 12
* net/mlx5/hws: fix error code of send queue action
* net/mlx5/hws: fix IPv4 fragment matching
* net/mlx5/hws: fix memory leak on general pool DB init
* net/mlx5/hws: fix pattern creation
* net/mlx5: ignore non-critical syndromes for Rx queue
* net/nfp: fix 48-bit DMA support for NFDk
* net/nfp: fix firmware name derived from PCI name
* net/nfp: fix getting RSS configuration
* net/nfp: fix max DMA length
* net/nfp: fix MTU configuration order
* net/nfp: fix offload of multiple output actions
* net/nfp: fix set DSCP flow action
* net/nfp: fix set IPv4 flow action
* net/nfp: fix set IPv6 flow action
* net/nfp: fix set MAC flow action
* net/nfp: fix set TP flow action
* net/nfp: fix set TTL flow action
* net/nfp: fix teardown of flows sharing a mask ID
* net/nfp: fix Tx packet drop for large data length
* net/nfp: fix VNI of VXLAN encap action
* net/nfp: restrict flow flush to the port
* net/nfp: store counter reset before zeroing flow query
* net/ngbe: add spinlock protection on YT PHY
* net/ngbe: fix packet type to parse from offload flags
* net/sfc: enforce fate action in transfer flow rules
* net/sfc: export pick transfer proxy callback to representors
* net/sfc: fix MAC address entry leak in transfer flow parsing
* net/sfc: fix resetting mark in tunnel offload switch rules
* net/sfc: invalidate switch port entry on representor unplug
* net/txgbe: fix default signal quality value for KX/KX4
* net/txgbe: fix interrupt loss
* net/txgbe: fix packet type to parse from offload flags
* net/txgbe: fix Rx buffer size in config register
* net/vhost: add missing newline in logs
* net/vhost: fix leak in interrupt handle setup
* net/vhost: fix Rx interrupt
* net/virtio: deduce IP length for TSO checksum
* net/virtio: fix empty devargs parsing
* net/virtio: remove address width limit for modern devices
* net/virtio-user: fix device starting failure handling
* pdump: fix build with GCC 12
* raw/ifpga/base: fix init with multi-process
* raw/skeleton: fix empty devargs parsing
* raw/skeleton: fix selftest
* regex/mlx5: fix doorbell record
* regex/mlx5: utilize all available queue pairs
* reorder: fix sequence number mbuf field register
* reorder: invalidate buffer from ready queue in drain
* ring: silence GCC 12 warnings
* sched: fix alignment of structs in subport
* table: fix action selector group size log2 setting
* telemetry: fix repeat display when callback don't init dict
* telemetry: move include after guard
* test/bbdev: extend HARQ tolerance
* test/bbdev: fix crash for non supported HARQ length
* test/bbdev: remove check for invalid opaque data
* test/crypto: add missing MAC-I to PDCP vectors
* test/crypto: fix capability check for ZUC cipher-auth
* test/crypto: fix skip condition for CPU crypto SGL
* test/crypto: fix statistics error messages
* test/crypto: fix typo in AES test
* test/crypto: fix ZUC digest length in comparison
* test: fix segment length in packet generator
* test/mbuf: fix mbuf reset test
* test/mbuf: fix test with mbuf debug enabled
* test/reorder: fix double free of drained buffers
* vdpa/ifc: fix argument compatibility check
* vdpa/ifc: fix reconnection in SW-assisted live migration
* version: 22.11.2-rc1
* vhost: decrease log level for unimplemented requests
* vhost: fix net header settings in datapath
* vhost: fix OOB access for invalid vhost ID
* vhost: fix possible FD leaks
* vhost: fix possible FD leaks on truncation
* vhost: fix slot index in async split virtqueue Tx

22.11.2 Validation
~~~~~~~~~~~~~~~~~~

* Intel(R) Testing

   * Basic Intel(R) NIC testing

      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu20.04, Ubuntu22.04, Fedora35, Fedora37, RHEL8.6, RHEL8.4, FreeBSD13.1, SUSE15, CentOS7.9, openEuler22.03-SP1 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing

      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 7.0u3, etc.
      * Cryptodev:

         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

* Nvidia(R) Testing

   * Basic functionality

      * Tx/Rx, xstats, timestamps, link status, RTE flow, RSS, VLAN, checksum and TSO, ptype...
      * link_status interrupt, l3fwd-power, multi-process.
      * LRO, regEx, buffer split, Tx scheduling.

   * Build tests

      * Ubuntu 20.04.6 with MLNX_OFED_LINUX-5.9-0.5.6.0.
      * Ubuntu 20.04.6 with rdma-core master (d2dbc88).
      * Ubuntu 20.04.6 with rdma-core v28.0.
      * Ubuntu 18.04.6 with rdma-core v17.1.
      * Ubuntu 18.04.6 with rdma-core master (d2dbc88) (i386).
      * Fedora 38 with rdma-core v44.0.
      * Fedora 39 (Rawhide) with rdma-core v44.0.
      * CentOS 7 7.9.2009 with rdma-core master (d2dbc88).
      * CentOS 7 7.9.2009 with MLNX_OFED_LINUX-5.9-0.5.6.0.
      * CentOS 8 8.4.2105 with rdma-core master (d2dbc88).
      * OpenSUSE Leap 15.4 with rdma-core v38.1.
      * Windows Server 2019 with Clang 11.0.0.

   * Test platform

      * NIC: ConnectX-5 / OS: Ubuntu 20.04 / Kernel: 6.3.0 / Driver: rdma-core v45.0 / Firmware: 16.35.2000
      * NIC: ConnectX-6 Dx / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-5.9-0.5.6.0 / Firmware: 22.36.1010
      * NIC: ConnectX-7 / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-5.9-0.5.6.0 / Firmware: 22.36.1010
      * DPU: BlueField-2 / DOCA SW version: 1.5.1 / Firmware: 24.35.2000

22.11.2 Known Issues
~~~~~~~~~~~~~~~~~~~~


