.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 23.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_23_11.html


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

* **Build requirements increased for C11.**

  From DPDK 23.11 onwards,
  building DPDK will require a C compiler which supports the C11 standard,
  including support for C11 standard atomics.

  More specifically, the requirements will be:

  * Support for flag "-std=c11" (or similar)
  * __STDC_NO_ATOMICS__ is *not defined* when using c11 flag

  Please note:

  * C11, including standard atomics, is supported from GCC version 5 onwards,
    and is the default language version in that release
    (Ref: https://gcc.gnu.org/gcc-5/changes.html)
  * C11 is the default compilation mode in Clang from version 3.6,
    which also added support for standard atomics
    (Ref: https://releases.llvm.org/3.6.0/tools/clang/docs/ReleaseNotes.html)

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

* **Added support for allow/block list in vmbus bus driver.***

  The ``vmbus`` bus driver now supports -a and -b EAL options for selecting
  devices.

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
  Such P4 pipeline, not using the standard blocks of the flow API,
  can be managed with ``RTE_FLOW_ITEM_TYPE_FLEX`` and ``RTE_FLOW_ACTION_TYPE_PROG``.

* **Added flow group set miss actions.**

  Introduced ``rte_flow_group_set_miss_actions()`` API to explicitly set
  a group's miss actions, which are the actions to be performed on packets
  that didn't match any of the flow rules in the group.

* **Updated Amazon ena (Elastic Network Adapter) net driver.**

  * Upgraded ENA HAL to latest version.
  * Added support for connection tracking allowance utilization metric.
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
  can happen based on inner packet.

  The API when implemented by an ethdev, application would be able to process
  packets that are received without/failed inline offload processing
  (such as fragmented ESP packets with inline IPsec offload).
  The API when implemented by a cryptodev, can be used for injecting packets
  to ethdev Rx after IPsec processing and take advantage of ethdev Rx actions
  for the inner packet which cannot be accelerated in inline protocol offload mode.

* **Updated cryptodev scheduler driver.**

  * Added support for DOCSIS security protocol
    through the ``rte_security`` API callbacks.

* **Updated ipsec_mb crypto driver.**

  * Added Intel IPsec MB v1.5 library support for x86 platform.
  * Added support for digest encrypted to AESNI_MB asynchronous crypto driver.

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

* **Added event DMA adapter library.**

  * Added the Event DMA Adapter Library. This library extends the event-based
    model by introducing APIs that allow applications to enqueue/dequeue DMA
    operations to/from dmadev as events scheduled by an event device.

* **Added eventdev support to link queues to port with link profile.**

  Introduced event link profiles that can be used to associated links between
  event queues and an event port with a unique identifier termed as link profile.
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

  Added CLI based graph application which exercises on different use cases.
  Application provides a framework so that each use case can be added via a file.
  Each CLI will further be translated into a graph representing the use case.

* **Added layer 2 MACsec forwarding example application.**

  Added a new example layer 2 forwarding application to benchmark
  MACsec encryption/decryption using rte_security based inline sessions.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

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

.. This section should contain API changes. Sample format:

   * sample: Add a short 1-2 sentence description of the API change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

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
