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

* **Added division round up macro.**

  Added the ``RTE_DIV_ROUND_UP`` macro in ``rte_common.h``
  to divide dividend by divisor, rounding up to the nearest integer.

* **Improved pointer arithmetic macros.**

  Updated  the macros ``RTE_PTR_ADD``, ``RTE_PTR_SUB``,
  ``RTE_PTR_ALIGN``, ``RTE_PTR_ALIGN_CEIL`` and ``RTE_PTR_ALIGN_FLOOR``
  to preserve const/volatile qualifiers
  and use pointer arithmetic instead of integer casts to enable compiler optimizations.
  See API changes below.

* **Added API to get CPU socket ID.**

  Added the experimental ``rte_cpu_socket_id()`` function
  to map an OS logical CPU ID to the NUMA socket containing that CPU.

* **Added pile stack and mempool driver.**

  Introduced pile, a lock-free, stack-like implementation,
  optimized for bulk operations.
  The pile is only LIFO on bulk level, not on object level;
  i.e. arrays of bulks are pushed and popped in LIFO manner,
  but objects within each bulk are not ordered as expected by a stack.
  Furthermore, it is not strictly bounded by its size, but might hold more objects.

* **Added unplug operation support to DPAA bus.**

  Implemented device unplug operation to allow runtime removal of DPAA devices.

* **Added TPID support to VLAN tag insertion.**

  Added ``rte_vlan_insert_tpid()`` to the net library.

* **Added VFIO cdev (IOMMUFD) mode.**

  Added support for the VFIO character device (cdev) API, also known as IOMMUFD.
  DPDK now supports both the group mode and the new cdev mode.

* **Added support for device-tree compatible matching in the platform bus.**

  The platform bus now matches DPDK drivers against the device-tree ``compatible`` sysfs strings
  exposed by each platform device, in addition to the existing kernel driver name and
  device name checks.
  This allows drivers to be probed by DT compatible string when
  the kernel driver is the generic ``vfio-platform``,
  which carries no device identity in its name.

* **Added module EEPROM decoding API to ethdev.**

  Added the experimental ``rte_eth_module_eeprom_parse()`` function
  to decode pluggable module EEPROM data according to the SFF specifications.
  It can decode data read with ``rte_eth_dev_get_module_eeprom()``
  or obtained from any other source with the same layout,
  such as the Linux ethtool ``ETHTOOL_GMODULEEEPROM`` ioctl.
  The testpmd command ``show port (port_id) module_eeprom decode``
  uses it to display the decoded module EEPROM.
  The SFF-8636 decoder also reports the per-lane loss of signal,
  loss of lock and Tx fault flags.
  The SFF-8472 decoder also reports the Rx loss of signal and Tx fault state.

* **Added flow item default mask and size lookup.**

  Added ``RTE_FLOW_CONV_OP_ITEM_DEFAULT_MASK_PTR`` and
  ``RTE_FLOW_CONV_OP_ITEM_SIZE`` conversion operations
  to ``rte_flow_conv()`` to retrieve the default mask
  and the specification structure size of a flow item type.

* **Added internal ethdev flow graph parser helper API.**

  Added internal ``flow_graph`` API in ethdev
  for PMD drivers to build graph-based flow pattern parsers.

* **Updated AF_PACKET driver.**

  Added ``ignore_outgoing`` devarg to only receive incoming packets.

* **Updated AF_XDP driver.**

  * Changed the default device plugin endpoint path used when
    ``use_cni`` or ``use_pinned_map`` is set without ``dp_path``.
    See :doc:`../nics/af_xdp` for more details.
  * Added support for Rx metadata hardware timestamping via vdev devargs
    ``xdp_meta_rx_ts_offset``, ``xdp_meta_valid_hint_offset``, and
    ``xdp_meta_rx_ts_valid_mask``.
  * Added ``read_clock`` operation to query the PTP hardware clock.

* **Updated Broadcom bnxt driver.**

  * Added ``tx_dma_err_cmpl`` statistics to report Tx completions
    that the device flagged with a DMA error.
    This is a port-level counter,
    and is also folded into the standard ``oerrors`` counter.
  * Raised the maximum Tx and Rx ring descriptor counts
    from 4096/8192 to 16384 each.

* **Updated Intel iavf driver.**

  * Runtime Rx/Tx queue setup is now automatically disabled
    while a per-queue bandwidth ``rte_tm`` hierarchy is committed,
    to avoid corrupting queue state on E810 VFs.
  * Renamed the ``enable_ptype_lldp`` devarg to ``enable_lldp``.
    The old name is no longer accepted.
  * Increased the maximum number of secondary unicast MAC addresses from 64 to 32k.
    This increases a VF port memory footprint by ~192kB.

* **Updated Intel ice driver.**

  * Added Tx context descriptor support to the AVX2 and AVX512 vector Tx paths,
    enabling QinQ tag insertion and outer IPv4/UDP checksum offloads on those paths.
  * Added support for getting and setting link (802.3x) flow control.
  * Added support for Tx rate limiting per queue.

* **Updated Intel ixgbe driver.**

  Added ``fdir_buffer_size`` devarg to select the Flow Director table size
  (``64k``, ``128k`` or ``256k``) at probe time. The default remains ``64k``.

* **Updated NVIDIA mlx5 ethernet driver.**

  * Increased the maximum number of secondary unicast MAC addresses from 128 to up to 4096
    (depending on devlink configuration on the associated kernel netdevice).

* **Updated NXP DPAA2 ethernet driver.**

  * Added the inner IP header to the RSS hash so tunneled traffic is
    distributed across the Rx queues.
  * Removed ``DPAA2_FLOW_CONTROL_MISS_FLOW`` environment variable. The
    flow steering miss queue now defaults to the last Rx queue configured
    in the traffic class. Users who previously redirected unmatched
    frames to a specific queue via this variable must use the rte_flow
    API instead.

* **Updated NXP ENETC4 PMD.**

  Updated the NXP ENETC4 poll mode driver for i.MX95:

  * Added ``RTE_ETH_RX_OFFLOAD_KEEP_CRC`` support
    for the ENETC4 PMD to preserve the Ethernet FCS.
  * Added TCP Segmentation Offload (TSO) support for the ENETC4 VF.
  * Added Receive Segment Coalesce (RSC / hardware LRO) support for ENETC4 PF and VF.
  * Extended the PF-to-VF link speed code field from 4-bit to 8-bit in ENETC4.
    Users running a PF kernel older than 6.18.37 must pass ``vf_link_legacy=1``
    as a device argument, otherwise link speed reporting will be incorrect.
  * Added firmware version reporting for the ENETC4 VF.
  * Added register dump support for ENETC4 PF and VF.
  * Added ring parameters support for the ENETC4 VF (``rxq_info_get`` / ``txq_info_get``).
  * Refreshed VF link speed on the link-up interrupt in the ENETC4 VF driver.
  * Added stats reset for the ENETC4 VF using a software snapshot/delta approach.
  * Added per-queue MSI-X Rx interrupt support for the ENETC4 VF.
  * Added SI-based port VLAN insertion (Tx) and removal (Rx) for ENETC4 PF and VF.
  * Updated ENETC4 VF link status reporting to use bitmask encoding.
  * Added Tx pause support for the ENETC4 VF via Rx congestion mode.
  * Added WRR Tx scheduler devarg (``enetc4_txq_wrr``) for ENETC4 VF ring weights.

* **Updated NXP DPAA drivers.**

  * Changed the default number of Rx queues in FMCLESS mode to the maximum
    supported per port instead of the number of lcores, because several
    queues may be processed on the same core.
  * Renamed the fmlib ``create_device()`` and ``get_device_id()`` helpers to
    ``fm_create_device()`` and ``fm_get_device_id()`` to avoid clashing with
    other drivers when DPDK is linked statically.
  * Added device arguments to the DPAA QDMA driver to control existing
    driver behaviour at runtime: ``dpaa_dma_sg_disable`` submits each
    descriptor as an individual single-entry transfer instead of the
    default scatter-gather batching, ``dpaa_dma_data_validation`` enables
    the post-transfer data validation helper for debugging, and, when the
    driver is built with ``RTE_DMA_DPAA_ERRATA_ERR050757``,
    ``dpaa_dma_pci_read_disable`` skips the ERR050757 source
    read-transaction programming.

* **Updated Solarflare network driver.**

  * Added VF support on AMD Solarflare X45xx adapters.

* **Updated ZTE zxdh ethernet driver.**

  * Added a fast single-segment Rx path (``zxdh_recv_single_pkts``)
    that is selected when the MTU fits in a single buffer.
  * Optimized the packed-ring Rx path.
  * Optimized the packed-ring Tx path
    with per-descriptor mbuf free (``rte_pktmbuf_free_seg``) and prefetch hints.
  * Changed the set of per-queue xstats counters.

* **Added RSA capability parameters in cryptodev.**

  Added ``rte_crypto_rsa_capa`` to report RSA modulus length,
  supported padding schemes, and MGF1 hash algorithms.
  Also added ``pss_explicit_salt`` so a PMD can advertise support
  for an application-supplied RSA-PSS salt,
  along with the corresponding ``pss_salt`` field in ``rte_crypto_rsa_op_param``
  to carry it per sign operation.

* **Added AMD AE4DMA DMA PMD.**

  Added a new dmadev driver for the AMD AE4DMA hardware DMA engine.
  Each PCI function exposes 16 hardware command queues; the PMD registers one
  dmadev per channel with a single virtual channel and supports
  memory-to-memory copy operations.

* **Added NXP i.MX95 eDMA5 DMA driver.**

  Added a DMA driver for the NXP i.MX95 eDMA5 controller.
  The driver exposes each eDMA5 instance as a DPDK dmadev device and
  supports memory-to-memory copy and scatter-gather copy operations.

* **Added per-queue statistics API in bbdev.**

  Added ``rte_bbdev_queue_stats_get()`` function to retrieve statistics
  for a specific queue, complementing the existing device-level statistics API.

* **Added AMD NTB support to the NTB rawdev driver.**

  Added support for the NTB endpoints integrated in AMD EPYC Embedded "Turin", "Genoa"
  and "Siena" processors to the ``raw/ntb`` driver.
  The NTB rawdev framework was generalized to support multiple vendors,
  with AMD-specific hardware access, a primary/secondary topology
  and a packed scratchpad handshake.

* **Supported auto-detection of uncore power driver.**

  The uncore power library got automatic probing of multiple uncore drivers
  when using ``RTE_UNCORE_PM_ENV_AUTO_DETECT``,
  instead of defaulting only to the Intel uncore driver.

* **Added graph profiling.**

  Added build time configurable graph node profiling statistics
  via ``RTE_GRAPH_PROFILE`` in ``rte_config.h`` (disabled by default).
  When enabled, tracks cycles spent processing various burst size ranges per node.
  The edges of the ranges are build time configurable via
  ``RTE_GRAPH_PROFILE_BURST_SMALL``, ``RTE_GRAPH_PROFILE_BURST_MEDIUM``, and
  ``RTE_GRAPH_PROFILE_BURST_LARGE`` in ``rte_config.h``.

* **Changed graph internal scheduling mechanism.**

  The graph walk functions now process nodes in topological order
  using bitmap scanning instead of a circular buffer.
  This ensures that in fan-out-then-converge topologies,
  all branches feeding a converge node complete before that node is visited.
  Reducing the occurrence of double-visits.

* **Added BPF validation application.**

  Added ``dpdk-validate-bpf`` tool to pre-validate eBPF programs
  for compatibility with the ``lib/bpf`` execution context.


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

* Removed the ``rte_atomicNN_t`` types and the ``rte_atomicNN_*`` functions
  (16, 32 and 64 bit variants of ``init``, ``read``, ``set``, ``add``,
  ``sub``, ``inc``, ``dec``, ``add_return``, ``sub_return``,
  ``inc_and_test``, ``dec_and_test``, ``test_and_set``, ``clear``,
  ``cmpset`` and ``exchange``). Use the C11 ``rte_atomic_*_explicit``
  operations instead.

* Removed SMP memory barriers:

  * ``rte_smp_mb()`` replaced with ``rte_atomic_thread_fence(rte_memory_order_seq_cst)``
  * ``rte_smp_rmb()`` replaced with ``rte_atomic_thread_fence(rte_memory_order_acquire)``
  * ``rte_smp_wmb()`` replaced with ``rte_atomic_thread_fence(rte_memory_order_release)``

  Some call sites became release stores or acquire loads instead of a
  standalone fence. Barriers that order CPU accesses against a device,
  rather than against another CPU, should use ``rte_io_wmb()`` and
  ``rte_io_rmb()`` instead.

* mempool: Removed the deprecated and obsolete ``flushthresh`` field
  from the ``rte_mempool_cache`` structure.

* net: Removed ``version``, ``ds``, ``ecn`` and ``flow_label``
  bitfields from ``struct rte_ipv6_hdr``.
  They accessed the wrong bits on little endian.
  Use ``vtc_flow`` with the ``RTE_IPV6_HDR_*_MASK``
  and ``RTE_IPV6_HDR_*_SHIFT`` macros.

* ethdev: Removed support for ethdev queue stats mapping.

  ``rte_eth_dev_set_tx_queue_stats_mapping`` and ``rte_eth_dev_set_rx_queue_stats_mapping``
  were deprecated and are now removed.

* ethdev: Removed the ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` build time limit.
  Per-queue xstats are now reported for all queues, not just the first 16.
  The ``rx_qN_errors`` xstat is removed, drops are still counted in ``ierrors``.

* pipeline: Removed the legacy library API:
  ``rte_pipeline_*``, ``rte_table_*``, ``rte_table_action_*``,
  ``rte_port_*`` and ``rte_port_in_action_*`` functions.
  The SWX API (``rte_swx_pipeline_*``, ``rte_swx_table_*``, ``rte_swx_port_*``) remains.
  Removed the ``dpdk-test-pipeline`` application,
  and the ``ip_pipeline`` example application,
  which were based on the legacy pipeline library API.
  The ``pipeline`` example application covers the SWX pipeline API.

* sched: Removed the private random number generator used by RED:
  the inline function ``rte_fast_rand()``.

* metrics: Removed the ``rte_metrics_telemetry.h`` header
  and the experimental telemetry helper API: ``rte_metrics_tel_reg_all_ethdev``,
  ``rte_metrics_tel_encode_json_format``, ``rte_metrics_tel_get_global_stats``,
  ``rte_metrics_tel_get_port_stats_ids``, ``rte_metrics_tel_get_ports_stats_json``
  and ``rte_metrics_tel_extract_data``.
  These had no users and exposed jansson types in the public header.
  The legacy telemetry commands ``ports_all_stat_values``,
  ``global_stat_values`` and ``ports_stats_values_by_name`` are unchanged.

* net/iavf: Removed the ``auto_reconfig`` devarg.

  The restoration of the promiscuous and all-multicast settings after a VF reset
  is now unconditional.

* net/iavf: Removed the dynamic mbuf field method for detecting LLDP packets
  on the transmit path, along with the ``set tx lldp on`` testpmd command.
  The only remaining method for detecting LLDP packets is by using
  the mbuf packet type in conjunction with the ``enable_lldp`` devarg.

* net/bnxt: Removed VMDq support.
  Applications configuring VMDq pools on bnxt now get zero pools reported,
  matching no VMDq hardware support.

* vhost: Removed async (DMA-accelerated) data path.

  The experimental async data path API has been removed from the vhost library.
  The following functions and flag are no longer available:

  - ``rte_vhost_async_channel_register``
  - ``rte_vhost_async_channel_register_thread_unsafe``
  - ``rte_vhost_async_channel_unregister``
  - ``rte_vhost_async_channel_unregister_thread_unsafe``
  - ``rte_vhost_async_dma_configure``
  - ``rte_vhost_async_dma_unconfigure``
  - ``rte_vhost_submit_enqueue_burst``
  - ``rte_vhost_poll_enqueue_completed``
  - ``rte_vhost_async_try_dequeue_burst``
  - ``rte_vhost_async_get_inflight``
  - ``rte_vhost_async_get_inflight_thread_unsafe``
  - ``rte_vhost_clear_queue``
  - ``rte_vhost_clear_queue_thread_unsafe``
  - ``RTE_VHOST_USER_ASYNC_COPY``


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

* eal: Improved pointer arithmetic macros.

  * ``RTE_PTR_ADD``, ``RTE_PTR_SUB``, ``RTE_PTR_ALIGN``, ``RTE_PTR_ALIGN_CEIL``,
    and ``RTE_PTR_ALIGN_FLOOR`` now preserve const/volatile qualifiers.
    These macros do not nest infinitely and may require intermediate variables.
  * Passing NULL to ``RTE_PTR_ADD``, ``RTE_PTR_SUB``, ``RTE_PTR_ALIGN``,
    ``RTE_PTR_ALIGN_CEIL``, or ``RTE_PTR_ALIGN_FLOOR`` clarified as undefined behavior.
  * ``RTE_PTR_ADD`` and ``RTE_PTR_SUB`` no longer accept integer types
    as the pointer argument; existing code should use native operators (e.g. + -).
  * ``RTE_PTR_ALIGN``, ``RTE_PTR_ALIGN_CEIL`` and ``RTE_PTR_ALIGN_FLOOR``
    still compile with an integer argument, but this is deprecated usage:
    existing code should use ``RTE_ALIGN``, ``RTE_ALIGN_CEIL`` or ``RTE_ALIGN_FLOOR`` instead.

* eal: Unaligned integer types are now really unaligned.

  ``unaligned_uint16_t``, ``unaligned_uint32_t`` and ``unaligned_uint64_t``
  are now declared with an alignment of 1 on all architectures.
  The compiler may generate narrower loads and stores than before.

* eal: The entire VFIO API (``rte_vfio_*``) was made internal.
  These functions are now available only to EAL and drivers,
  and are no longer part of the public API.

* interrupts: Updated interrupt handle file descriptor management.

  * ``rte_intr_instance_alloc()`` now initializes file descriptors
    to invalid values instead of zero.
  * Added ``rte_intr_fd_close()`` and ``rte_intr_dev_fd_close()``
    helpers to close file descriptors and reset them to -1.
  * ``rte_intr_unregister_callback_fn`` now receives a ``const`` interrupt handle.

* mempool: When creating a mempool, the cache size can be freely specified
  (although still not exceed the number of elements),
  and is no longer limited by the ``RTE_MEMPOOL_CACHE_MAX_SIZE``
  build time configuration parameter.
  Although ``RTE_MEMPOOL_CACHE_MAX_SIZE`` has lost its original meaning,
  it was kept for compatibility purposes,
  as it is often used for the ``cache_size`` parameter when creating mempools.

* mempool: Updated the ``rte_mempool`` structure as follows:

  * Added the ``sizeof_cache_per_lcore`` field,
    for indexing into the per-lcore local cache.

* mempool: Updated the ``rte_mempool_cache`` structure as follows:

  * Removed the deprecated and obsolete ``flushthresh`` field.
  * Removed the ``unused`` field.
  * Changed the ``objs`` array from fixed size to variable size.

* ethdev: Updated multi-queue mode validation at port configuration time:

  * DCB modes (Rx ``RTE_ETH_MQ_RX_DCB_FLAG``, Tx ``RTE_ETH_MQ_TX_DCB``/``RTE_ETH_MQ_TX_VMDQ_DCB``)
    are rejected if the driver has no ``get_dcb_info`` callback.
  * RSS mode (Rx ``RTE_ETH_MQ_RX_RSS_FLAG``) is rejected with more than
    one Rx queue if the driver reports no ``flow_type_rss_offloads``,
    and ignored with a single Rx queue.
  * VMDq modes (Rx ``RTE_ETH_MQ_RX_VMDQ_FLAG``,
    Tx ``RTE_ETH_MQ_TX_VMDQ_DCB``/``RTE_ETH_MQ_TX_VMDQ_ONLY``) are rejected
    if the driver reports no VMDq pool.

* ethdev: Updated VMDq related API.

  * A check was added in ``rte_eth_dev_mac_addr_add`` to validate that
    the ``pool`` parameter is 0 when VMDq is not configured.
  * The ``RTE_ETH_NUM_RECEIVE_MAC_ADDR`` and ``RTE_ETH_VMDQ_NUM_UC_HASH_ARRAY`` macros
    are VMDq related and are sizes of internal arrays in ethdev
    that only drivers need to care about.
    Those macros are moved to the driver only ethdev API.

* ethdev: Promoted the following API from experimental to stable:

  * device info:
    ``rte_eth_dev_capability_name``, ``rte_eth_dev_conf_get``,
    ``rte_eth_macaddrs_get``, ``rte_eth_dev_priv_dump``,
    ``rte_eth_rx_descriptor_dump``, ``rte_eth_tx_descriptor_dump``,
    ``rte_eth_dev_rss_algo_name``, ``rte_eth_find_rss_algo`` and
    ``rte_eth_dev_get_reg_info_ext``
  * fast path inline functions:
    ``rte_eth_recycle_mbufs`` and ``rte_eth_tx_queue_count``
  * link speed: ``rte_eth_link_speed_to_str`` and ``rte_eth_link_to_str``
  * speed lanes:
    ``rte_eth_speed_lanes_get``, ``rte_eth_speed_lanes_set`` and
    ``rte_eth_speed_lanes_get_capability``
  * SFF: ``rte_eth_dev_get_module_info`` and ``rte_eth_dev_get_module_eeprom``
  * hairpin queue: ``rte_eth_rx_hairpin_queue_setup``,
    ``rte_eth_tx_hairpin_queue_setup``,
    ``rte_eth_dev_hairpin_capability_get``,
    ``rte_eth_hairpin_bind``,
    ``rte_eth_hairpin_unbind``,
    ``rte_eth_hairpin_get_peer_ports``
  * queue helpers:
    ``rte_eth_rx_queue_is_valid``, ``rte_eth_tx_queue_is_valid``,
    ``rte_eth_rx_avail_thresh_set``, ``rte_eth_rx_avail_thresh_query``,
    ``rte_eth_recycle_rx_queue_info_get``,
    ``rte_eth_dev_count_aggr_ports`` and
    ``rte_eth_dev_map_aggr_tx_affinity``
  * clock: ``rte_eth_read_clock``
  * FEC: ``rte_eth_fec_get_capability``, ``rte_eth_fec_get`` and ``rte_eth_fec_set``
  * priority flow:
    ``rte_eth_dev_priority_flow_ctrl_queue_configure`` and
    ``rte_eth_dev_priority_flow_ctrl_queue_info_get``
  * meter (MTR) and policing: ``rte_mtr_*``
  * congestion management:
    ``rte_eth_cman_config_init``, ``rte_eth_cman_config_set``,
    ``rte_eth_cman_config_get`` and ``rte_eth_cman_info_get``
  * IP reassembly:
    ``rte_eth_ip_reassembly_capability_get``, ``rte_eth_ip_reassembly_conf_get`` and
    ``rte_eth_ip_reassembly_conf_set``
  * utility functions:
    ``rte_eth_get_monitor_addr``, ``rte_eth_representor_info_get``,
    ``rte_eth_buffer_split_get_supported_hdr_ptypes``,
    ``rte_eth_timesync_adjust_freq`` and ``rte_tm_node_query``
  * buffer split and error handling types:
    ``rte_eth_rxseg_split``, ``rte_eth_rxseg``, ``rte_eth_rxseg_capa``
    and ``rte_eth_err_handle_mode``
  * flow conversion and dump: ``rte_flow_conv`` and ``rte_flow_dev_dump``
  * flow tunnel: ``rte_flow_tunnel_decap_set``, ``rte_flow_tunnel_match``,
    ``rte_flow_tunnel_item_release``,
    ``rte_flow_tunnel_action_decap_release`` and
    ``rte_flow_get_restore_info``
  * flow age: ``rte_flow_get_aged_flows`` and ``rte_flow_get_q_aged_flows``
  * flow action: ``rte_flow_action_handle_*`` and ``rte_flow_action_list_handle_*``
  * flow flex: ``rte_flow_flex_item_create`` and ``rte_flow_flex_item_release``
  * flow template:
    ``rte_flow_configure``, ``rte_flow_info_get``,
    ``rte_flow_pattern_*``,
    ``rte_flow_actions_*``,
    ``rte_flow_template_*``,
    ``rte_flow_async_*``,
    ``rte_flow_push``, ``rte_flow_pull``
  * remaining flow helpers: ``rte_flow_actions_update``, ``rte_flow_restore_info_dynflag``,
    ``rte_flow_calc_table_hash``, ``rte_flow_group_set_miss_actions``,
    and ``rte_flow_calc_encap_hash``
  * all remaining ``rte_flow`` item, action and async operation structures

* eventdev: Promoted the following API from experimental to stable:

  * Rx adapter: ``rte_event_eth_rx_adapter_create_ext_with_params``,
    ``rte_event_eth_rx_adapter_runtime_params_init``,
    ``rte_event_eth_rx_adapter_runtime_params_set`` and
    ``rte_event_eth_rx_adapter_runtime_params_get``
  * Tx adapter: ``rte_event_eth_tx_adapter_runtime_params_init``,
    ``rte_event_eth_tx_adapter_runtime_params_set`` and
    ``rte_event_eth_tx_adapter_runtime_params_get``
  * crypto adapter: ``rte_event_crypto_adapter_runtime_params_init``,
    ``rte_event_crypto_adapter_runtime_params_set`` and
    ``rte_event_crypto_adapter_runtime_params_get``
  * timer adapter: ``rte_event_timer_remaining_ticks_get``
  * DMA adapter: ``rte_event_dma_adapter_*``
  * link profiles: ``rte_event_port_profile_links_set``,
    ``rte_event_port_profile_links_get`` and
    ``rte_event_port_profile_unlink``

* raw/ntb: Removed the ``mw_size_align`` field from ``struct ntb_dev_info``.
  Applications must now query the required memory window base-address alignment
  with the new ``rte_pmd_ntb_get_mem_align()`` function instead of relying on this field.

* reorder: Promoted the following API from experimental to stable:
  ``rte_reorder_seqn``, ``rte_reorder_drain_up_to_seqn``,
  ``rte_reorder_min_seqn_set`` and ``rte_reorder_memory_footprint_get``.

* vhost: Introduced ``RTE_VHOST_USER_MAP_POPULATE``
  as a replacement of ``RTE_VHOST_USER_ASYNC_COPY``
  for users that did not rely on the vhost async copy feature
  but expected guest pages to be prefaulted.

* vhost: Removed ``get_vfio_group_fd`` from ``struct rte_vdpa_dev_ops``.
  vDPA drivers should now use the VFIO container device assignment API.

* graph: The internal scheduling mechanism of ``struct rte_graph`` has changed.
  The circular buffer fields (``head``, ``tail``, ``cir_mask``, ``cir_start``)
  have been replaced with a topological bitmap scheduler.
  ``struct rte_node`` has a new internal field to hold the scheduling order.

* bpf: Made the ``bpf_def.h`` header file private,
  so eBPF instruction opcodes are no longer publicly available
  (their numerical values follow the RFC and e.g. Linux headers).
  The definition of the instruction struct and number of program arguments
  were moved to ``rte_bpf.h`` and gained the public ``RTE_`` prefix:
  ``struct rte_ebpf_insn`` and ``RTE_BPF_FUNC_MAX_ARGS``.

* telemetry: Promoted the following API from experimental to stable:

  * ``rte_tel_data_add_array_uint_hex``
  * ``rte_tel_data_add_dict_uint_hex``
  * ``rte_telemetry_register_cmd_arg``


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

* cryptodev: The struct ``rte_cryptodev_asymmetric_xform_capability``
  was updated to include ``rsa_capa``, a new ``rte_crypto_rsa_capa`` field.

* cryptodev: The struct ``rte_crypto_rsa_op_param``
  was updated to include ``pss_salt``.


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
