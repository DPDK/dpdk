.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2026 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 26.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_26_11.html


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

* **Updated AF_XDP driver.**

  * Changed the default device plugin endpoint path used when
    ``use_cni`` or ``use_pinned_map`` is set without ``dp_path``.
    See :doc:`../nics/af_xdp` for more details.
  * Added support for Rx metadata hardware timestamping via vdev devargs
    ``xdp_meta_rx_ts_offset``, ``xdp_meta_valid_hint_offset``, and
    ``xdp_meta_rx_ts_valid_mask``.
  * Added ``read_clock`` operation to query the PTP hardware clock.

* **Updated Solarflare network driver.**

  * Added VF support on AMD Solarflare X45xx adapters.


* **Updated NXP dpaa2 driver.**

  * Added the inner IP header to the RSS hash so tunneled traffic is
    distributed across the Rx queues.

* **Added TPID support to VLAN tag insertion.**

  Added ``rte_vlan_insert_tpid()`` to the net library.

* **Updated ZTE zxdh ethernet driver.**

  * Added a fast single-segment Rx path (``zxdh_recv_single_pkts``) that
    is selected when the MTU fits in a single buffer.
  * Optimized the packed-ring Rx recv path.
  * Changed the set of per-queue xstats counters.
  * Optimized the packed-ring Tx xmit path with per-descriptor mbuf
    free (``rte_pktmbuf_free_seg``) and prefetch hints.


* **Updated NXP ENETC4 PMD.**

  Updated the NXP ENETC4 poll mode driver for i.MX95:

  * Added KEEP_CRC Rx offload support for the ENETC4 PMD to preserve the Ethernet FCS.
  * Added TCP Segmentation Offload (TSO) support for the ENETC4 VF.
  * Added Receive Segment Coalesce (RSC / hardware LRO) support for ENETC4 PF and VF.
  * Extended the PF-to-VF link speed code field from 4-bit to 8-bit in ENETC4.
    Users running a PF kernel older than 6.18.37 must pass ``vf_link_legacy=1``
    as a device argument, otherwise link speed reporting will be incorrect.
  * Added firmware version reporting for the ENETC4 VF.
  * Added register dump support for ENETC4 PF and VF.
  * Added ring parameters support for the ENETC4 VF (rxq_info_get / txq_info_get).
  * Refreshed VF link speed on the link-up interrupt in the ENETC4 VF driver.
  * Added stats reset for the ENETC4 VF using a software snapshot/delta approach.
  * Added per-queue MSI-X Rx interrupt support for the ENETC4 VF.
  * Added SI-based port VLAN insertion (Tx) and removal (Rx) for ENETC4 PF and VF.
  * Updated ENETC4 VF link status reporting to use bitmask encoding.
  * Added Tx PAUSE support for the ENETC4 VF via Rx congestion mode.
  * Added WRR Tx scheduler devarg (``enetc4_txq_wrr``) for ENETC4 VF ring weights.

Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* Removed deprecated symbols:

  * eal: ``__rte_packed``
  * fib: ``RTE_FIB6_IPV6_ADDR_SIZE``, ``RTE_FIB6_MAXDEPTH``
  * lpm: ``RTE_LPM6_IPV6_ADDR_SIZE``, ``RTE_LPM6_MAX_DEPTH``
  * net: ``RTE_IP_ICMP_ECHO_REPLY``, ``RTE_IP_ICMP_ECHO_REQUEST``
  * pci: ``PCI_ID_ANY``
  * rib: ``RTE_RIB6_IPV6_ADDR_SIZE``, ``get_msk_part``, ``rte_rib6_copy_addr``,
    ``rte_rib6_is_equal``
  * table: ``RTE_LPM_IPV6_ADDR_SIZE``

* ethdev: Removed support for ethdev queue stats mapping.

  ``rte_eth_dev_set_tx_queue_stats_mapping`` and ``rte_eth_dev_set_rx_queue_stats_mapping``
  were deprecated and are now removed.

* ethdev: Removed the ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` build time limit.
  Per-queue xstats are now reported for all queues, not just the first 16.
  The ``rx_qN_errors`` xstat is removed, drops are still counted in ``ierrors``.

* Removed the ``dpdk-test-pipeline`` application, which was based on
  the legacy pipeline library API.

* Removed the ``ip_pipeline`` example application, which was based on
  the legacy pipeline library API.
  The ``pipeline`` example application covers the SWX pipeline API.

* Removed the legacy pipeline library API: ``rte_pipeline_*``,
  ``rte_port_in_action_*`` and ``rte_table_action_*`` functions.
  The SWX pipeline API (``rte_swx_pipeline_*``) remains.

* Removed the legacy table library API (``rte_table_*`` functions).
  The SWX table API (``rte_swx_table_*``) remains.

* Removed the legacy port library API (``rte_port_*`` functions).
  The SWX port API (``rte_swx_port_*``) remains.


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

* **ethdev: updated VMDq related API.**

  * At port configuration time, the number of VMDq pools advertised by a driver is now used to
    validate VMDq related Rx and Tx modes (``RTE_ETH_MQ_RX_VMDQ_FLAG``, ``RTE_ETH_MQ_TX_VMDQ_DCB``,
    ``RTE_ETH_MQ_TX_VMDQ_ONLY``).
  * A check was added in ``rte_eth_dev_mac_addr_add`` to validate that the ``pool``
    parameter is 0 when VMDq is not configured.
  * The ``RTE_ETH_NUM_RECEIVE_MAC_ADDR`` and ``RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY`` macros are VMDq
    related and are sizes of internal arrays in ethdev that only drivers need to care about.
    Those macros are moved to the driver only ethdev API.

* ethdev: Promoted the following API from experimental to stable:

  * meter (MTR) and policing: ``rte_mtr_*``
  * SFF: ``rte_eth_dev_get_module_info`` and ``rte_eth_dev_get_module_eeprom``
  * flow conversion: ``rte_flow_conv``
  * hairpin queue: ``rte_eth_rx_hairpin_queue_setup``,
    ``rte_eth_tx_hairpin_queue_setup``,
    ``rte_eth_dev_hairpin_capability_get``,
    ``rte_eth_hairpin_bind``,
    ``rte_eth_hairpin_unbind``,
    ``rte_eth_hairpin_get_peer_ports``
  * clock: ``rte_eth_read_clock``
  * flow dump: ``rte_flow_dev_dump``
  * FEC: ``rte_eth_fec_get_capability``, ``rte_eth_fec_get`` and ``rte_eth_fec_set``
  * link speed: ``rte_eth_link_speed_to_str`` and ``rte_eth_link_to_str``
  * flow tunnel: ``rte_flow_tunnel_decap_set``, ``rte_flow_tunnel_match``,
    ``rte_flow_tunnel_item_release``,
    ``rte_flow_tunnel_action_decap_release`` and
    ``rte_flow_get_restore_info``
  * flow age: ``rte_flow_get_aged_flows`` and ``rte_flow_get_q_aged_flows``
  * flow action: ``rte_flow_action_handle_*`` and ``rte_flow_action_list_handle_*``
  * flow template:
    ``rte_flow_configure``, ``rte_flow_info_get``,
    ``rte_flow_pattern_*``,
    ``rte_flow_actions_*``,
    ``rte_flow_template_*``,
    ``rte_flow_async_*``,
    ``rte_flow_push``, ``rte_flow_pull``


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
