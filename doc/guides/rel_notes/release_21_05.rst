.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html
      xdg-open build/doc/html/guides/rel_notes/release_21_05.html


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
     =======================================================

* **Added Alpine Linux with musl libc support**

  The distribution Alpine Linux, using musl libc and busybox,
  got initial support starting with building DPDK without modification.

* **Added phase-fair lock.**

  Phase-fair lock provides fairness guarantees.
  It has two ticket pools, one for readers and one for writers.

* **Added support for Marvell CN10K SoC drivers.**

  Added Marvell CN10K SoC support. Marvell CN10K SoC are based on Octeon 10
  family of ARM64 processors with ARM Neoverse N2 core with accelerators for
  packet processing, timers, cryptography, etc.

  * Added common/cnxk driver consisting of common API to be used by
    net, crypto and event PMD's.
  * Added mempool/cnxk driver which provides the support for the integrated
    mempool device.

* **Enhanced ethdev representor syntax.**

  * Introduced representor type of VF, SF and PF.
  * Supported sub-function and multi-host in representor syntax::

      representor=#            [0,2-4]      /* Legacy VF compatible.         */
      representor=[[c#]pf#]vf# c1pf2vf3     /* VF 3 on PF 2 of controller 1. */
      representor=[[c#]pf#]sf# sf[0,2-1023] /* 1023 SFs.                     */
      representor=[c#]pf#      c2pf[0,1]    /* 2 PFs on controller 2.        */

* **Added queue state in queried Rx/Tx queue info.**

  * Added new field ``queue_state`` to ``rte_eth_rxq_info`` structure to
    provide indicated Rx queue state.
  * Added new field ``queue_state`` to ``rte_eth_txq_info`` structure to
    provide indicated Tx queue state.

* **Updated meter API.**

  * Added packet mode in the meter profile parameters data structures
    to support metering traffic by packet per second (PPS),
    in addition to the initial bytes per second (BPS) mode (value 0).
  * Added support of pre-defined meter policy via flow action list per color.

* **Added packet integrity match to flow rules.**

  * Added ``RTE_FLOW_ITEM_TYPE_INTEGRITY`` flow item.
  * Added ``rte_flow_item_integrity`` data structure.

* **Added TCP connection tracking offload in flow API.**

  * Added conntrack item and action for stateful connection offload.

* **Updated Arkville PMD driver.**

  Updated Arkville net driver with new features and improvements, including:

  * Generalized passing meta data between PMD and FPGA, allowing up to 20
    bytes of user specified information in RX and TX paths.

  * Updated dynamic PMD extensions API using standardized names.

  * Added support for new Atomic Rules PCI device IDs ``0x100f, 0x1010, 0x1017,
    0x1018, 0x1019``.

* **Updated Broadcom bnxt driver.**

  * Updated HWRM structures to 1.10.2.15 version.

* **Updated Hisilicon hns3 driver.**

  * Added support for module EEPROM dumping.
  * Added support for freeing Tx mbuf on demand.
  * Added support for copper port in Kunpeng930.
  * Added support for runtime config to select IO burst function.
  * Added support for outer UDP checksum in Kunpeng930.
  * Added support for query Tx descriptor status.
  * Added support for query Rx descriptor status.
  * Added support for IEEE 1588 PTP.

* **Updated Intel iavf driver.**

  Updated the Intel iavf driver with new features and improvements, including:

  * Added flow filter to support GTPU inner L3/L4 fields matching.
  * In AVX512 code, added the new RX and TX paths to use the HW offload
    features. When the HW offload features are configured to be used, the
    offload paths are chosen automatically. In parallel the support of HW
    offload features was removed from the legacy AVX512 paths.

* **Updated Intel ice driver.**

  * Added Intel ice support on Windows.
  * Added GTPU TEID support for DCF switch filter.
  * Added flow priority support for DCF switch filter.

* **Updated Marvell OCTEON TX2 ethdev driver.**

  * Added support for flow action port id.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Added support for VXLAN and NVGRE encap as sample actions.
  * Support push VLAN on ingress traffic and pop VLAN on egress traffic in E-Switch mode.
  * Added support for ASO (Advanced Steering Operation) meter.

* **Updated NXP DPAA driver.**

  * Added support for shared ethernet interface.
  * Added support for external buffers in Tx.

* **Updated NXP DPAA2 driver.**

  * Added support for traffic management.
  * Added support for configurable Tx confirmation.
  * Added support for external buffers in Tx.

* **Updated Wangxun txgbe driver.**

  * Added support for txgbevf PMD.
  * Support device arguments to handle AN training for backplane NICs.

* **Enabled vmxnet3 PMD on Windows.**

* **Enabled libpcap-based PMD on Windows.**

   A libpcap distribution, such as Npcap or WinPcap, is required to run the PMD.

* **Updated the AF_XDP driver.**

  * Added support for preferred busy polling.

* **Added support of multiple data-units in cryptodev API.**

  The cryptodev library has been enhanced to allow operations on multiple
  data-units for AES-XTS algorithm, the data-unit length should be set in the
  transformation. A capability for it was added too.

* **Added a cryptodev feature flag to support cipher wrapped keys.**

  A new feature flag has been added to allow application to provide
  cipher wrapped keys in session xforms.

* **Updated the OCTEON TX crypto PMD.**

  * Added support for DIGEST_ENCRYPTED mode in OCTEON TX crypto PMD.

* **Updated the OCTEON TX2 crypto PMD.**

  * Added support for DIGEST_ENCRYPTED mode in OCTEON TX2 crypto PMD.
  * Added support in lookaside protocol offload mode for IPsec with
    UDP encapsulation support for NAT Traversal.
  * Added support in lookaside protocol offload mode for IPsec with
    IPv4 transport mode.

* **Updated Mellanox RegEx PMD.**

  * Added support for multi-segments mbuf.

* **Introduced period timer mode in eventdev timer adapter.**

  * Added support for periodic timer mode in eventdev timer adapter.
  * Added support for periodic timer mode in octeontx2 event device driver.

* **Added event device vector capability.**

  * Added ``rte_event_vector`` data structure which is capable of holding
    multiple ``uintptr_t`` of the same flow thereby allowing applications
    to vectorize their pipelines and also reduce the complexity of pipelining
    the events across multiple stages.
  * This also reduced the scheduling overhead on a event device.

* **Added Predictable RSS functionality to the Toeplitz hash library.**

  Added feature for finding collisions of the Toeplitz hash function -
  the hash function used in NICs to spread the traffic among the queues.
  It can be used to get predictable mapping of the flows.

* **Updated testpmd.**

  * Added a command line option to configure forced speed for Ethernet port.
    ``dpdk-testpmd -- --eth-link-speed N``
  * Added command to show link flow control info.
    ``show port (port_id) flow_ctrl``
  * Added command to display Rx queue used descriptor count.
    ``show port (port_id) rxq (queue_id) desc used count``
  * Added command to cleanup a Tx queue's mbuf on a port.
    ``port cleanup (port_id) txq (queue_id) (free_cnt)``
  * Added command to dump internal representation information of single flow.
    ``flow dump (port_id) rule (rule_id)``
  * Added commands to create and delete meter policy.
    ``add port meter policy (port_id) (policy_id) ...``
  * Added commands to construct conntrack context and relevant indirect
    action handle creation, update for conntrack action as well as conntrack
    item matching.

* **Added support for the FIB lookup method in the l3fwd example app.**

  Previously the l3fwd sample app only supported LPM and EM lookup methods,
  the app now supports the Forwarding Information Base (FIB) lookup method.

* **Updated ipsec-secgw sample application.**

  * Updated the ``ipsec-secgw`` sample application with UDP encapsulation
    support for NAT Traversal.

* **Enhanced crypto adapter forward mode.**

  * Added ``rte_event_crypto_adapter_enqueue()`` API to enqueue events to crypto
    adapter if forward mode is supported by driver.
  * Added support for crypto adapter forward mode in octeontx2 event and crypto
    device driver.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* Removed support for Intel DLB V1 hardware. This is not a broad market device,
  and existing customers already obtain the source code directly from Intel.


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

* eal: The experimental TLS API added in ``rte_thread.h`` has been renamed
  from ``rte_thread_tls_*`` to ``rte_thread_*`` to avoid naming redundancy
  and confusion with the transport layer security term.

* pci: The value ``PCI_ANY_ID`` is marked as deprecated
  and can be replaced with ``RTE_PCI_ANY_ID``.

* ethdev: Added a ``rte_flow`` pointer parameter to the function
  ``rte_flow_dev_dump()`` allowing dump for single flow.

* cryptodev: The experimental raw data path API for dequeue
  ``rte_cryptodev_raw_dequeue_burst`` got a new parameter
  ``max_nb_to_dequeue`` to provide flexible control on dequeue.

* ethdev: The experimental flow API for shared action has been generalized
  as a flow action handle used in rules through an indirect action.
  The functions ``rte_flow_shared_action_*`` manipulating the action object
  are replaced with ``rte_flow_action_handle_*``.
  The action ``RTE_FLOW_ACTION_TYPE_SHARED`` is deprecated and can be
  replaced with ``RTE_FLOW_ACTION_TYPE_INDIRECT``.

* ethdev: The experimental function ``rte_mtr_policer_actions_update()``,
  the enum ``rte_mtr_policer_action``, and the struct members
  ``policer_action_recolor_supported`` and ``policer_action_drop_supported``
  have been removed.


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

* No ABI change that would break compatibility with 20.11.

* The experimental function ``rte_telemetry_legacy_register`` has been
  removed from the public API and is now an internal-only function. This
  function was already marked as internal in the API documentation for it,
  and was not for use by external applications.


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
