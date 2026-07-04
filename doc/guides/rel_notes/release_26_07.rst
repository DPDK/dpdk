.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2026 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 26.07
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_26_07.html


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

* **Added option to change memory limits per page size.**

  Added the EAL option ``--pagesz-mem``
  to override the default per-page-size memory limits.
  Each maximum can be configured with a pair ``<pagesz>:<limit>``.

* **Added option to disable auto probing.**

  Added EAL options affecting the initial bus probing.

  * ``-A`` or ``--no-auto-probing`` disable the initial bus probing: no device is probed during
    ``rte_eal_init`` and the application is responsible for probing each device,
  * ``--auto-probing`` enables the initial bus probing, which is the current default behavior.

* **Changed mempool cache behaviour.**

  * The mempool cache flush/refill algorithm was improved,
    to reduce the mempool cache miss rate for most application types.
    Applications where each lcore only puts or gets to a mempool,
    e.g. pipelined applications where ethdev Rx and Tx run on separate lcores,
    should adapt to the new algorithm by doubling their configured mempool cache size,
    to avoid doubling their mempool cache miss rate.
  * The effective size of a mempool cache was changed
    to match the specified size at mempool creation;
    the effective size was previously 50 % larger than requested.
  * The size of the ``struct rte_mempool_cache`` was kept
    for API/ABI compatibility purposes.

* **Added RISC-V vector paths.**

  * Increased the default SIMD bitwidth to allow using the vector extension.
  * Added vectorized ACL which can process up to 8 flows in parallel.
  * Added vectorized IPv4 LPM lookup for the node library.

* **Added peek style API for staged-ordered ring (``rte_soring``).**

  For sorings with producer/consumer in ``RTE_RING_SYNC_ST``,
  ``RTE_RING_SYNC_MT_HTS`` mode, provide the ability to split enqueue/dequeue
  operation into two phases (enqueue/dequeue start and enqueue/dequeue finish).
  This allows the user to inspect objects in the ring without removing them
  (aka MT safe peek).

* **Added PTP protocol definitions.**

  Added IEEE 1588 Precision Time Protocol header structures, constants,
  and inline helpers to ``lib/net/rte_ptp.h``.
  Provides wire-format structures with endian-annotated types
  and correction field manipulation for transparent clock implementations.

* **Added PTP software relay example application.**

  Added a new example application ``ptp_tap_relay_sw``
  demonstrating a software PTP transparent clock relay
  between a DPDK port and a kernel TAP interface.

* **Added no-IOMMU mode to UACCE bus.**

  Added no-IOMMU mode for devices without or not enabling IOMMU/SVA.

* **Added unplug operation support to VMBUS bus.**

  Implemented device unplug operation to allow runtime removal of VMBUS devices.

* **Added selective Rx in ethdev API.**

  Some parts of packets may be discarded in Rx
  by configuring a split of packets received in a queue,
  and assigning no mempool to some configuration segments.
  This is a driver capability advertised in the ``selective_rx`` bit.

* **Added vhost support for dynamic memory regions.**

  The feature ``VHOST_USER_PROTOCOL_F_CONFIGURE_MEM_SLOTS`` has been implemented
  to support adding and removing memory regions without resetting
  the whole guest memory map.

* **Added LinkData sxe2 ethernet driver.**

  Added network driver for the LinkData network adapters.

* **Updated Google gve driver.**

  * Added hardware timestamping support on DQO queues.

* **Updated Intel iavf driver.**

  * Added support for QinQ offloading operations.
  * Added support for transmitting LLDP packets based on mbuf packet type.
  * Implemented AVX2 context descriptor transmit paths.

* **Updated Intel ice driver.**

  * Added ``rl_burst_size`` devarg to configure the scheduler rate-limiter
    burst size, reducing Tx latency jitter for time-sensitive traffic.

* **Updated Microsoft mana driver.**

  * Added device reset support to the MANA PMD,
    doing automatic recovery from hardware service reset events,
    and notification to upper layers of the reset lifecycle.

* **Updated NVIDIA mlx5 ethernet driver.**

  * Added support for selective Rx in scalar SPRQ Rx path.

* **Updated NXP dpaa2 driver.**

  * Added inner RSS level support for tunnelled traffic.
  * Added RSS RETA query and update support.
  * Removed the software VLAN strip offload:
    ``RTE_ETH_RX_OFFLOAD_VLAN_STRIP`` is no longer advertised,
    as no hardware strip backs it.
    An application that needs the tag removed must now strip it itself.

* **Updated NXP enetc ethernet driver.**

  * Added support for ESP packet type in packet parsing.
  * Added scatter-gather support for ENETC4 PFs and VFs.
  * Added devargs option ``enetc4_vsi_disable`` to disable VSI-PSI messaging.
  * Added devargs options ``enetc4_vsi_timeout`` and ``enetc4_vsi_delay``
    for VSI-PSI messaging timeout and delay.
  * Added devargs option ``enetc4_txq_prior`` to set Tx queues priorities.
  * Added devargs option ``nc`` to select non-cacheable Rx/Tx ops.

* **Updated PCAP ethernet driver.**

  * Added support for VLAN insertion and stripping.
  * Added support for reporting link state in ``iface`` mode.
  * Added support for link state interrupt in ``iface`` mode.
  * Added nanosecond precision to timestamp support.
  * Added ``snaplen`` devarg to configure packet capture snapshot length.
  * Added ``eof`` devarg to use link state to signal end of receive file input.
  * Added unit test suite.

* **Updated Marvell cnxk crypto driver.**

  * Added support for ML-KEM and ML-DSA on CN20K platform.

* **Updated Wangxun ngbe driver.**

  * Implemented UDP Segmentation Offload (USO) in the transmit path.
    The ``RTE_ETH_TX_OFFLOAD_UDP_TSO`` capability was advertised
    since the driver's initial integration but the data path was missing;
    it is now functional.

* **Updated Wangxun txgbe driver.**

  * Implemented UDP Segmentation Offload (USO) in the transmit path.
    The ``RTE_ETH_TX_OFFLOAD_UDP_TSO`` capability was advertised
    since the driver's initial integration but the data path was missing;
    it is now functional.
  * Added support for VF sensing PF down.
    VFs now detect when the PF goes down
    (via the mailbox ``TXGBE_VT_MSGTYPE_CTS`` flag) and report link down.
    When the PF comes back, the VF triggers an ``RTE_ETH_EVENT_INTR_RESET`` event
    so the application can reset the VF and resume normal packet Rx/Tx.
  * Added VF support for the Amber-Lite 40G NIC variant,
    with new device IDs ``0x503f`` and ``0x513f``.

* **Updated Intel qat crypto driver dependency requirements.**

  The QAT crypto PMD now requires IPsec MB library (v1.4.0+)
  for HMAC precomputes on all platforms.
  OpenSSL 3.0+ is now optional and used only for DOCSIS BPI cipher fallback.
  Previously, QAT could build with OpenSSL-only on x86.

  On ARM, both IPsec MB and OpenSSL are required for full functionality.

* **Extended BPF API.**

  * Added an extensible BPF loading API comprising the function
    ``rte_bpf_load_ex`` and struct ``rte_bpf_prm_ex``.
    This enables new features such as loading classic BPF (cBPF),
    loading ELF images directly from memory buffers,
    and executing multi-argument programs, while avoiding future ABI breakages.
  * Added support for loading and executing BPF programs with up to 5 arguments.
    This introduces new API functions ``rte_bpf_exec_ex``,
    ``rte_bpf_exec_burst_ex``, and ``rte_bpf_get_jit_ex``.
  * Added API functions ``rte_bpf_eth_rx_install`` and ``rte_bpf_eth_tx_install``
    for installing already loaded BPF programs as port callbacks
    (as opposed to loading them directly from ELF files).

* **Added BPF validation debugging API and hardened BPF validator.**

  * Introduced a new API (prefixed with ``rte_bpf_validate_debug_``)
    to introspect the BPF validator.
    This provides a mechanism to set breakpoints or catchpoints during validation
    and inspect the verifier's internal state (such as tracked register bounds).
    This API is crucial primarily for writing comprehensive tests for the validator,
    but also serves as a foundation for a future interactive eBPF validation debugger.
  * Fixed numerous bugs in the BPF validator's abstract interpretation logic,
    including incorrect bounds tracking for jumps and arithmetic operations,
    as well as fixing several instances of undefined behavior (UB)
    when verifying malicious or corrupt programs.

* **Updated testpmd application.**

  Added support for setting VLAN priority and CFI/DEI bits
  in ``tx_vlan set``/``tx_qinq set`` commands and ``vlan_tci`` parameter.

* **Added AI review helpers.**

  Added AGENTS.md file for AI review
  and supporting scripts to review patches and documentation.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* crypto/openssl: Removed support for OpenSSL 1.x versions from the OpenSSL crypto PMD.

  The OpenSSL crypto PMD now requires OpenSSL 3.0 as the minimum version,
  and all compatibility code for OpenSSL 1.0.1, 1.1.0, and 1.1.1 versions has been removed.

* crypto/ipsec_mb: Removed Chacha20-poly1305 and KASUMI crypto drivers.

  The Chacha20-poly1305 and KASUMI drivers were just wrappers
  around the main AESNI_MB driver.

* crypto/ipsec_mb: Removed ZUC and SNOW 3G crypto drivers from the x86 release.

  The ZUC and SNOW 3G crypto drivers are using APIs
  that are now deprecated in the Intel IPsec Multi-Buffer library.


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

* **ethdev: promoted flow metadata API from experimental to stable.**

  The following ethdev symbols are no longer marked experimental:

  - ``rte_flow_dynf_metadata_register``
  - ``rte_flow_dynf_metadata_offs``
  - ``rte_flow_dynf_metadata_mask``
  - ``rte_flow_dynf_metadata_avail``
  - ``rte_flow_dynf_metadata_get``
  - ``rte_flow_dynf_metadata_set``

* **mlx5: promoted driver event and steering management APIs from experimental to stable.**

  The following mlx5 functions are no longer marked experimental:

  - ``rte_pmd_mlx5_driver_event_cb_register``
  - ``rte_pmd_mlx5_driver_event_cb_unregister``
  - ``rte_pmd_mlx5_enable_steering``
  - ``rte_pmd_mlx5_disable_steering``

* **ip_frag: changed handling of malformed fragments.**

  - Duplicate fragments are tolerated instead of failing reassembly.
  - Overlapping fragments are rejected on arrival rather than during reassembly.
  - Oversized fragments (reassembled length over 65535) are rejected.
  - For IPv6, fragments with per-fragment extension headers are rejected.

  Overlap and oversize are now detected on arrival, which adds a scan of the
  already received fragments per fragment and may affect throughput.


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

* No ABI change that would break compatibility with 25.11.


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
