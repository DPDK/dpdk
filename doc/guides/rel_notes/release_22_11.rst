.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_22_11.html


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
     * Device abstraction libs and PMDs (ordered alphabetically by vendor name)
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =======================================================

* **Added initial LoongArch architecture support.**

  Added EAL implementation for LoongArch architecture.
  The initial devices the porting was tested on included Loongson 3A5000,
  Loongson 3C5000 and Loongson 3C5000L.
  In theory this implementation should work with any target based on
  ``LoongArch`` ISA.

* **Added support for multiple mbuf pools per ethdev Rx queue.**

  The capability allows application to provide many mempools
  of different size, and PMD and/or NIC to choose a memory pool
  based on the packet's length and/or Rx buffers availability.

* **Added support for congestion management in ethdev.**

  Added new API functions ``rte_eth_cman_config_init()``,
  ``rte_eth_cman_config_get()``, ``rte_eth_cman_config_set()``,
  ``rte_eth_cman_info_get()`` to support congestion management.

* **Added protocol header based buffer split.**

  * Added ``rte_eth_buffer_split_get_supported_hdr_ptypes()`` to get supported
    header protocols to split at.
  * Supported protocol-based buffer split using added ``proto_hdr``
    in structure ``rte_eth_rxseg_split``.

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

* **Added configuration for asynchronous flow connection tracking.**

  Added connection tracking action number hint to ``rte_flow_configure``
  and ``rte_flow_info_get``.
  PMD can prepare the connection tracking resources according to the hint.

* **Added support for queue-based async query in flow API.**

  Added new function ``rte_flow_async_action_handle_query()``,
  to query the action asynchronously.

* **Extended metering and marking support in the flow API.**

  * Added METER_COLOR item to match color marker set by a meter.
  * Added ability to set color marker via modify field flow API.
  * Added meter API to get a pointer to profile/policy by their ID.
  * Added METER_MARK action for metering with lockless profile/policy access.

* **Added flow offload action to route packets to kernel.**

  Added new flow action which allows application to re-route packets
  directly to the kernel without software involvement.

* **Updated AF_XDP driver.**

  * Made compatible with libbpf v0.8.0 (when used with libxdp).

* **Updated Intel iavf driver.**

  * Added flow subscription support.

* **Updated Intel ice driver.**

  * Added protocol based buffer split support in scalar path.

* **Updated Marvell cnxk driver.**

  * Added support for flow action REPRESENTED_PORT.

* **Added Microsoft mana driver.**

* **Updated Netronome nfp driver.**

  Added the needed data structures and logics to support flow API offload:

  * Added the support of flower firmware.
  * Added the flower service infrastructure.
  * Added the control message interactive channels between PMD and firmware.
  * Added the support of representor port.

* **Updated NXP dpaa2 driver.**

  * Added support for flow action REPRESENTED_PORT.

* **Updated Wangxun ngbe driver.**

  * Added support to set device link down/up.

* **Added support for MACsec in rte_security.**

  Added MACsec transform for rte_security session and added new API
  to configure security associations (SA) and secure channels (SC).

* **Added new algorithms to cryptodev.**

  * Added symmetric hash algorithm ShangMi 3 (SM3).
  * Added symmetric cipher algorithm ShangMi 4 (SM4) in ECB, CBC and CTR modes.

* **Updated Intel QuickAssist Technology (QAT) symmetric crypto driver.**

  * Added support for SM3 hash algorithm.
  * Added support for SM4 encryption algorithm in ECB, CBC and CTR modes.

* **Updated Marvell cnxk crypto driver.**

  * Added AES-CCM support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES & DES DOCSIS algorithm support in lookaside crypto for CN9K.

* **Updated aesni_mb crypto driver.**

  * Added support for 8-byte and 16-byte tags for ZUC-EIA3-256.

* **Added bbdev operation for FFT processing.**

  Added a new operation type in bbdev for FFT processing with new functions
  ``rte_bbdev_enqueue_fft_ops``, ``rte_bbdev_dequeue_fft_ops``,
  and related structures.

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

  Added support to aggregate crypto operations processed by event crypto adapter
  into single event containing ``rte_event_vector``
  whose event type is ``RTE_EVENT_TYPE_CRYPTODEV_VECTOR``.

* **Added NitroSketch in membership library.**

  Added a new data structure called sketch into membership library,
  to profile the traffic efficiently.
  NitroSketch provides high-fidelity approximate measurements
  and appears as a promising alternative to traditional approaches
  such as packet sampling.

* **Added Intel uncore frequency control API to the power library.**

  Added API to allow uncore frequency adjustment.
  This is done through manipulating related uncore frequency control
  sysfs entries to adjust the minimum and maximum uncore frequency values,
  which works on Linux with Intel hardware only.

* **Rewritten pmdinfo script.**

  The ``dpdk-pmdinfo.py`` script was rewritten to produce valid JSON only.
  PCI-IDs parsing has been removed.
  To get a similar output to the (now removed) ``-r/--raw`` flag,
  the following command may be used:

  .. code-block:: sh

     strings $dpdk_binary_or_driver | sed -n 's/^PMD_INFO_STRING= //p'


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* mem: Removed not implemented and deprecated ``rte_malloc_set_limit``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_VF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ACTION_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``OF_SET_MPLS_TTL``, ``OF_DEC_MPLS_TTL``,
  ``OF_SET_NW_TTL``, ``OF_COPY_TTL_OUT`` and ``OF_COPY_TTL_IN``
  which are not actually supported by any PMD.
  ``MODIFY_FIELD`` action should be used to do packet edits via flow API.

* vhost: Removed deprecated ``rte_vhost_gpa_to_vva`` and
  ``rte_vhost_get_queue_num`` helpers.


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
   =======================================================

* eal: RTE_FUNC_PTR_OR_* macros have been marked deprecated and will be removed
  in the future. Applications can use ``devtools/cocci/func_or_ret.cocci``
  to update their code.

* eal: Updated ``rte_eal_remote_launch`` so it returns -EPIPE in case of
  a read or write error on the pipe, instead of calling ``rte_panic``.

* eal: Updated return types for rte_{bsf,fls} inline functions
  to be consistently ``uint32_t``.

* mempool: Deprecated helper macro ``MEMPOOL_HEADER_SIZE()`` is removed.
  The replacement macro ``RTE_MEMPOOL_HEADER_SIZE()`` is internal only.

* mempool: Deprecated macro to register mempool driver
  ``MEMPOOL_REGISTER_OPS()`` is removed. Use replacement macro
  ``RTE_MEMPOOL_REGISTER_OPS()`` instead.

* mempool: Deprecated macros ``MEMPOOL_PG_NUM_DEFAULT`` and
  ``MEMPOOL_PG_SHIFT_MAX`` are removed. These macros are not used and
  not required any more.

* mbuf: Removed deprecated ``PKT_*`` flags.
  Use corresponding flags with ``RTE_MBUF_F_`` prefix instead.
  Application can use ``devtools/cocci/prefix_mbuf_offload_flags.cocci``
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
  flows, as the final step of deprecation process that had been started
  in DPDK 21.11. See items ``PORT_REPRESENTOR``, ``REPRESENTED_PORT``.

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

* security: MACsec support is added which resulted in updates
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

.. This section should contain ABI changes. Sample format:

   * sample: Add a short 1-2 sentence description of the ABI change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

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


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


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
   =======================================================
