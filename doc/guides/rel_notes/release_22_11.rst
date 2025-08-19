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



22.11.3 Release Notes
---------------------


22.11.3 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: fix socket ID default value
* app/testpmd: fix checksum engine with GTP on 32-bit
* app/testpmd: fix flow rule number parsing
* app/testpmd: fix GTP L2 length in checksum engine
* app/testpmd: fix meter mark handle update
* app/testpmd: fix primary process not polling all queues
* app/testpmd: revert primary process polling all queues fix
* baseband/fpga_5gnr_fec: fix possible division by zero
* baseband/fpga_5gnr_fec: fix starting unconfigured queue
* build: fix warning when getting NUMA nodes
* ci: fix build for Arm cross compilation in GHA
* ci: fix libabigail cache in GHA
* common/cnxk: fix CPT backpressure disable on LBK
* common/cnxk: fix inline device VF identification
* common/cnxk: fix IPsec IPv6 tunnel address byte swap
* common/cnxk: fix receive queue with multiple mask
* common/cnxk: fix setting channel mask for SDP interfaces
* common/cnxk: fix uninitialized pointer read
* common/iavf: fix MAC type for 710 NIC
* common/idpf/base: fix control queue send and receive
* common/idpf/base: fix ITR register definitions for AVF
* common/idpf/base: fix memory leaks on control queue
* common/idpf/base: fix parameters when send msg to cp
* common/idpf: fix memory leak on AVX512 Tx queue close
* common/idpf: remove device stop flag
* common/mlx5: adjust fork call with new kernel API
* common/mlx5: fix obtaining IB device in LAG mode
* common/qat: detach crypto from compress build
* common/qat: fix command parameter corruption
* common/sfc_efx/base: fix Rx queue without RSS hash prefix
* crypto/cnxk: fix IPsec CCM capabilities
* cryptodev: clarify error codes for symmetric session
* cryptodev: fix comments of modular operation parameters
* cryptodev: fix device socket ID type
* crypto/ipsec_mb: fix enqueue counter for SNOW3G
* crypto/ipsec_mb: optimize allocation in session
* crypto/openssl: fix memory free
* crypto/openssl: fix memory leak in auth processing
* crypto/openssl: skip workaround at compilation time
* crypto/qat: fix null algorithm digest placement
* crypto/qat: fix stack buffer overflow in SGL loop
* crypto/qat: fix sym device prototype
* crypto/scheduler: fix last element for valid args
* devtools: fix bashism in mailmap check
* devtools: fix mailmap check for parentheses
* dma/dpaa2: set VFA bit for route-by-port with VF
* doc: add flow template API requirements for mlx5
* doc: fix auth algos in cryptoperf app
* doc: fix description of runtime directories
* doc: fix event timer adapter guide
* doc: fix format in flow API guide
* doc: fix kernel patch link in hns3 guide
* doc: fix link to flow capabilities from bnxt guide
* doc: fix number of leading spaces in hns3 guide
* doc: fix syntax in hns3 guide
* doc: fix typo in cnxk platform guide
* doc: fix typo in graph guide
* doc: fix typos and wording in flow API guide
* doc: improve wording of cuda guide
* doc: remove warning with Doxygen 1.9.7
* doc: update BIOS settings and supported HW for NTB
* eal: avoid calling cleanup twice
* eal/linux: fix legacy mem init with many segments
* eal/linux: fix secondary process crash for mp hotplug requests
* eal/x86: improve multiple of 64 bytes memcpy performance
* ethdev: check that at least one FEC mode is specified
* ethdev: fix calloc arguments
* ethdev: fix indirect action conversion
* ethdev: fix MAC address occupies two entries
* ethdev: fix potential leak in PCI probing helper
* ethdev: update documentation for API to get FEC
* ethdev: update documentation for API to set FEC
* event/cnxk: fix mempool cookies check
* event/cnxk: fix nanoseconds to ticks conversion
* event/cnxk: fix setting attributes in empty get work
* event/cnxk: fix Tx adapter data pointer
* eventdev/timer: fix buffer flush
* eventdev/timer: fix timeout event wait behavior
* event/dsw: free rings on close
* examples/fips_validation: fix digest length in AES-GCM
* examples/fips_validation: fix external build
* examples/ip_pipeline: fix build with GCC 13
* examples/ipsec-secgw: fix socket ID default value
* examples/ipsec-secgw: fix TAP default MAC address
* examples/ipsec-secgw: fix zero address in ethernet header
* examples/l2fwd-cat: fix external build
* examples/l3fwd: fix duplicate expression for default nexthop
* examples/ntb: fix build with GCC 13
* fib: fix adding default route
* hash: fix reading unaligned bits in Toeplitz hash
* ipc: fix file descriptor leakage with unhandled messages
* ipsec: fix NAT-T header length
* kernel/freebsd: fix function parameter list
* kni: fix build with Linux 6.3
* kni: fix build with Linux 6.5
* mbuf: fix Doxygen comment of distributor metadata
* member: fix PRNG seed reset in NitroSketch mode
* mem: fix memsegs exhausted message
* mempool/cnxk: avoid hang when counting batch allocs
* net/bonding: fix destroy dedicated queues flow
* net/bonding: fix startup when NUMA is not supported
* net/cnxk: fix cookies check with security offload
* net/cnxk: fix flow queue index validation
* net/cnxk: flush SQ before configuring MTU
* net/dpaa2: fix checksum good flags
* net/e1000: fix queue number initialization
* net/e1000: fix Rx and Tx queue status
* net: fix return type of IPv4 L4 packet checksum
* net/hns3: delete duplicate macro definition
* net/hns3: extract PTP to its own header file
* net/hns3: fix build warning
* net/hns3: fix device start return value
* net/hns3: fix FEC mode check
* net/hns3: fix FEC mode for 200G ports
* net/hns3: fix IMP reset trigger
* net/hns3: fix inaccurate log
* net/hns3: fix index to look up table in NEON Rx
* net/hns3: fix mbuf leakage when RxQ started after reset
* net/hns3: fix mbuf leakage when RxQ started during reset
* net/hns3: fix missing FEC capability
* net/hns3: fix never set MAC flow control
* net/hns3: fix non-zero weight for disabled TC
* net/hns3: fix redundant line break in log
* net/hns3: fix RTC time after reset
* net/hns3: fix RTC time on initialization
* net/hns3: fix Rx multiple firmware reset interrupts
* net/hns3: fix uninitialized variable
* net/hns3: fix variable type mismatch
* net/hns3: uninitialize PTP
* net/i40e: fix comments
* net/i40e: fix Rx data buffer size
* net/i40e: fix tunnel packet Tx descriptor
* net/iavf: fix abnormal disable HW interrupt
* net/iavf: fix protocol agnostic offloading with big packets
* net/iavf: fix Rx data buffer size
* net/iavf: fix stop ordering
* net/iavf: fix tunnel TSO path selection
* net/iavf: fix virtchnl command called in interrupt
* net/iavf: fix VLAN insertion in vector path
* net/iavf: fix VLAN offload with AVX512
* net/iavf: release large VF when closing device
* net/ice: adjust timestamp mbuf register
* net/ice/base: fix incorrect defines for DCBx
* net/ice/base: remove unreachable code
* net/ice: fix 32-bit build
* net/ice: fix DCF control thread crash
* net/ice: fix DCF RSS initialization
* net/ice: fix MAC type of E822 and E823
* net/ice: fix outer UDP checksum offload
* net/ice: fix protocol agnostic offloading with big packets
* net/ice: fix RSS hash key generation
* net/ice: fix Rx data buffer size
* net/ice: fix statistics
* net/ice: fix timestamp enabling
* net/ice: fix tunnel packet Tx descriptor
* net/ice: fix VLAN mode parser
* net/ice: initialize parser for double VLAN
* net/idpf: fix Rx data buffer size
* net/igc: fix Rx and Tx queue status
* net/ixgbe: add proper memory barriers in Rx
* net/ixgbe: fix Rx and Tx queue status
* net/mana: avoid unnecessary assignments in data path
* net/mana: fix counter overflow for posted WQE
* net/mana: fix Tx queue statistics
* net/mana: fix WQE count for ringing RQ doorbell
* net/mana: optimize completion queue by batch processing
* net/mana: return probing failure if no device found
* net/mana: use datapath logging
* net/mlx5: enhance error log for tunnel offloading
* net/mlx5: fix device removal event handling
* net/mlx5: fix drop action attribute validation
* net/mlx5: fix drop action memory leak
* net/mlx5: fix duplicated tag index matching in SWS
* net/mlx5: fix error in VLAN actions creation
* net/mlx5: fix error set for age pool initialization
* net/mlx5: fix error set in control tables create
* net/mlx5: fix error set in Tx representor tagging
* net/mlx5: fix flow dump for modify field
* net/mlx5: fix flow workspace destruction
* net/mlx5: fix handle validation for meter mark
* net/mlx5: fix LRO TCP checksum
* net/mlx5: fix matcher layout size calculation
* net/mlx5: fix MPRQ stride size for headroom
* net/mlx5: fix profile check of meter mark
* net/mlx5: fix query for NIC flow capability
* net/mlx5: fix return value of vport action
* net/mlx5: fix risk in NEON Rx descriptor read
* net/mlx5: fix RSS expansion inner buffer overflow
* net/mlx5: fix validation for conntrack indirect action
* net/mlx5: fix VXLAN matching with zero value
* net/mlx5: forbid duplicated tag index in pattern template
* net/mlx5: forbid MPRQ restart
* net/mlx5: reduce counter pool name length
* net/netvsc: fix sizeof calculation
* net/nfp: fix address always related with PF ID 0
* net/nfp: fix control mempool creation
* net/nfp: fix disabling promiscuous mode
* net/nfp: fix endian conversion for tunnel decap action
* net/nfp: fix flow hash table creation
* net/nfp: fix IPv6 address for set flow action
* net/nfp: fix IPv6 flow item
* net/nfp: fix offloading flows
* net/nfp: fix representor creation
* net/nfp: fix representor name too long
* net/nfp: fix TOS of IPv6 GENEVE encap flow action
* net/nfp: fix TOS of IPv6 NVGRE encap flow action
* net/nfp: fix TOS of IPv6 VXLAN encap flow action
* net/nfp: fix TP flow action for UDP
* net/nfp: fix Tx descriptor free logic of NFD3
* net/nfp: fix unneeded endian conversion
* net/nfp: fix VLAN push flow action
* net/nfp: fix VNI of IPv4 NVGRE encap action
* net/nfp: fix VNI of IPv6 NVGRE encap action
* net/nfp: fix VNI of VXLAN encap action
* net/ngbe: adapt to MNG veto bit setting
* net/ngbe: fix extended statistics
* net/ngbe: fix link status in no LSC mode
* net/ngbe: fix RSS offload capability
* net/ngbe: remove redundant codes
* net/qede: fix RSS indirection table initialization
* net/sfc: invalidate dangling MAE flow action FW resource IDs
* net/sfc: stop misuse of Rx ingress m-port metadata on EF100
* net/tap: set locally administered bit for fixed MAC address
* net/txgbe: adapt to MNG veto bit setting
* net/txgbe/base: fix Tx with fiber hotplug
* net/txgbe: fix blocking system events
* net/txgbe: fix extended statistics
* net/txgbe: fix interrupt enable mask
* net/txgbe: fix to set autoneg for 1G speed
* net/txgbe: fix use-after-free on remove
* net/virtio: fix initialization to return negative errno
* net/virtio: propagate interrupt configuration error values
* net/virtio-user: fix leak when initialisation fails
* net/vmxnet3: fix drop of empty segments in Tx
* net/vmxnet3: fix return code in initializing
* pci: fix comment referencing renamed function
* pipeline: fix double free for table stats
* raw/ntb: avoid disabling interrupt twice
* Revert "net/iavf: fix tunnel TSO path selection"
* ring: fix dequeue parameter name
* ring: fix use after free
* telemetry: fix autotest on Alpine
* test: add graph tests
* test/bonding: fix include of standard header
* test/crypto: fix IPsec AES CCM vector
* test/crypto: fix PDCP-SDAP test vectors
* test/crypto: fix return value for SNOW3G
* test/crypto: fix session creation check
* test/malloc: fix missing free
* test/malloc: fix statistics checks
* test/mbuf: fix crash in a forked process
* test/security: fix event inline IPsec reassembly tests
* version: 22.11.3-rc1
* vfio: fix include with musl runtime
* vhost: fix invalid call FD handling
* vhost: fix notification stats for packed ring

22.11.3 Validation
~~~~~~~~~~~~~~~~~~

* Intel(R) Testing

   * Basic Intel(R) NIC testing

      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu20.04, Ubuntu22.04, Fedora38, RHEL8.7, RHEL9.2, FreeBSD13.1, SUSE15, CentOS7.9, openEuler22.03-SP1，OpenAnolis8.8 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing

      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:

         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

* Nvidia(R) Testing

   * Basic functionality

      * Tx/Rx, xstats, timestamps, link status, RTE flow, RSS, VLAN, checksum and TSO, ptype...
      * link_status interrupt, l3fwd-power, multi-process.
      * LRO, regEx, buffer split, Tx scheduling.

   * Build tests

      * Ubuntu 20.04.6 with MLNX_OFED_LINUX-23.04-1.1.3.0.
      * Ubuntu 20.04.6 with rdma-core master (4cce53f).
      * Ubuntu 20.04.6 with rdma-core v28.0.
      * Ubuntu 18.04.6 with rdma-core master (4cce53f) (i386).
      * Fedora 38 with rdma-core v44.0.
      * Fedora 39 (Rawhide) with rdma-core v46.0.
      * OpenSUSE Leap 15.5 with rdma-core v42.0.
      * Windows Server 2019 with Clang 11.0.0.

   * Test platform

      * NIC: ConnectX-6 Dx / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-23.07-0.5.0.0 / Firmware: 22.38.1002
      * NIC: ConnectX-7 / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-23.07-0.5.0.0 / Firmware: 28.38.1002
      * DPU: BlueField-2 / DOCA SW version: 2.2.0 / Firmware: 24.38.1002

* Redhat Testing

   * Test scenarios

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q - cross numa node throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing

   * Version Information:

      * RHEL9
      * qemu-kvm-6.2.0 + qemu-kvm-7.2.0
      * kernel 5.14
      * X540-AT2 NIC(ixgbe, 10G)

22.11.3 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Intel(R) Testing

   * Cryptodev: Performance drop for 1c1t scenario

22.11.4 Release Notes
---------------------


22.11.4 Fixes
~~~~~~~~~~~~~

* app/bbdev: fix link with NXP LA12XX
* app/dumpcap: allow multiple invocations
* app/dumpcap: fix mbuf pool ring type
* app/pipeline: add sigint handler
* app/procinfo: adjust format of RSS info
* app/procinfo: fix RSS info
* app/procinfo: remove unnecessary rte_malloc
* app/test: disable graph auto test for windows
* app/test: fix reference to master in bonding test
* app/testpmd: add explicit check for tunnel TSO
* app/testpmd: fix early exit from signal
* app/testpmd: fix help string
* app/testpmd: fix primary process not polling all queues
* app/testpmd: fix tunnel TSO capability check
* app/testpmd: fix tunnel TSO configuration
* app/testpmd: remove useless check in TSO command
* baseband/acc: fix ACC100 HARQ input alignment
* baseband/acc: fix TB mode on VRB1
* build: add libarchive to optional external dependencies
* bus/dpaa: fix build with asserts for GCC 13
* bus/ifpga: fix driver header dependency
* bus/pci: add PASID control
* bus/pci: fix device ID log
* ci: fix race on container image name
* common/cnxk: fix aura disable handling
* common/cnxk: fix default flow action setting
* common/cnxk: fix different size bit operations
* common/cnxk: fix DPI memzone name
* common/cnxk: fix incorrect aura ID
* common/cnxk: fix pool buffer size in opaque mode
* common/cnxk: fix RSS key configuration
* common/cnxk: fix SDP channel mask
* common/cnxk: fix xstats for different packet sizes
* common/cnxk: remove dead Meson code
* common/cnxk: replace direct API usage in REE
* common/mlx5: fix controller index parsing
* common/mlx5: replace use of PMD log type
* config/arm: fix aarch32 build with GCC 13
* config: fix RISC-V native build
* crypto/cnxk: fix IPsec CCM and GCM capabilities
* cryptodev: add missing doc for security context
* crypto/dpaa2_sec: fix debug prints
* crypto/dpaa_sec: fix debug prints
* crypto/ipsec_mb: add dependency check for cross build
* crypto/nitrox: fix panic with high number of segments
* crypto/openssl: fix memory leaks in asym session
* crypto/qat: fix raw API null algorithm digest
* dma/cnxk: fix chunk buffer failure return code
* dma/cnxk: fix device reconfigure
* dma/cnxk: fix device state
* doc: fix hns3 build option about max queue number
* doc: fix RSS flow description in hns3 guide
* doc: fix some ordered lists
* doc: remove number of commands in vDPA guide
* doc: remove restriction on ixgbe vector support
* doc: replace code blocks with includes in security guide
* doc: update features in hns3 guide
* doc: update kernel module entry in QAT guide
* doc: update versions recommendations for i40e and ice
* eal/riscv: fix vector type alignment
* eal/unix: fix firmware reading with external xz helper
* eal/windows: fix build with recent MinGW
* ethdev: account for smaller MTU when setting default
* ethdev: add check in async flow action query
* ethdev: fix 32-bit build with GCC 13
* ethdev: fix ESP packet type description
* ethdev: fix function name in comment
* event/cnxk: fix CASP usage for clang
* event/cnxk: fix context flush in port cleanup
* event/cnxk: fix getwork mode devargs parsing
* event/cnxk: fix return values for capability API
* eventdev/crypto: fix circular buffer full case
* eventdev/eth_rx: fix timestamp field register in mbuf
* eventdev: fix alignment padding
* eventdev: fix device pointer for vdev-based devices
* eventdev: fix missing driver names in info struct
* eventdev: fix symbol export for port maintenance
* event/dlb2: disable PASID
* event/dlb2: fix disable PASID
* event/dlb2: fix missing queue ordering capability flag
* event/dlb2: fix name check in self-test
* event/sw: fix ordering corruption with op release
* event/sw: remove obsolete comment
* examples/ethtool: fix pause configuration
* examples/ipsec-secgw: fix partial overflow
* fib6: fix adding default route as first route
* fib: fix adding default route overwriting entire table
* gpu/cuda: fix build with external GDRCopy
* hash: align SSE lookup to scalar implementation
* malloc: remove return from void functions
* mem: fix deadlock with multiprocess
* mempool: clarify enqueue/dequeue ops documentation
* mempool/cnxk: fix alloc from non-EAL threads
* mempool/cnxk: fix free from non-EAL threads
* mempool: fix default ops for an empty mempool
* mempool: fix get function documentation
* meter: fix RFC4115 trTCM API Doxygen
* net/af_packet: fix Rx and Tx queue state
* net/af_xdp: fix Rx and Tx queue state
* net/ark: support single function with multiple port
* net/avp: fix Rx and Tx queue state
* net/axgbe: identify CPU with cpuid
* net/bnx2x: fix Rx and Tx queue state
* net/bnxt: fix Rx and Tx queue state
* net/bonding: fix header for C++
* net/bonding: fix link status callback stop
* net/bonding: fix possible overrun
* net/bonding: fix Rx and Tx queue state
* net/cnxk: fix data offset in vector Tx
* net/cnxk: fix uninitialized variable
* net/cxgbe: fix Rx and Tx queue state
* net/dpaa2: fix Rx and Tx queue state
* net/dpaa: fix Rx and Tx queue state
* net/e1000: fix Rx and Tx queue state
* net/ena: fix Rx and Tx queue state
* net/enetc: fix Rx and Tx queue state
* net/enic: avoid extra unlock in MTU set
* net/enic: fix Rx and Tx queue state
* net/gve: fix max MTU limit
* net/gve: fix RX buffer size alignment
* net/gve: update max Rx packet length to be based on MTU
* net/hinic: fix Rx and Tx queue state
* net/hns3: fix double stats for IMP and global reset
* net/hns3: fix error code for multicast resource
* net/hns3: fix flushing multicast MAC address
* net/hns3: fix ignored reset event
* net/hns3: fix IMP or global reset
* net/hns3: fix LRO offload to report
* net/hns3: fix mailbox sync
* net/hns3: fix multiple reset detected log
* net/hns3: fix order in NEON Rx
* net/hns3: fix reset event status
* net/hns3: fix setting DCB capability
* net/hns3: fix some error logs
* net/hns3: fix some return values
* net/hns3: fix traffic management dump text alignment
* net/hns3: fix traffic management thread safety
* net/hns3: fix typo in function name
* net/hns3: fix unchecked Rx free threshold
* net/hns3: fix uninitialized hash algo value
* net/hns3: fix VF default MAC modified when set failed
* net/hns3: fix VF reset handler interruption
* net/hns3: keep set/get algo key functions local
* net/hns3: refactor interrupt state query
* net/hns3: remove reset log in secondary
* net/i40e: fix buffer leak on Rx reconfiguration
* net/i40e: fix FDIR queue receives broadcast packets
* net/iavf: fix checksum offloading
* net/iavf: fix ESN session update
* net/iavf: fix indent in Tx path
* net/iavf: fix port stats clearing
* net/iavf: fix TSO with big segments
* net/iavf: fix Tx debug
* net/iavf: fix Tx offload flags check
* net/iavf: fix Tx offload mask
* net/iavf: fix Tx preparation
* net/iavf: fix VLAN offload strip flag
* net/iavf: remove log from Tx prepare function
* net/iavf: unregister interrupt handler before FD close
* net/ice: fix crash on closing representor ports
* net/ice: fix DCF port statistics
* net/ice: fix initial link status
* net/ice: fix L1 check interval
* net/ice: fix TM configuration clearing
* net/ice: fix TSO with big segments
* net/ice: fix Tx preparation
* net/ice: remove log from Tx prepare function
* net/ice: write timestamp to first segment in scattered Rx
* net/ipn3ke: fix Rx and Tx queue state
* net/mana: add 32-bit short doorbell
* net/mana: add missing new line to data path logs
* net/mana: enable 32-bit build
* net/memif: fix Rx and Tx queue state
* net/mlx4: fix Rx and Tx queue state
* net/mlx5: fix counter query during port close
* net/mlx5: fix decap action checking in sample flow
* net/mlx5: fix destroying external representor flow
* net/mlx5: fix E-Switch mirror flow rule validation
* net/mlx5: fix flow thread safety flag for HWS
* net/mlx5: fix flow workspace double free in Windows
* net/mlx5: fix hairpin queue states
* net/mlx5: fix hairpin queue unbind
* net/mlx5: fix jump ipool entry size
* net/mlx5: fix LACP redirection in Rx domain
* net/mlx5: fix leak in sysfs port name translation
* net/mlx5: fix missing flow rules for external SQ
* net/mlx5: fix MPRQ stride size check
* net/mlx5: fix multi-segment Tx inline data length
* net/mlx5: fix NIC flow capability query
* net/mlx5: fix offset size in conntrack flow action
* net/mlx5: fix shared Rx queue list management
* net/mlx5: fix unlock mismatch
* net/mlx5: fix use after free on Rx queue start
* net/mlx5: fix validation of sample encap flow action
* net/mlx5/hws: fix field copy bind
* net/mlx5/hws: fix integrity bits level
* net/mlx5: zero UDP checksum over IPv4 in encapsulation
* net/mvneta: fix Rx and Tx queue state
* net/mvpp2: fix Rx and Tx queue state
* net/netvsc: increase VSP response timeout to 60 seconds
* net/nfp: fix control message packets
* net/nfp: fix crash on close
* net/nfp: fix DMA error after abnormal exit
* net/nfp: fix initialization of physical representors
* net/nfp: fix link status interrupt
* net/nfp: fix reconfigure logic in PF initialization
* net/nfp: fix reconfigure logic in VF initialization
* net/nfp: fix reconfigure logic of set MAC address
* net/nfp: fix Rx and Tx queue state
* net/ngbe: add proper memory barriers in Rx
* net/ngbe: check process type in close operation
* net/ngbe: fix flow control
* net/ngbe: fix Rx and Tx queue state
* net/ngbe: keep link down after device close
* net/ngbe: prevent NIC from slowing down link speed
* net/ngbe: reconfigure MAC Rx when link update
* net/null: fix Rx and Tx queue state
* net/octeon_ep: fix Rx and Tx queue state
* net/octeontx: fix Rx and Tx queue state
* net/pfe: fix Rx and Tx queue state
* net/ring: fix Rx and Tx queue state
* net/sfc: account for data offset on Tx
* net/sfc: add missing error code indication to MAE init path
* net/sfc: fix Rx and Tx queue state
* net/sfc: remove null dereference in log
* net/sfc: set max Rx packet length for representors
* net/softnic: fix Rx and Tx queue state
* net/tap: fix IPv4 checksum offloading
* net/tap: fix L4 checksum offloading
* net/tap: fix RSS for fragmented packets
* net/tap: use MAC address parse API instead of local parser
* net/txgbe: add proper memory barriers in Rx
* net/txgbe: add Tx queue maximum limit
* net/txgbe: check process type in close operation
* net/txgbe: fix GRE tunnel packet checksum
* net/txgbe: fix out of bound access
* net/txgbe: fix Rx and Tx queue state
* net/txgbe: keep link down after device close
* net/txgbe: reconfigure MAC Rx when link update
* net/vhost: fix Rx and Tx queue state
* net/virtio: fix link state interrupt vector setting
* net/virtio: fix missing next flag in Tx packed ring
* net/virtio: fix Rx and Tx queue state
* net/vmxnet3: fix Rx and Tx queue state
* pdump: fix error number on IPC response
* random: initialize state for unregistered non-EAL threads
* rawdev: fix device class in log message
* Revert "eventdev: fix alignment padding"
* Revert "net/iavf: fix abnormal disable HW interrupt"
* test/bbdev: assert failed test for queue configure
* test/bbdev: fix Python script subprocess
* test/bonding: add missing check
* test/bonding: fix uninitialized RSS configuration
* test/bonding: remove unreachable statement
* test/crypto: fix IV in some vectors
* test/crypto: fix return value for GMAC case
* test/crypto: fix typo in asym tests
* test/crypto: skip some synchronous tests with CPU crypto
* test/event: fix crypto null device creation
* test: fix named test macro
* test/hash: fix creation error log
* test/security: fix IPv6 next header field
* usertools/pmdinfo: fix usage typos
* vdpa/mlx5: fix unregister kick handler order
* vhost: fix checking virtqueue access in stats API
* vhost: fix check on virtqueue access in async registration
* vhost: fix check on virtqueue access in in-flight getter
* vhost: fix missing check on virtqueue access
* vhost: fix missing lock protection in power monitor API
* vhost: fix missing spinlock unlock
* vhost: fix missing vring call check on virtqueue access

22.11.4 Validation
~~~~~~~~~~~~~~~~~~

* Redhat Testing

   * Test scenarios

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 4Q throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q - cross numa node throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing

   * Test Versions and device

      * qemu-kvm-7.2.0
      * kernel 5.14
      * X540-AT2 NIC(ixgbe, 10G)

* Nvidia(R) Testing

   * Basic functionality:

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
      * Regex application tests.
      * Buffer Split tests.
      * Tx scheduling tests.

   * Test platform

      * NIC: ConnectX-6 Dx / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-23.10-1.1.9.0 / Firmware: 22.39.2048
      * NIC: ConnectX-7 / OS: Ubuntu 20.04 / Driver: MLNX_OFED_LINUX-23.10-1.1.9.0 / Firmware: 28.39.2048
      * DPU: BlueField-2 / DOCA SW version: 2.5.0 / Firmware: 24.39.2048

   * OS/driver:

      * Ubuntu 20.04.6 with MLNX_OFED_LINUX-23.10-1.1.9.0.
      * Ubuntu 20.04.6 with rdma-core master (9016f34).
      * Ubuntu 20.04.6 with rdma-core v28.0.
      * Fedora 38 with rdma-core v44.0.
      * Fedora 40 (Rawhide) with rdma-core v48.0.
      * OpenSUSE Leap 15.5 with rdma-core v42.0.
      * Windows Server 2019 with Clang 16.0.6.

* Intel(R) Testing

   * Basic Intel(R) NIC testing

      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu20.04, Ubuntu22.04, Fedora38, RHEL8.7, RHEL9.2, FreeBSD13.2, SUSE15, CentOS7.9, openEuler22.03-SP1，OpenAnolis8.8 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing

      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:

         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

22.11.4 Known Issues
~~~~~~~~~~~~~~~~~~~~



22.11.5 Release Notes
---------------------


22.11.5 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: add missing op resubmission
* app/crypto-perf: fix copy segment size
* app/crypto-perf: fix data comparison
* app/crypto-perf: fix encrypt operation verification
* app/crypto-perf: fix next segment mbuf
* app/crypto-perf: fix out-of-place mbuf size
* app/crypto-perf: verify strdup return
* app/dumpcap: verify strdup return
* app/pdump: verify strdup return
* app/testpmd: fix async flow create failure handling
* app/testpmd: fix burst option parsing
* app/testpmd: fix crash in multi-process forwarding
* app/testpmd: fix error message for invalid option
* app/testpmd: fix GRO packets flush on timeout
* app/testpmd: fix --stats-period option check
* app/testpmd: hide --bitrate-stats in help if disabled
* app/testpmd: return if no packets in GRO heavy weight mode
* app/testpmd: verify strdup return
* baseband/acc: fix common logs
* baseband/acc: fix logtypes register
* baseband/fpga_5gnr_fec: use a better random generator
* build: fix linker warnings about undefined symbols
* build: fix reasons conflict
* build: link static libs with whole-archive in subproject
* build: pass cflags in subproject
* bus/dpaa: verify strdup return
* bus/fslmc: verify strdup return
* bus/ifpga: remove dead code
* bus/vdev: fix devargs in secondary process
* bus/vdev: verify strdup return
* ci: update versions of actions in GHA
* common/cnxk: fix link config for SDP
* common/cnxk: fix mbox region copy
* common/cnxk: fix mbox struct attributes
* common/cnxk: fix memory leak in CPT init
* common/cnxk: fix possible out-of-bounds access
* common/cnxk: fix RSS RETA configuration
* common/cnxk: fix Tx MTU configuration
* common/cnxk: fix VLAN check for inner header
* common/cnxk: remove CN9K inline IPsec FP opcodes
* common/cnxk: remove dead code
* common/mlx5: fix calloc parameters
* common/mlx5: fix duplicate read of general capabilities
* common/sfc_efx/base: use C11 static assert
* config: fix CPU instruction set for cross-build
* cryptodev: remove unused extern variable
* crypto/ipsec_mb: fix incorrectly setting cipher keys
* crypto/qat: fix crash with CCM null AAD pointer
* dmadev: fix calloc parameters
* dma/dpaa2: fix logtype register
* dma/idxd: verify strdup return
* doc: add --latencystats option in testpmd guide
* doc: add link speeds configuration in features table
* doc: add traffic manager in features table
* doc: fix commands in eventdev test tool guide
* doc: fix configuration in baseband 5GNR driver guide
* doc: fix default IP fragments maximum in programmer guide
* doc: fix typo in packet framework guide
* doc: fix typo in profiling guide
* doc: fix typos in cryptodev overview
* doc: update link to Windows DevX in mlx5 guide
* drivers/net: fix buffer overflow for packet types list
* eal: verify strdup return
* eal/x86: add AMD vendor check for TSC calibration
* ethdev: fix NVGRE encap flow action description
* event/cnxk: fix dequeue timeout configuration
* event/cnxk: verify strdup return
* eventdev/crypto: fix enqueueing
* eventdev: fix calloc parameters
* eventdev: fix Doxygen processing of vector struct
* eventdev: improve Doxygen comments on configure struct
* event/dlb2: remove superfluous memcpy
* event/opdl: fix compile-time check
* examples/ipsec-secgw: fix cryptodev to SA mapping
* examples/ipsec-secgw: fix Rx queue ID in Rx callback
* examples/ipsec-secgw: fix typo in error message
* examples/ipsec-secgw: fix width of variables
* examples/l3fwd: fix Rx over not ready port
* examples/packet_ordering: fix Rx with reorder mode disabled
* examples/qos_sched: fix memory leak in args parsing
* examples/vhost: verify strdup return
* hash: remove some dead code
* kernel/freebsd: fix module build on FreeBSD 14
* lib: add newline in logs
* lib: remove redundant newline from logs
* lib: use dedicated logtypes and macros
* net: add macros for VLAN metadata parsing
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
* net/cnxk: fix buffer size configuration
* net/cnxk: fix flow RSS configuration
* net/cnxk: fix mbuf fields in multi-segment Tx
* net/cnxk: fix MTU limit
* net/ena/base: limit exponential backoff
* net/ena/base: restructure interrupt handling
* net/ena: fix fast mbuf free
* net/ena: fix mbuf double free in fast free mode
* net/failsafe: fix memory leak in args parsing
* net: fix TCP/UDP checksum with padding data
* net/hns3: enable PFC for all user priorities
* net/hns3: fix disable command with firmware
* net/hns3: fix reset level comparison
* net/hns3: fix VF multiple count on one reset
* net/hns3: refactor handle mailbox function
* net/hns3: refactor PF mailbox message struct
* net/hns3: refactor send mailbox function
* net/hns3: refactor VF mailbox message struct
* net/hns3: remove QinQ insert support for VF
* net/hns3: support new device
* net/i40e: remove incorrect 16B descriptor read block
* net/i40e: remove redundant judgment in flow parsing
* net/iavf: fix memory leak on security context error
* net/iavf: remove error logs for VLAN offloading
* net/iavf: remove incorrect 16B descriptor read block
* net/ice: fix link update
* net/ice: fix memory leaks
* net/ice: fix tunnel TSO capabilities
* net/ice: fix version for experimental symbols
* net/ice: remove incorrect 16B descriptor read block
* net/ionic: fix device close
* net/ionic: fix missing volatile type for cqe pointers
* net/ionic: fix RSS query
* net/ixgbe: fix memoy leak after device init failure
* net/ixgbe: increase VF reset timeout
* net/ixgbevf: fix RSS init for x550 NICs
* net/mana: fix memory leak on MR allocation
* net/mana: handle MR cache expansion failure
* net/mana: prevent values overflow returned from RDMA layer
* net/memif: fix extra mbuf refcnt update in zero copy Tx
* net/mlx5: fix age position in hairpin split
* net/mlx5: fix async flow create error handling
* net/mlx5: fix condition of LACP miss flow
* net/mlx5: fix connection tracking action validation
* net/mlx5: fix conntrack action handle representation
* net/mlx5: fix counters map in bonding mode
* net/mlx5: fix DR context release ordering
* net/mlx5: fix drop action release timing
* net/mlx5: fix error packets drop in regular Rx
* net/mlx5: fix flow configure validation
* net/mlx5: fix flow counter cache starvation
* net/mlx5: fix GENEVE option item translation
* net/mlx5: fix GENEVE TLV option management
* net/mlx5: fix HWS meter actions availability
* net/mlx5: fix incorrect counter cache dereference
* net/mlx5: fix IP-in-IP tunnels recognition
* net/mlx5: fix jump action validation
* net/mlx5: fix meter policy priority
* net/mlx5: fix rollback on failed flow configure
* net/mlx5: fix stats query crash in secondary process
* net/mlx5: fix template clean up of FDB control flow rule
* net/mlx5: fix use after free when releasing Tx queues
* net/mlx5: fix VLAN handling in meter split
* net/mlx5: fix VLAN ID in flow modify
* net/mlx5: fix warning about copy length
* net/mlx5/hws: check not supported fields in VXLAN
* net/mlx5/hws: enable multiple integrity items
* net/mlx5/hws: fix port ID for root table
* net/mlx5/hws: fix tunnel protocol checks
* net/mlx5/hws: fix VLAN inner type
* net/mlx5/hws: fix VLAN item in non-relaxed mode
* net/mlx5: prevent ioctl failure log flooding
* net/mlx5: prevent querying aged flows on uninit port
* net/mlx5: remove device status check in flow creation
* net/mlx5: remove duplication of L3 flow item validation
* net/mlx5: remove GENEVE options length limitation
* net/netvsc: fix VLAN metadata parsing
* net/nfp: fix calloc parameters
* net/nfp: fix device close
* net/nfp: fix device resource freeing
* net/nfp: fix resource leak for CoreNIC firmware
* net/nfp: fix resource leak for exit of CoreNIC firmware
* net/nfp: fix resource leak for exit of flower firmware
* net/nfp: fix resource leak for flower firmware
* net/nfp: fix resource leak for PF initialization
* net/nfp: fix switch domain free check
* net/nfp: free switch domain ID on close
* net/tap: do not overwrite flow API errors
* net/tap: fix traffic control handle calculation
* net/tap: log Netlink extended ack unavailability
* net/thunderx: fix DMAC control register update
* net/virtio: remove duplicate queue xstats
* net/vmxnet3: fix initialization on FreeBSD
* net/vmxnet3: ignore Rx queue interrupt setup on FreeBSD
* pipeline: fix calloc parameters
* rawdev: fix calloc parameters
* regexdev: fix logtype register
* Revert "build: add libarchive to optional external dependencies"
* telemetry: fix connected clients count
* telemetry: fix empty JSON dictionaries
* test/bpf: fix mbuf init in some filter test
* test/cfgfile: fix typo in error messages
* test: do not count skipped tests as executed
* test/event: fix crash in Tx adapter freeing
* test/event: skip test if no driver is present
* test: fix probing in secondary process
* test/mbuf: fix external mbuf case with assert enabled
* test/power: fix typo in error message
* test: verify strdup return
* vdpa/mlx5: fix queue enable drain CQ
* version: 22.11.5-rc1
* vhost: fix deadlock during vDPA SW live migration
* vhost: fix memory leak in Virtio Tx split path
* vhost: fix virtqueue access check in vhost-user setup

22.11.5 Validation
~~~~~~~~~~~~~~~~~~

* Red Hat(R) Testing

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 8.2.0
      * libvirt 10.0.0
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server: PASS
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Intel(R) Testing

   * Basic Intel(R) NIC testing
      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu20.04, Ubuntu22.04, Fedora38, RHEL8.7, RHEL9.2, FreeBSD13.1, SUSE15, Centos7.9, openEuler22.03-SP1，OpenAnolis8.8 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PPF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing
      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:
         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.


* Nvidia(R) Testing

   * Basic functionality via testpmd/example applications

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split tests
      * Tx scheduling tests

   * Build tests

      * Ubuntu 20.04.6 with MLNX_OFED_LINUX-24.01-0.3.3.1.
      * Ubuntu 20.04.6 with rdma-core master (4b08a22).
      * Ubuntu 20.04.6 with rdma-core v28.0.
      * Fedora 38 with rdma-core v44.0.
      * Fedora 40 (Rawhide) with rdma-core v48.0.
      * OpenSUSE Leap 15.5 with rdma-core v42.0.
      * Windows Server 2019 with Clang 16.0.6.

   * BlueField-2

      * DOCA 2.6.0
      * fw 24.40.1000

   * ConnectX-7

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-24.01-0.3.3.1
      * fw 28.40.1000

   * ConnectX-6 Dx

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-24.01-0.3.3.1
      * fw 22.40.1000

22.11.6 Release Notes
---------------------


22.11.6 Fixes
~~~~~~~~~~~~~

* app/bbdev: fix interrupt tests
* app/crypto-perf: fix result for asymmetric
* app/crypto-perf: remove redundant local variable
* app/dumpcap: handle SIGTERM and SIGHUP
* app/pdump: handle SIGTERM and SIGHUP
* app/testpmd: add postpone option to async flow destroy
* app/testpmd: fix build on signed comparison
* app/testpmd: fix help string of BPF load command
* app/testpmd: fix lcore ID restriction
* app/testpmd: fix outer IP checksum offload
* app/testpmd: fix parsing for connection tracking item
* app/testpmd: handle IEEE1588 init failure
* baseband/acc: fix memory barrier
* baseband/la12xx: forbid secondary process
* bpf: disable on 32-bit x86
* bpf: fix load hangs with six IPv6 addresses
* bpf: fix MOV instruction evaluation
* buildtools: fix build with clang 17 and ASan
* build: use builtin helper for python dependencies
* bus/dpaa: fix bus scan for DMA devices
* bus/dpaa: fix memory leak in bus scan
* bus/dpaa: remove redundant file descriptor check
* bus/pci: fix build with musl 1.2.4 / Alpine 3.19
* bus/pci: fix FD in secondary process
* bus/pci: fix UIO resource mapping in secondary process
* bus/vdev: fix device reinitialization
* bus/vdev: revert fix devargs in secondary process
* common/dpaax/caamflib: fix PDCP AES-AES watchdog error
* common/dpaax/caamflib: fix PDCP-SDAP watchdog error
* common/dpaax: fix IOVA table cleanup
* common/dpaax: fix node array overrun
* common/idpf: fix flex descriptor mask
* common/mlx5: fix PRM structs
* common/mlx5: fix unsigned/signed mismatch
* common/mlx5: remove unneeded field when modify RQ table
* config: fix warning for cross build with meson >= 1.3.0
* crypto/cnxk: fix minimal input normalization
* cryptodev: fix build without crypto callbacks
* cryptodev: validate crypto callbacks from next node
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
* dmadev: fix structure alignment
* dma/hisilicon: remove support for HIP09 platform
* dma/idxd: fix setup with Ubuntu 24.04
* doc: add baseline mode in l3fwd-power guide
* doc: fix link to hugepage mapping from Linux guide
* doc: fix mbuf flags
* doc: fix testpmd ring size command
* doc: fix typo in l2fwd-crypto guide
* doc: remove empty section from testpmd guide
* doc: remove reference to mbuf pkt field
* eal: fix logs for '--lcores'
* eal/linux: lower log level on allocation attempt failure
* eal/unix: support ZSTD compression for firmware
* eal/windows: install sched.h file
* ethdev: fix device init without socket-local memory
* ethdev: fix GENEVE option item conversion
* eventdev/crypto: fix opaque field handling
* event/sw: fix warning from useless snprintf
* examples/fips_validation: fix dereference and out-of-bound
* examples: fix lcore ID restriction
* examples: fix port ID restriction
* examples: fix queue ID restriction
* examples/ipsec-secgw: fix SA salt endianness
* examples/ipsec-secgw: revert SA salt endianness
* examples/l3fwd: fix crash in ACL mode for mixed traffic
* examples/l3fwd: fix crash on multiple sockets
* fbarray: fix finding for unaligned length
* fbarray: fix incorrect lookahead behavior
* fbarray: fix incorrect lookbehind behavior
* fbarray: fix lookahead ignore mask handling
* fbarray: fix lookbehind ignore mask handling
* graph: fix ID collisions
* hash: check name when creating a hash
* hash: fix RCU reclamation size
* hash: fix return code description in Doxygen
* kni: fix build with Linux 6.8
* latencystats: fix literal float suffix
* malloc: fix multi-process wait condition handling
* mbuf: fix dynamic fields copy
* mempool: replace GCC pragma with cast
* net/af_packet: align Rx/Tx structs to cache line
* net/af_xdp: count mbuf allocation failures
* net/af_xdp: fix port ID in Rx mbuf
* net/af_xdp: fix stats reset
* net/af_xdp: remove unused local statistic
* net/ark: fix index arithmetic
* net/axgbe: check only minimum speed for cables
* net/axgbe: delay AN timeout during KR training
* net/axgbe: disable interrupts during device removal
* net/axgbe: disable RRC for yellow carp devices
* net/axgbe: enable PLL control for fixed PHY modes only
* net/axgbe: fix connection for SFP+ active cables
* net/axgbe: fix fluctuations for 1G Bel Fuse SFP
* net/axgbe: fix linkup in PHY status
* net/axgbe: fix MDIO access for non-zero ports and CL45 PHYs
* net/axgbe: fix SFP codes check for DAC cables
* net/axgbe: fix Tx flow on 30H HW
* net/axgbe: reset link when link never comes back
* net/axgbe: update DMA coherency values
* net/bonding: fix failover time of LACP with mode 4
* net/cnxk: fix outbound security with higher packet burst
* net/cnxk: fix promiscuous state after MAC change
* net/cnxk: fix RSS config
* net/dpaa: forbid MTU configuration for shared interface
* net/e1000/base: fix link power down
* net/ena: fix bad checksum handling
* net/ena: fix checksum handling
* net/ena: fix return value check
* net: fix outer UDP checksum in Intel prepare helper
* net/fm10k: fix cleanup during init failure
* net/hns3: check Rx DMA address alignmnent
* net/hns3: disable SCTP verification tag for RSS hash input
* net/hns3: fix double free for Rx/Tx queue
* net/hns3: fix offload flag of IEEE 1588
* net/hns3: fix Rx timestamp flag
* net/hns3: fix uninitialized variable in FEC query
* net/hns3: fix variable overflow
* net/i40e: fix outer UDP checksum offload for X710
* net/iavf: remove outer UDP checksum offload for X710 VF
* net/ice/base: fix board type definition
* net/ice/base: fix check for existing switch rule
* net/ice/base: fix GCS descriptor field offsets
* net/ice/base: fix masking when reading context
* net/ice/base: fix memory leak in firmware version check
* net/ice/base: fix pointer to variable outside scope
* net/ice/base: fix potential TLV length overflow
* net/ice/base: fix preparing PHY for timesync command
* net/ice/base: fix return type of bitmap hamming weight
* net/ice/base: fix sign extension
* net/ice/base: fix size when allocating children arrays
* net/ice/base: fix temporary failures reading NVM
* net/ice: fix check for outer UDP checksum offload
* net/ice: fix memory leaks in raw pattern parsing
* net/ice: fix return value for raw pattern parsing
* net/ice: fix sizing of filter hash table
* net/ionic: fix mbuf double-free when emptying array
* net/ixgbe/base: fix 5G link speed reported on VF
* net/ixgbe/base: fix PHY ID for X550
* net/ixgbe/base: revert advertising for X550 2.5G/5G
* net/ixgbe: do not create delayed interrupt handler twice
* net/ixgbe: do not update link status in secondary process
* net/mlx5: break flow resource release loop
* net/mlx5: fix access to flow template operations
* net/mlx5: fix Arm build with GCC 9.1
* net/mlx5: fix crash on counter pool destroy
* net/mlx5: fix disabling E-Switch default flow rules
* net/mlx5: fix end condition of reading xstats
* net/mlx5: fix flow template indirect action failure
* net/mlx5: fix hash Rx queue release in flow sample
* net/mlx5: fix indexed pool with invalid index
* net/mlx5: fix MTU configuration
* net/mlx5: fix start without duplicate flow patterns
* net/mlx5: fix uplink port probing in bonding mode
* net/mlx5/hws: add template match none flag
* net/mlx5/hws: decrease log level for creation failure
* net/mlx5/hws: fix action template dump
* net/mlx5/hws: fix deletion of action vport
* net/mlx5/hws: fix function comment
* net/mlx5/hws: fix port ID on root item convert
* net/mlx5/hws: fix spinlock release on context open
* net/mlx5/hws: remove unused variable
* net/mlx5: support jump in meter hierarchy
* net/nfp: adapt reverse sequence card
* net/nfp: disable ctrl VNIC queues on close
* net/nfp: fix allocation of switch domain
* net/nfp: fix disabling 32-bit build
* net/nfp: fix IPv6 TTL and DSCP flow action
* net/nfp: fix representor port queue release
* net/nfp: forbid offload flow rules with empty action list
* net/nfp: remove redundant function call
* net/ngbe: add special config for YT8531SH-CA PHY
* net/ngbe: fix hotplug remove
* net/ngbe: fix memory leaks
* net/ngbe: fix MTU range
* net/ngbe: keep PHY power down while device probing
* net/tap: fix file descriptor check in isolated flow
* net/txgbe: fix flow filters in VT mode
* net/txgbe: fix hotplug remove
* net/txgbe: fix memory leaks
* net/txgbe: fix MTU range
* net/txgbe: fix Rx interrupt
* net/txgbe: fix tunnel packet parsing
* net/txgbe: fix Tx hang on queue disable
* net/txgbe: fix VF promiscuous and allmulticast
* net/txgbe: reconfigure more MAC Rx registers
* net/txgbe: restrict configuration of VLAN strip offload
* net/virtio: fix MAC table update
* net/virtio-user: add memcpy check
* net/vmxnet3: fix init logs
* pcapng: add memcpy check
* power: increase the number of UNCORE frequencies
* telemetry: fix connection parameter parsing
* telemetry: lower log level on socket error
* test/crypto: fix allocation comment
* test/crypto: fix asymmetric capability test
* test/crypto: fix enqueue/dequeue callback case
* test/crypto: fix vector global buffer overflow
* test/crypto: remove unused stats in setup
* test: force IOVA mode on PPC64 without huge pages
* usertools/devbind: fix indentation
* vdpa/sfc: remove dead code
* version: 22.11.6-rc1
* vhost: cleanup resubmit info before inflight setup
* vhost: fix build with GCC 13

22.11.6 Validation
~~~~~~~~~~~~~~~~~~

* Red Hat(R) Testing

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 7.2.0
      * libvirt 9.0
      * openvswitch 3.1
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server: PASS
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Intel(R) Testing

   * Basic Intel(R) NIC testing
      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.04, Ubuntu22.04, Fedora40, RHEL9.3, RHEL9.4, FreeBSD14, SUSE15, Centos7.9, openEuler22.03-SP1，OpenAnolis8.8 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PPF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing
      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:
         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.


* Nvidia(R) Testing

   * Basic functionality via testpmd/example applications

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split tests
      * Tx scheduling tests

   * Build tests

      * Debian 12 with MLNX_OFED_LINUX-24.04-0.7.0.0.
      * Ubuntu 20.04.6 with MLNX_OFED_LINUX-24.07-0.6.1.0.
      * Ubuntu 20.04.6 with rdma-core master (dd9c687).
      * Ubuntu 20.04.6 with rdma-core v28.0.
      * Fedora 38 with rdma-core v48.0.
      * Fedora 42 (Rawhide) with rdma-core v51.0.
      * OpenSUSE Leap 15.6 with rdma-core v49.1.

   * BlueField-2

      * DOCA 2.8.0
      * fw 24.42.1000

   * ConnectX-7

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-24.07-0.6.1.0
      * fw 28.42.1000

   * ConnectX-6 Dx

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-24.07-0.6.1.0
      * fw 22.42.1000

22.11.7 Release Notes
---------------------


22.11.7 Fixes
~~~~~~~~~~~~~

* app/dumpcap: fix handling of jumbo frames
* app/dumpcap: remove unused struct array
* app/procinfo: fix leak on exit
* app/testpmd: fix aged flow destroy
* app/testpmd: remove flex item init command leftover
* app/testpmd: remove redundant policy action condition
* app/testpmd: remove unnecessary cast
* baseband/acc: fix access to deallocated mem
* baseband/acc: fix ring memory allocation
* baseband/la12xx: fix use after free in modem config
* bpf: fix free function mismatch if convert fails
* build: remove version check on compiler links function
* buildtools/chkincs: check driver specific headers
* bus/dpaa: fix lock condition during error handling
* bus/dpaa: fix PFDRs leaks due to FQRNIs
* bus/dpaa: fix the fman details status
* bus/dpaa: fix VSP for 1G fm1-mac9 and 10
* bus/fslmc: fix Coverity warnings in QBMAN
* common/cnxk: fix base log level
* common/cnxk: fix build on Ubuntu 24.04
* common/cnxk: fix CPT HW word size for outbound SA
* common/dpaax/caamflib: enable fallthrough warnings
* common/dpaax/caamflib: fix PDCP SNOW-ZUC watchdog
* common/idpf: fix use after free in mailbox init
* common/mlx5: fix error CQE handling for 128 bytes CQE
* common/mlx5: fix misalignment
* config/arm: fix warning for native build with meson >= 0.55
* crypto/bcmfs: fix free function mismatch
* crypto/dpaa2_sec: fix memory leak
* crypto/openssl: fix 3DES-CTR with big endian CPUs
* crypto/openssl: fix potential string overflow
* crypto/qat: fix modexp/inv length
* dev: fix callback lookup when unregistering device
* devtools: fix check of multiple commits fixed at once
* devtools: fix forbidden token check with multiple files
* dmadev: fix potential null pointer access
* dma/idxd: fix free function mismatch in device probe
* doc: correct definition of stats per queue feature
* eal/unix: optimize thread creation
* eal/x86: fix 32-bit write combining store
* ethdev: fix overflow in descriptor count
* ethdev: verify queue ID in Tx done cleanup
* event/cnxk: fix free function mismatch in port config
* event/cnxk: fix Rx timestamp handling
* eventdev: fix possible array underflow/overflow
* event/octeontx: fix possible integer overflow
* examples/eventdev: fix queue crash with generic pipeline
* examples/ipsec-secgw: fix dequeue count from cryptodev
* examples/l2fwd-event: fix spinlock handling
* examples/l3fwd: fix read beyond boundaries
* examples/ntb: check info query return
* examples/vhost: fix free function mismatch
* fib6: add runtime checks in AVX512 lookup
* fib: fix AVX512 lookup
* hash: fix thash LFSR initialization
* member: fix choice of bucket for displacement
* net/bnx2x: fix always true expression
* net/bnx2x: fix duplicate branch
* net/bnx2x: fix possible infinite loop at startup
* net/bnx2x: remove dead conditional
* net/bnxt: fix bad action offset in Tx BD
* net/bnxt: fix reading SFF-8436 SFP EEPROMs
* net/bnxt: fix TCP and UDP checksum flags
* net/bnxt/tf_core: fix Thor TF EM key size check
* net/bnxt/tf_ulp: fix parent child DB counters
* net/cnxk: fix build on Ubuntu 24.04
* net/cnxk: fix Rx offloads to handle timestamp
* net/cnxk: fix Rx timestamp handling for VF
* net/dpaa2: fix memory corruption in TM
* net/dpaa2: remove unnecessary check for null before free
* net/dpaa: fix reallocate mbuf handling
* net/dpaa: fix typecasting channel ID
* net/e1000: fix link status crash in secondary process
* net/e1000: fix use after free in filter flush
* net/ena: revert redefining memcpy
* net/gve/base: fix build with Fedora Rawhide
* net/hns3: fix crash for NEON and SVE
* net/hns3: fix dump counter of registers
* net/hns3: fix error code for repeatedly create counter
* net/hns3: fix fully use hardware flow director table
* net/hns3: remove ROH devices
* net/hns3: remove some basic address dump
* net/hns3: restrict tunnel flow rule to one header
* net/hns3: verify reset type from firmware
* net/i40e/base: fix blinking X722 with X557 PHY
* net/i40e/base: fix DDP loading with reserved track ID
* net/i40e/base: fix loop bounds
* net/i40e/base: fix misleading debug logs and comments
* net/i40e/base: fix repeated register dumps
* net/i40e/base: fix setting flags in init function
* net/i40e/base: fix unchecked return value
* net/i40e: check register read for outer VLAN
* net/i40e: fix AVX-512 pointer copy on 32-bit
* net/iavf: add segment-length check to Tx prep
* net/iavf: fix AVX-512 pointer copy on 32-bit
* net/iavf: fix crash when link is unstable
* net/iavf: preserve MAC address with i40e PF Linux driver
* net/ice/base: add bounds check
* net/ice/base: fix iteration of TLVs in Preserved Fields Area
* net/ice/base: fix link speed for 200G
* net/ice/base: fix VLAN replay after reset
* net/ice: detect stopping a flow director queue twice
* net/ice: fix AVX-512 pointer copy on 32-bit
* net/idpf: fix AVX-512 pointer copy on 32-bit
* net/ionic: fix build on Fedora Rawhide
* net/ixgbe/base: fix unchecked return value
* net/ixgbe: fix link status delay on FreeBSD
* net/mana: support rdma-core via pkg-config
* net/memif: fix buffer overflow in zero copy Rx
* net/mlx5: fix counter query loop getting stuck
* net/mlx5: fix default RSS flows creation order
* net/mlx5: fix flex item header length field translation
* net/mlx5: fix GRE flow item translation for root table
* net/mlx5: fix memory leak in metering
* net/mlx5: fix miniCQEs number calculation
* net/mlx5: fix next protocol validation after flex item
* net/mlx5: fix non full word sample fields in flex item
* net/mlx5: fix number of supported flex parsers
* net/mlx5: fix real time counter reading from PCI BAR
* net/mlx5: fix reported Rx/Tx descriptor limits
* net/mlx5: fix Rx queue reference count in flushing flows
* net/mlx5: fix shared queue port number in vector Rx
* net/mlx5: fix SQ flow item size
* net/mlx5: fix SWS meter state initialization
* net/mlx5/hws: fix allocation of STCs
* net/mlx5: update flex parser arc types support
* net/mlx5: workaround list management of Rx queue control
* net/mvneta: fix possible out-of-bounds write
* net/netvsc: fix using Tx queue higher than Rx queues
* net/netvsc: force Tx VLAN offload on 801.2Q packet
* net/nfb: fix use after free
* net/nfp: fix double free in flow destroy
* net/nfp: fix link change return value
* net/ngbe: fix driver load bit to inform firmware
* net/ngbe: fix interrupt lost in legacy or MSI mode
* net/ngbe: reconfigure more MAC Rx registers
* net/ngbe: restrict configuration of VLAN strip offload
* net/pcap: fix blocking Rx
* net/pcap: set live interface as non-blocking
* net/sfc: fix use after free in debug logs
* net/tap: avoid memcpy with null argument
* net/tap: restrict maximum number of MP FDs
* net/txgbe: fix driver load bit to inform firmware
* net/txgbe: fix SWFW mbox
* net/txgbe: fix VF-PF mbox interrupt
* net/txgbe: remove outer UDP checksum capability
* net/virtio: fix Rx checksum calculation
* net/virtio-user: reset used index counter
* net/vmxnet3: fix crash after configuration failure
* net/vmxnet3: fix potential out of bounds stats access
* net/vmxnet3: support larger MTU with version 6
* pcapng: fix handling of chained mbufs
* power: enable CPPC
* power: fix log message when checking lcore ID
* power: fix mapped lcore ID
* raw/ifpga/base: fix use after free
* raw/ifpga: fix free function mismatch in interrupt config
* Revert "test/bonding: fix loop on members"
* test/bonding: fix loop on members
* test/bonding: fix MAC address comparison
* test/bonding: remove redundant info query
* test/crypto: fix synchronous API calls
* test/eal: fix lcore check
* test/eal: fix loop coverage for alignment macros
* test/event: avoid duplicate initialization
* vdpa: update used flags in used ring relay
* version: 22.11.7-rc1
* vhost: fix offset while mapping log base address

22.11.7 Validation
~~~~~~~~~~~~~~~~~~

* Red Hat(R) Testing

   * Platform

      * RHEL 9.2
      * Kernel 5.14
      * Qemu 7.2.0
      * libvirt 9.0
      * openvswitch 3.1
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server: PASS
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Nvidia(R) Testing

   * Basic functionality via testpmd/example applications

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split tests
      * Tx scheduling tests

   * Build tests

      * Debian 12 with MLNX_OFED_LINUX-24.10-1.1.4.0.
      * Ubuntu 22.04 with MLNX_OFED_LINUX-24.10-1.1.4.0.
      * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-1.1.4.0.
      * Ubuntu 24.04 with rdma-core v50.0.
      * Fedora 40 with rdma-core v48.0.
      * Fedora 42 (Rawhide) with rdma-core v51.0.
      * OpenSUSE Leap 15.6 with rdma-core v49.1.

   * BlueField-2

      * DOCA 2.9.1
      * fw 24.43.2026

   * ConnectX-7

      * Ubuntu 22.04
      * Driver MLNX_OFED_LINUX-24.10-1.1.4.0
      * fw 28.43.2026

   * ConnectX-6 Dx

      * Ubuntu 22.04
      * Driver MLNX_OFED_LINUX-24.10-1.1.4.0
      * fw 22.43.2026


* Intel(R) Testing

   * Basic Intel(R) NIC testing
      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.04, Ubuntu24.10, Fedora40, RHEL8.10, RHEL9.4, FreeBSD14.1, SUSE15, Centos7.9, AzureLinux3.0, OpenAnolis8.9 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PPF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing
      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:
         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.

22.11.8 Release Notes
---------------------


22.11.8 Fixes
~~~~~~~~~~~~~

* app/test: fix a merge problem in oop patch
* app/testpmd: avoid crash in DCB config
* app/testpmd: fix out-of-bound reference in offload config
* app/testpmd: show all DCB priority TC map
* build: force GCC 15 to initialize padding bits
* bus/pci: fix registered device name
* ci: point at GitHub mirror
* common/cnxk: fix atomic load in batch ops
* common/cnxk: fix DPI mailbox structure
* common/cnxk: fix inbound IPsec SA setup
* common/cnxk: fix null check
* common/idpf: fix void function returning a value
* crypto/cnxk: fix build with GCC 15
* crypto/dpaa2_sec: fix bitmask truncation
* crypto/dpaa_sec: fix bitmask truncation
* crypto/openssl: fix CMAC auth context update
* crypto/openssl: validate incorrect RSA signature
* crypto/virtio: fix data queues iteration
* crypto/virtio: fix redundant queue free
* doc: fix feature flags for queue start/stop
* doc: update ionic driver guide
* eal: fix devargs layers parsing out of bounds
* eal: fix undetected NUMA nodes
* eal/linux: fix memseg length in legacy mem init
* eal/linux: remove useless assignments
* eal/x86: fix some intrinsics header include for Windows
* ethdev: convert string initialization
* ethdev: fix functions available in new device event
* event/dpaa: fix bitmask truncation
* examples/ipsec-secgw: fix cryptodev and eventdev IDs
* examples/ipsec-secgw: fix IV length in CTR 192/256
* examples/l3fwd: fix socket ID check
* examples/ptpclient: fix message parsing
* examples/vhost_crypto: fix user callbacks
* mempool: fix errno in empty create
* net/af_packet: fix socket close on device stop
* net/bnxt: fix crash when representor is re-attached
* net/bnxt: fix indication of allocation
* net/bonding: fix dedicated queue setup
* net/cnxk: fix NIX send header L3 type
* net/dpaa2: fix bitmask truncation
* net/dpaa: fix bitmask truncation
* net/e1000/base: correct mPHY access logic
* net/e1000/base: fix bitwise operation type
* net/e1000/base: fix data type in MAC hash
* net/e1000/base: fix iterator type
* net/e1000/base: fix MAC address hash bit shift
* net/e1000/base: fix NVM data type in bit shift
* net/e1000/base: fix reset for 82580
* net/e1000/base: fix semaphore timeout value
* net/e1000/base: fix unchecked return
* net/e1000/base: fix uninitialized variable
* net/e1000/base: skip management check for 82575
* net/e1000: fix crashes in secondary processes
* net/enetfec: remove useless assignment
* net/hinic: fix flow type bitmask overflow
* net/hns3: fix copper port initialization
* net/hns3: fix mbuf freeing in simple Tx path
* net/hns3: fix reset timeout
* net/hns3: remove PVID info dump for VF
* net/hns3: rename RAS module
* net/i40e: remove duplicate code
* net/iavf: check interrupt registration failure
* net/iavf: fix crash on app exit on FreeBSD
* net/iavf: fix mbuf release in Arm multi-process
* net/iavf: remove reset of Tx prepare function pointer
* net/ice: fix dropped packets when using VRRP
* net/ice: fix memory leak in scalar Rx
* net/igc/base: fix bitwise operation type
* net/igc/base: fix data type in MAC hash
* net/igc/base: fix deadlock when writing i225 register
* net/igc/base: fix infinite loop
* net/igc/base: fix iterator type
* net/igc/base: fix LTR for i225
* net/igc/base: fix MAC address hash bit shift
* net/igc/base: fix NVM data type in bit shift
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
* net/mlx5: adjust actions per rule limitation
* net/mlx5: fix actions translation error overwrite
* net/mlx5: fix flush of non-template flow rules
* net/mlx5: fix GRE flow match with SWS
* net/mlx5: fix GRE matching on root table
* net/mlx5: fix hardware packet type translation
* net/mlx5: fix IPIP tunnel verification
* net/mlx5: fix LACP packet handling in isolated mode
* net/mlx5: fix leak in HWS flow counter action
* net/mlx5: fix Netlink socket leak
* net/mlx5: fix polling CQEs
* net/mlx5/hws: fix crash using represented port without ID
* net/mlx5/hws: fix GTP flags matching
* net/netvsc: remove device if its net devices removed
* net/netvsc: scan all net devices under the PCI device
* net/qede: fix debug messages array
* net/qede: fix nested loops
* net/sfc: remove unnecessary assignment
* net/thunderx/base: fix build with GCC 15
* net/txgbe: remove useless condition for SW-FW sync
* pdump: clear statistics when enabled
* raw/cnxk_gpio: fix file descriptor leak
* Revert "examples/vhost_crypto: fix user callbacks"
* stack: fix pop in C11 implementation
* test/bbdev: update FFT test vectors
* test/bonding: fix active backup receive test
* test/crypto: fix AES-ECB test lengths
* test/crypto: fix check for OOP header data
* test/crypto: remove unused variable
* test/dma: fix pointers in IOVA as PA mode
* test/event: fix number of queues in eventdev conf
* test/ring: fix init with custom number of lcores
* use Python raw string notation
* version: 22.11.8-rc1
* vhost: add null callback checks
* vhost: check descriptor chains length
* vhost: check GSO size validity
* vhost: clear ring addresses when getting vring base

22.11.8 Validation
~~~~~~~~~~~~~~~~~~

* Red Hat(R) Testing

   * Platform

      * RHEL 9.2
      * Kernel 5.14
      * Qemu 7.2.0
      * libvirt 9.0
      * openvswitch 3.1
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server: PASS
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Intel(R) Testing

   * Basic Intel(R) NIC testing
      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.04, Ubuntu24.10, Fedora41, RHEL10, RHEL9.4, FreeBSD14.2, SUSE15, Centos7.9, AzureLinux3.0, OpenAnolis8.9 etc.
      * PF(i40e, ixgbe): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PPF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing
      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:
         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.


* Nvidia(R) Testing

   * Basic functionality via testpmd/example applications

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split tests
      * Tx scheduling tests

   * Build tests

      * Ubuntu 22.04 with MLNX_OFED_LINUX-24.10-2.1.8.0.
      * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-2.1.8.0.
      * Ubuntu 22.04 with rdma-core master (9e9a957).
      * Ubuntu 24.04 with rdma-core v50.0.
      * Fedora 41 with rdma-core v51.0.
      * Fedora 43 (Rawhide) with rdma-core v56.0.
      * OpenSUSE Leap 15.6 with rdma-core v49.1.

   * BlueField-2

      * DOCA 2.10.0
      * fw 24.44.1036

   * ConnectX-6 Dx

      * Ubuntu 22.04
      * Driver MLNX_OFED_LINUX-24.10-2.1.8.0
      * fw 22.43.2566

22.11.9 Release Notes
---------------------


22.11.9 Fixes
~~~~~~~~~~~~~

* acl: fix build with GCC 15 on aarch64
* app/crypto-perf: fix AAD offset alignment
* app/eventdev: fix number of releases sent during cleanup
* app/testpmd: fix RSS hash key update
* app/testpmd: relax number of TCs in DCB command
* bus/auxiliary: fix crash in cleanup
* bus: cleanup device lists
* bus/fslmc: fix use after free
* bus/pci/bsd: fix device existence check
* bus/vmbus: align ring buffer data to page boundary
* bus/vmbus: use Hyper-V page size
* ci: bump tested distributions in GHA
* common/cnxk: fix CQ tail drop
* common/cnxk: fix E-tag pattern parsing
* common/cnxk: fix null pointer checks
* common/cnxk: fix qsize in CPT iq enable
* common/dpaax: fix PDCP AES only 12-bit SN
* common/dpaax: fix PDCP key command race condition
* common/mlx5: fix extraction of auxiliary device name
* config/arm: add grace build config
* crypto/mlx5: adjust CQE error struct name
* crypto/openssl: include private exponent in RSA session
* crypto/qat: fix out-of-place chain/cipher/auth headers
* crypto/qat: fix out-of-place chain/cipher/auth headers
* crypto/qat: fix out-of-place header bytes in AEAD raw API
* crypto/virtio: add request check on request side
* crypto/virtio: fix cipher data source length
* crypto/virtio: fix driver cleanup
* doc: remove reference to deprecated --use-device option
* eal: add description of service corelist in usage
* eal: fix return value of lcore role
* eal/freebsd: unregister alarm callback before free
* eal/linux: improve ASLR check
* eal/linux: unregister alarm callback before free
* eal/unix: fix log message for madvise failure
* eal: warn if no lcore is available
* ethdev: fix error struct in flow configure
* ethdev: keep promiscuous/allmulti value before disabling
* event/dlb2: fix default credits based on HW version
* event/dlb2: fix dequeue with CQ depth <= 16
* event/dlb2: fix num single link ports for DLB2.5
* event/dlb2: fix validaton of LDB port COS ID arguments
* examples/ipsec-secgw: fix crash in event vector mode
* examples/ipsec-secgw: fix crash with IPv6
* examples/ipsec-secgw: fix number of queue pairs
* examples/multi_process: fix ports cleanup on exit
* examples/multi_process: revert ports cleanup on exit
* examples/ntb: check more heap allocations
* mem: fix lockup on address space shortage
* net/af_xdp: fix use after free in zero-copy Tx
* net/bonding: avoid RSS RETA update in flow isolation mode
* net/cnxk: fix descriptor count update on reconfig
* net/cnxk: fix lock for security session ops
* net/e1000: fix EEPROM dump
* net/e1000: fix igb Tx queue offloads capability
* net/e1000: fix xstats name
* net/fm10k/base: fix compilation warnings
* net/hns3: allow Rx vector mode with VLAN filter
* net/hns3: allow Tx vector when fast free not enabled
* net/hns3: check requirement for hardware GRO
* net/hns3: fix CRC data segment
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
* net/i40e/base: remove unused variables
* net/i40e: fix RSS on plain IPv4
* net/iavf: fix VLAN strip disabling for ADQ v2 capability
* net/iavf: fix VLAN strip setting after enabling filter
* net/ice/base: fix integer overflow
* net/ice/base: fix typo in device ID description
* net/ice: fix querying RSS hash for DCF
* net/ixgbe/base: correct definition of endianness macro
* net/ixgbe/base: fix compilation warnings
* net/ixgbe: fix indentation
* net/ixgbe: fix port mask default value in filter
* net/mana: check vendor ID when probing RDMA device
* net/mlx5: align PF and VF/SF MAC address handling
* net/mlx5: avoid setting kernel MTU if not needed
* net/mlx5: fix counter service cleanup on init failure
* net/mlx5: fix crash on age query with indirect conntrack
* net/mlx5: fix header modify action on group 0
* net/mlx5: fix hypervisor detection in VLAN workaround
* net/mlx5: fix masked indirect age action validation
* net/mlx5: fix maximal queue size query
* net/mlx5: fix VLAN stripping on hairpin queue
* net/mlx5: fix WQE size calculation for Tx queue
* net/mlx5: remove unsupported flow meter action in HWS
* net/mlx5: validate GTP PSC QFI width
* net/netvsc: add stats counters from VF
* net/netvsc: use Hyper-V page size
* net/ngbe: fix device statistics
* net/ngbe: fix MAC control frame forwarding
* net/null: fix packet copy
* net/qede: fix use after free
* net/tap: fix qdisc add failure handling
* net/txgbe: add LRO flag in mbuf when enabled
* net/txgbe: fix device statistics
* net/txgbe: fix FDIR perfect mode for IPv6
* net/txgbe: fix MAC control frame forwarding
* net/txgbe: fix ntuple filter parsing
* net/txgbe: fix packet type for FDIR filter
* net/txgbe: fix raw pattern match for FDIR rule
* net/txgbe: fix reserved extra FDIR headroom
* net/txgbe: fix to create FDIR filter for SCTP packet
* net/txgbe: fix to create FDIR filter for tunnel packet
* net/txgbe: restrict VLAN strip configuration on VF
* net/virtio: fix check of threshold for Tx freeing
* pcapng: fix null dereference in close
* power/intel_uncore: fix crash closing uninitialized driver
* Revert "net/virtio: fix check of threshold for Tx freeing"
* test/crypto: fix auth and cipher case IV length
* test/crypto: fix RSA decrypt validation
* test/crypto: set to null after freeing operation
* test/malloc: improve resiliency
* trace: fix overflow in per-lcore trace buffer
* version: 22.11.9-rc1
* vhost/crypto: fix cipher data length
* vhost: search virtqueues driver data in read-only area

22.11.9 Validation
~~~~~~~~~~~~~~~~~~

* Red Hat(R) Testing

   * Platform

      * RHEL 9.6
      * Kernel 5.14
      * Qemu 9.1.0
      * libvirt 10.10
      * openvswitch 3.3
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP  reconnect with dpdk-client, qemu-server: PASS
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Intel(R) Testing

   * Basic Intel(R) NIC testing
      * Build & CFLAG compile: cover the build test combination with latest GCC/Clang version and the popular OS revision such as Ubuntu24.04, Ubuntu25.04, Fedora42, RHEL10, RHEL9.6, FreeBSD14.2, SUSE15, AzureLinux3.0, OpenAnolis8.10 etc.
      * PF(i40e, ixgbe, igc): test scenarios including RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * VF(i40e, ixgbe): test scenarios including VF-RTE_FLOW/TSO/Jumboframe/checksum offload/VLAN/VXLAN, etc.
      * PPF/VF(ice): test scenarios including Switch features/Package Management/Flow Director/Advanced Tx/Advanced RSS/ACL/DCF/Flexible Descriptor, etc.
      * Intel NIC single core/NIC performance: test scenarios including PF/VF single core performance test, etc.
      * IPsec: test scenarios including ipsec/ipsec-gw/ipsec library basic test - QAT&SW/FIB library, etc.

   * Basic cryptodev and virtio testing
      * Virtio: both function and performance test are covered. Such as PVP/Virtio_loopback/virtio-user loopback/virtio-net VM2VM perf testing/VMAWARE ESXI 8.0, etc.
      * Cryptodev:
         * Function test: test scenarios including Cryptodev API testing/CompressDev ISA-L/QAT/ZLIB PMD Testing/FIPS, etc.
         * Performance test: test scenarios including Thoughput Performance/Cryptodev Latency, etc.


* Nvidia(R) Testing

   * Basic functionality via testpmd/example applications

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split tests
      * Tx scheduling tests

   * Build tests

      * Debian 12 with MLNX_OFED_LINUX-24.10-3.2.5.0.
      * Ubuntu 22.04 with MLNX_OFED_LINUX-24.10-3.2.5.0.
      * Ubuntu 24.04 with MLNX_OFED_LINUX-24.10-3.2.5.0.
      * Ubuntu 22.04 with rdma-core master (091ddb5).
      * Ubuntu 24.04 with rdma-core v50.0.
      * Fedora 42 with rdma-core v55.0.
      * Fedora 43 (Rawhide) with rdma-core v58.0.
      * OpenSUSE Leap 15.6 with rdma-core v49.1.
      * Windows Server 2022 with Clang 18.1.8.

   * BlueField-2

      * DOCA 2.5.4
      * fw 24.39.5050

   * ConnectX-7

      * Ubuntu 22.04
      * Driver MLNX_OFED_LINUX-24.10-2.1.8.0
      * fw 28.43.2566

   * ConnectX-6 Dx

      * Ubuntu 22.04
      * Driver MLNX_OFED_LINUX-24.10-2.1.8.0
      * fw 22.43.2566
