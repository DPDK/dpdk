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

* **Updated Intel iavf driver.**

  * Added flow subscription support.

* **Updated Marvell cnxk driver.**

  * Added support for flow action REPRESENTED_PORT.

* **Updated NXP dpaa2 driver.**

  * Added support for flow action REPRESENTED_PORT.

* **Updated Wangxun ngbe driver.**

  * Added support to set device link down/up.

* **Added support for MACsec in rte_security.**

  Added MACsec transform for rte_security session and added new API
  to configure security associations (SA) and secure channels (SC).

* **Updated Marvell cnxk crypto driver.**

  * Added AES-CCM support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES & DES DOCSIS algorithm support in lookaside crypto for CN9K.

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


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_VF``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ITEM_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT``.

* ethdev: removed ``RTE_FLOW_ACTION_TYPE_PHY_PORT``;
  use ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT``.

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

* security: MACsec support is added which resulted in updates
  to structures ``rte_security_macsec_xform``, ``rte_security_macsec_stats``
  and security capability structure ``rte_security_capability``
  to accommodate MACsec capabilities.

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

* ethdev: enum ``RTE_FLOW_ITEM`` was affected by deprecation procedure.

* ethdev: enum ``RTE_FLOW_ACTION`` was affected by deprecation procedure.

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
