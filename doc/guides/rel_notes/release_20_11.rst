.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html
      xdg-open build/doc/html/guides/rel_notes/release_20_11.html


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

* **Added write combining store APIs.**

  Added ``rte_write32_wc`` and ``rte_write32_wc_relaxed`` APIs
  that enable write combining stores (depending on architecture).
  The functions are provided as a generic stub and
  x86 specific implementation.

* **Added prefetch with intention to write APIs.**

  Added new prefetch function variants e.g. ``rte_prefetch0_write``,
  which allow the programmer to prefetch a cache line and also indicate
  the intention to write.

* **Added the rte_cldemote API.**

  Added a hardware hint CLDEMOTE, which is similar to prefetch in reverse.
  CLDEMOTE moves the cache line to the more remote cache, where it expects
  sharing to be efficient. Moving the cache line to a level more distant from
  the processor helps to accelerate core-to-core communication.
  This API is specific to x86 and implemented as a stub for other
  architectures.

* **Added support for limiting maximum SIMD bitwidth.**

  Added a new EAL config setting ``max_simd_bitwidth`` to limit the vector
  path selection at runtime. This value can be set by apps using the
  ``rte_vect_set_max_simd_bitwidth`` function, or by the user with EAL flag
  ``--force-max-simd-bitwidth``.

* **Added zero copy APIs for rte_ring.**

  For rings with producer/consumer in ``RTE_RING_SYNC_ST``, ``RTE_RING_SYNC_MT_HTS``
  modes, these APIs split enqueue/dequeue operation into three phases
  (enqueue/dequeue start, copy data to/from ring, enqueue/dequeue finish).
  Along with the advantages of the peek APIs, these provide the ability to
  copy the data to the ring memory directly without the need for temporary
  storage.

* **Updated CRC modules of the net library.**

  * Added runtime selection of the optimal architecture-specific CRC path.
  * Added optimized implementations of CRC32-Ethernet and CRC16-CCITT
    using the AVX512 and VPCLMULQDQ instruction sets.

* **Introduced extended buffer description for receiving.**

  Added the extended Rx buffer description for Rx queue setup routine
  providing the individual settings for each Rx segment with maximal size,
  buffer offset and memory pool to allocate data buffers from.

* **Added the FEC API, for a generic FEC query and config.**

  Added the FEC API which provides functions for query FEC capabilities and
  current FEC mode from device. An API for configuring FEC mode is also provided.

* **Added thread safety to rte_flow functions.**

  Added the ``RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE`` device flag to indicate
  whether a PMD supports thread safe operations. If the PMD doesn't set the flag,
  the rte_flow API level functions will protect the flow operations with a mutex.

* **Added flow-based traffic sampling support.**

  Added a new action ``RTE_FLOW_ACTION_TYPE_SAMPLE`` that will sample the
  incoming traffic and send a duplicated traffic with the specified ratio to
  the application, while the original packet will continue to the target
  destination.

  The packets sampling is '1/ratio'. A ratio value set to 1 means that the
  packets will be completely mirrored. The sample packet can be assigned with
  a different set of actions than the original packet.

* **Added support of shared action in flow API.**

  Added shared action support to use single flow actions in multiple flow
  rules. An update to the shared action configuration alters the behavior of all
  flow rules using it.

  * Added a new action: ``RTE_FLOW_ACTION_TYPE_SHARED`` to use shared action
    as a flow action.
  * Added new flow APIs to create/update/destroy/query shared actions.

* **Added support to flow rules to allow private PMD items/actions.**

  * Flow rule verification has been  updated to accept private PMD
    items and actions.

* **Added a generic API to offload tunneled traffic and restore missed packets.**

  * Added a new hardware independent helper to the flow API that
    offloads tunneled traffic and restores missed packets.

* **Updated the ethdev library to support hairpin between two ports.**

  New APIs have been introduced to support binding / unbinding of 2 ports in a
  hairpin configuration. The hairpin Tx part flow rules can be inserted
  explicitly. A new API has been added to get the hairpin peer ports list.

* **Updated the Amazon ena driver.**

  Updated the ena PMD with new features and improvements, including:

  * Added network interface metrics which can be read using xstats.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with new features and improvements, including:

  * Added support for 200G PAM4 link speed.
  * Added support for RSS hash level selection.
  * Updated HWRM structures to 1.10.1.70 version.
  * Added TRUFLOW support for Stingray devices.
  * Added support for representors on MAIA cores of SR.
  * Added support for VXLAN decap offload using rte_flow.
  * Added support to indicate native rte_flow API thread safety.

* **Updated Cisco enic driver.**

  * Added support for VF representors with single-queue Tx/Rx and flow API
  * Added support for egress PORT_ID action
  * Added support for non-zero priorities for group 0 flows
  * Added support for VXLAN decap combined with VLAN pop

* **Added hns3 FEC PMD, for supporting query and config FEC mode.**

  Added the FEC PMD which provides functions for querying FEC capabilities and
  current FEC mode from a device. A PMD for configuring FEC mode is also provided.

* **Updated the Intel iavf driver.**

  Updated the iavf PMD with new features and improvements, including:

  * Added support for flexible descriptor metadata extraction.
  * Added support for outer IP hash of GTPC and GTPU.
  * Added support of AVX512 instructions in Rx and Tx path.
  * Added support for up to 256 queue pairs per VF.

* **Updated Intel ixgbe driver.**

  Updated the Intel ixgbe driver to use write combining stores.

* **Updated Intel i40e driver.**

  Updated the Intel i40e driver to use write combining stores.

* **Updated Intel ice driver.**

  * Added support for write combining stores.
  * Added ACL filter support for the Intel DCF.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Added vectorized Multi-Packet Rx Queue burst.
  * Added support for 2 new miniCQE formats: Flow Tag and L3/L4 header.
  * Added support for PMD level multiple-thread flow insertion.
  * Added support for matching on fragmented/non-fragmented IPv4/IPv6 packets.
  * Added support for QinQ packets matching.
  * Added support for the new VLAN fields ``has_vlan`` in the Ethernet item
    and ``has_more_vlan`` in the VLAN item.
  * Updated the supported timeout for Age action to the maximal value supported
    by the rte_flow API.
  * Added support for Age action query.
  * Added support for multi-ports hairpin.
  * Allow unknown link speed.

  Updated Mellanox mlx5 vDPA driver:

  * Added support of vDPA VirtQ error handling.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added SR-IOV PF support
  * Added Alveo SN1000 SmartNICs (EF100 architecture) support including
    flow API transfer rules for switch HW offload
  * Added ARMv8 support
  * Claimed flow API native thread safety

* **Added Wangxun txgbe PMD.**

  Added a new PMD driver for Wangxun 10 Gigabit Ethernet NICs.

  See the :doc:`../nics/txgbe` for more details.

* **Updated Virtio driver.**

  * Added support for Vhost-vDPA backend to the Virtio-user PMD.
  * Changed default link speed to unknown.
  * Added support for the 200G link speed.

* **Updated Memif PMD.**

  * Added support for abstract socket addresses.
  * Changed default socket address type to abstract.

* **Added UDP/IPv4 GRO support for VxLAN and non-VxLAN packets.**

  For VxLAN packets, added inner UDP/IPv4 support.
  For non-VxLAN packets, added UDP/IPv4 support.

* **Extended the flow-perf application.**

  * Added support for user order instead of bit mask.
    Now the user can create any structure of rte_flow
    using the flow performance application with any order.
    Moreover the app also now starts to support inner
    items matching as well.
  * Added header modify actions.
  * Added flag action.
  * Added raw encap/decap actions.
  * Added VXLAN encap/decap actions.
  * Added ICMP (code/type/identifier/sequence number) and ICMP6 (code/type) matching items.
  * Added option to set port mask for insertion/deletion:
    ``--portmask=N``
    where N represents the hexadecimal bitmask of the ports used.

* **Added raw data-path APIs for cryptodev library.**

  Added raw data-path APIs to Cryptodev to help accelerate external libraries
  or applications which need to avail of fast cryptodev enqueue/dequeue
  operations but which do not necessarily need to depend on mbufs and
  cryptodev operation mempools.

* **Updated the aesni_mb crypto PMD.**

  * Added support for intel-ipsec-mb version 0.55.
  * Added support for AES-ECB 128, 192 and 256.
  * Added support for ZUC-EEA3/EIA3 algorithms.
  * Added support for SNOW3G-UEA2/UIA2 algorithms.
  * Added support for KASUMI-F8/F9 algorithms.
  * Added support for Chacha20-Poly1305.
  * Added support for AES-256 CCM algorithm.

* **Updated the aesni_gcm crypto PMD.**

  * Added SGL support for AES-GMAC.

* **Added Broadcom BCMFS symmetric crypto PMD.**

  Added a symmetric crypto PMD for Broadcom FlexSparc crypto units.
  See :doc:`../cryptodevs/bcmfs` guide for more details on this new PMD.

* **Updated NXP DPAA2_SEC crypto PMD.**

  * Added DES-CBC support for cipher_only, chain and ipsec protocol.
  * Added support for non-HMAC auth algorithms
    (MD5, SHA1, SHA224, SHA256, SHA384, SHA512).

* **Updated Marvell NITROX symmetric crypto PMD.**

  * Added AES-GCM support.
  * Added cipher only offload support.

* **Updated Marvell OCTEON TX2 crypto PMD.**

  * Updated the OCTEON TX2 crypto PMD lookaside protocol offload for IPsec with
    IPv6 support.

* **Updated Intel QAT PMD.**

  * Added Raw Data-path APIs support.
  * Added support for write combining stores.

* **Added Intel ACC100 bbdev PMD.**

  Added a new ``acc100`` bbdev driver for the Intel\ |reg| ACC100 accelerator
  also known as Mount Bryce.  See the
  :doc:`../bbdevs/acc100` BBDEV guide for more details on this new driver.

* **Updated rte_security library to support SDAP.**

  ``rte_security_pdcp_xform`` in ``rte_security`` lib is updated to enable
  5G NR processing of SDAP headers in PMDs.

* **Added Marvell OCTEON TX2 regex PMD.**

  Added a new PMD driver for the hardware regex offload block for OCTEON TX2 SoC.

  See the :doc:`../regexdevs/octeontx2` for more details.

* **Updated Software Eventdev driver.**

  Added performance tuning arguments to allow tuning the scheduler for
  better throughput in high core count use cases.

* **Added a new driver for the Intel Dynamic Load Balancer v1.0 device.**

  Added the new ``dlb`` eventdev driver for the Intel DLB V1.0 device. See the
  :doc:`../eventdevs/dlb` eventdev guide for more details on this new driver.

* **Added a new driver for the Intel Dynamic Load Balancer v2.0 device.**

  Added the new ``dlb2`` eventdev driver for the Intel DLB V2.0 device. See the
  :doc:`../eventdevs/dlb2` eventdev guide for more details on this new driver.

* **Added Ice Lake (Gen4) support for Intel NTB.**

  Added NTB device support (4th generation) for the Intel Ice Lake platform.

* **Updated ioat rawdev driver.**

  The ioat rawdev driver has been updated and enhanced. Changes include:

  * Added support for Intel\ |reg| Data Streaming Accelerator hardware.  For
    more information, see `Introducing the Intel Data Streaming Accelerator
    (Intel DSA)
    <https://01.org/blogs/2019/introducing-intel-data-streaming-accelerator>`_.
  * Added support for the fill operation via the API ``rte_ioat_enqueue_fill()``,
    where the hardware fills an area of memory with a repeating pattern.
  * Added a per-device configuration flag to disable management
    of user-provided completion handles.
  * Renamed the ``rte_ioat_do_copies()`` API to ``rte_ioat_perform_ops()``,
    and renamed the ``rte_ioat_completed_copies()`` API to ``rte_ioat_completed_ops()``
    to better reflect the APIs' purposes, and remove the implication that
    they are limited to copy operations only.
    Note: The old API is still provided but marked as deprecated in the code.
  * Added a new API ``rte_ioat_fence()`` to add a fence between operations.
    This API replaces the ``fence`` flag parameter in the ``rte_ioat_enqueue_copies()`` function,
    and is clearer as there is no ambiguity as to whether the flag should be
    set on the last operation before the fence or the first operation after it.

* **Updated the pipeline library for alignment with the P4 language.**

  Added a new Software Switch (SWX) pipeline type that provides more
  flexibility through APIs and feature alignment with the P4 language.
  Some enhancements are:

  * The packet headers, meta-data, actions, tables and pipelines are
    dynamically defined instead of selected from a pre-defined set.
  * The actions and the pipeline are defined with instructions.
  * Extern objects and functions can be plugged into the pipeline.
  * Transaction-oriented table updates.

* **Added new AVX512 specific classify algorithms for ACL library.**

  * Added new ``RTE_ACL_CLASSIFY_AVX512X16`` vector implementation,
    which can process up to 16 flows in parallel. Requires AVX512 support.

  * Added new ``RTE_ACL_CLASSIFY_AVX512X32`` vector implementation,
    which can process up to 32 flows in parallel. Requires AVX512 support.

* **Added AVX512 lookup implementation for FIB.**

  Added a AVX512 lookup functions implementation into FIB and FIB6 libraries.

* **Added support to update subport bandwidth dynamically.**

   * Added new API ``rte_sched_port_subport_profile_add`` to add new
     subport bandwidth profiles to the subport profile table at runtime.

   * Added support to update the subport rate dynamically.

* **Updated FIPS validation sample application.**

  * Added scatter gather support.
  * Added NIST GCMVS complaint GMAC test method support.

* **Updated l3wfd-acl sample application.**

  * Added new optional parameter ``--eth-dest`` for the ``l3fwd-acl`` to allow
    the user to specify the destination mac address for each ethernet port
    used.
  * Replaced ``--scalar`` command-line option with ``--alg=<value>``, to allow
    the user to select the desired classify method.

* **Updated vhost sample application.**

  Added vhost asynchronous APIs support, which demonstrates how the application
  can leverage IOAT DMA channels with vhost asynchronous APIs.
  See the :doc:`../sample_app_ug/vhost` for more details.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* build: Support for the Make build system has been removed from DPDK.
  Meson is now the primary build system.
  Sample applications can still be built with Make standalone, using pkg-config.

* vhost: Dequeue zero-copy support has been removed.

* kernel: The module ``igb_uio`` has been moved to the git repository
  `dpdk-kmods <https://git.dpdk.org/dpdk-kmods/>`_ in a new directory
  ``linux/igb_uio``.

* Removed Python 2 support since it was sunsetted in January 2020. See
  `Sunsetting Python 2 <https://www.python.org/doc/sunset-python-2/>`_

* Removed TEP termination sample application.

* Removed the deprecated ``dpdk-setup.sh`` script.


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

* build macros: The macros defining ``RTE_MACHINE_CPUFLAG_*`` have been removed.
  The information provided by these macros is now available through standard
  compiler macros.

* eal: Replaced the function ``rte_get_master_lcore()`` with
  ``rte_get_main_lcore()``. The old function is deprecated.

  The iterator for worker lcores is also changed:
  ``RTE_LCORE_FOREACH_SLAVE`` is replaced with
  ``RTE_LCORE_FOREACH_WORKER``.

* eal: The definitions related to including and excluding devices
  have been changed from blacklist/whitelist to block/allow list.
  There are compatibility macros and command line mapping to accept
  the old values but applications and scripts are strongly encouraged
  to migrate to the new names.

* eal: The ``rte_logs`` struct and global symbol was made private
  and is no longer part of the API.

* eal: Made the ``rte_dev_event`` structure private to the EAL as no public API
  used it.

* eal: ``rte_cio_rmb()`` and ``rte_cio_wmb()`` were deprecated since 20.08
  and are removed in this release.

* mem: Removed the unioned field ``phys_addr`` from
  the structures ``rte_memseg`` and ``rte_memzone``.
  The field ``iova`` remains from the old unions.

* mempool: Removed the unioned fields ``phys_addr`` and ``physaddr`` from
  the structures ``rte_mempool_memhdr`` and ``rte_mempool_objhdr``.
  The field ``iova`` remains from the old unions.
  The flag name ``MEMPOOL_F_NO_PHYS_CONTIG`` is removed,
  while the aliased flag ``MEMPOOL_F_NO_IOVA_CONTIG`` is kept.

* mbuf: Removed the functions ``rte_mbuf_data_dma_addr*``
  and the macros ``rte_pktmbuf_mtophys*``.
  The same functionality is still available with the functions and macros
  having ``iova`` in their names instead of ``dma_addr`` or ``mtophys``.

* mbuf: Removed the unioned field ``buf_physaddr`` from ``rte_mbuf``.
  The field ``buf_iova`` remains from the old union.

* mbuf: Removed the unioned field ``refcnt_atomic`` from
  the structures ``rte_mbuf`` and ``rte_mbuf_ext_shared_info``.
  The field ``refcnt`` remains from the old unions.

* mbuf: Removed the unioned fields ``userdata`` and ``udata64``
  from the structure ``rte_mbuf``. It is replaced with dynamic fields.

* mbuf: Removed the field ``seqn`` from the structure ``rte_mbuf``.
  It is replaced with dynamic fields.

* mbuf: Removed the field ``timestamp`` from the structure ``rte_mbuf``.
  It is replaced with the dynamic field RTE_MBUF_DYNFIELD_TIMESTAMP_NAME
  which was previously used only for Tx.

* pci: Removed the ``rte_kernel_driver`` enum defined in rte_dev.h and
  replaced with a private enum in the PCI subsystem.

* pci: Removed the PCI resources map API from the public API
  (``pci_map_resource`` and ``pci_unmap_resource``) and moved it to the
  PCI bus driver along with the PCI resources lists and associated structures
  (``pci_map``, ``pci_msix_table``, ``mapped_pci_resource`` and
  ``mapped_pci_res_list``).

* ethdev: Removed the ``kdrv`` field in the ethdev ``rte_eth_dev_data``
  structure as it gave no useful abstracted information to the applications.

* ethdev: ``rte_eth_rx_descriptor_done()`` API has been deprecated.

* ethdev: Renamed basic statistics per queue. An underscore is inserted
  between the queue number and the rest of the xstat name:

  * ``rx_qN*`` -> ``rx_qN_*``
  * ``tx_qN*`` -> ``tx_qN_*``

* ethdev: Added capability to query age flow action.

* ethdev: Changed ``rte_eth_dev_stop`` return value from ``void`` to
  ``int`` to provide a way to report various error conditions.

* ethdev: Added ``int`` return type to ``rte_eth_dev_close()``.

* ethdev: Renamed internal functions:

  * ``_rte_eth_dev_callback_process()`` -> ``rte_eth_dev_callback_process()``
  * ``_rte_eth_dev_reset`` -> ``rte_eth_dev_internal_reset()``

* ethdev: Modified field type of ``base`` and ``nb_queue`` in struct
  ``rte_eth_dcb_tc_queue_mapping`` from ``uint8_t`` to ``uint16_t``.
  As the data of ``uint8_t`` will be truncated when queue number in
  a TC is greater than 256.

* ethdev: Removed the legacy filter API, including
  ``rte_eth_dev_filter_supported()`` and ``rte_eth_dev_filter_ctrl()``.

* ethdev: Removed the legacy L2 tunnel configuration API, including
  ``rte_eth_dev_l2_tunnel_eth_type_conf()`` and
  ``rte_eth_dev_l2_tunnel_offload_set()``..

* vhost: Moved vDPA APIs from experimental to stable.

* vhost: Add a new function ``rte_vhost_crypto_driver_start`` to be called
  instead of ``rte_vhost_driver_start`` by crypto applications.

* cryptodev: The structure ``rte_crypto_sym_vec`` is updated to support both
  cpu_crypto synchronous operations and asynchronous raw data-path APIs.

* cryptodev: ``RTE_CRYPTO_AEAD_LIST_END`` from ``enum rte_crypto_aead_algorithm``,
  ``RTE_CRYPTO_CIPHER_LIST_END`` from ``enum rte_crypto_cipher_algorithm`` and
  ``RTE_CRYPTO_AUTH_LIST_END`` from ``enum rte_crypto_auth_algorithm``
  are removed to avoid future ABI breakage while adding new algorithms.

* scheduler: Renamed functions ``rte_cryptodev_scheduler_slave_attach``,
  ``rte_cryptodev_scheduler_slave_detach`` and
  ``rte_cryptodev_scheduler_slaves_get`` to
  ``rte_cryptodev_scheduler_worker_attach``,
  ``rte_cryptodev_scheduler_worker_detach`` and
  ``rte_cryptodev_scheduler_workers_get`` accordingly.

* scheduler: Renamed the configuration value
  ``RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES`` to
  ``RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS``.

* security: The ``hfn_ovrd`` field in ``rte_security_pdcp_xform`` is changed from
  ``uint32_t`` to ``uint8_t`` so that a new field ``sdap_enabled`` can be added
  to support SDAP.

* security: The API ``rte_security_session_create`` is updated to take two
  mempool objects: one for session and other for session private data.
  So the application need to create two mempools and get the size of session
  private data using API ``rte_security_session_get_size`` for private session
  mempool.

* ipsec: ``RTE_SATP_LOG2_NUM`` has been dropped from ``enum`` and
  subsequently moved ``rte_ipsec`` lib from experimental to stable.

* baseband/fpga_lte_fec: Renamed function ``fpga_lte_fec_configure`` to
  ``rte_fpga_lte_fec_configure`` and structure ``fpga_lte_fec_conf`` to
  ``rte_fpga_lte_fec_conf``.

* baseband/fpga_5gnr_fec: Renamed function ``fpga_5gnr_fec_configure`` to
  ``rte_fpga_5gnr_fec_configure`` and structure ``fpga_5gnr_fec_conf`` to
  ``rte_fpga_5gnr_fec_conf``.

* rawdev: Added a structure size parameter to the functions
  ``rte_rawdev_queue_setup()``, ``rte_rawdev_queue_conf_get()``,
  ``rte_rawdev_info_get()`` and ``rte_rawdev_configure()``,
  allowing limited driver type-checking and ABI compatibility.

* rawdev: Changed the return type of the function ``rte_dev_info_get()``
  and the function ``rte_rawdev_queue_conf_get()``
  from ``void`` to ``int`` allowing the return of error codes from drivers.

* rawdev: The running of a drivers ``selftest()`` function can now be done
  using the ``rawdev_autotest`` command in the ``dpdk-test`` binary. This
  command now calls the self-test function for each rawdev found on the
  system, and does not require a specific command per device type.
  Following this change, the ``ioat_rawdev_autotest`` command has been
  removed as no longer needed.

* raw/ioat: As noted above, the ``rte_ioat_do_copies()`` and
  ``rte_ioat_completed_copies()`` functions have been renamed to
  ``rte_ioat_perform_ops()`` and ``rte_ioat_completed_ops()`` respectively.

* stack: the experimental tag has been dropped from the stack library, and its
  interfaces are considered stable as of DPDK 20.11.

* bpf: ``RTE_BPF_XTYPE_NUM`` has been dropped from ``rte_bpf_xtype``.

* gso: Changed ``rte_gso_segment`` behaviour and return value:

  * ``pkt`` is not saved to ``pkts_out[0]`` if not GSOed.
  * Return 0 instead of 1 for the above case.
  * ``pkt`` is not freed, no matter whether it is GSOed, leaving to the caller.

* acl: ``RTE_ACL_CLASSIFY_NUM`` enum value has been removed.
  This enum value was not used inside DPDK, while it prevented the addition of new
  classify algorithms without causing an ABI breakage.

* sched: Added ``subport_profile_id`` as an argument
  to function ``rte_sched_subport_config``.

* sched: Removed ``tb_rate``, ``tc_rate``, ``tc_period`` and ``tb_size``
  from ``struct rte_sched_subport_params``.


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

* eal: Removed the unimplemented function ``rte_dump_registers()``.

* ``ethdev`` changes

  * The following device operation function pointers moved
    from ``struct eth_dev_ops`` to ``struct rte_eth_dev``:

    * ``eth_rx_queue_count_t       rx_queue_count;``
    * ``eth_rx_descriptor_done_t   rx_descriptor_done;``
    * ``eth_rx_descriptor_status_t rx_descriptor_status;``
    * ``eth_tx_descriptor_status_t tx_descriptor_status;``

  * ``struct eth_dev_ops`` is no longer accessible by applications,
    which was already an internal data structure.

  * ``ethdev`` internal functions are marked with ``__rte_internal`` tag.

  * Added extensions' attributes to struct ``rte_flow_item_ipv6``.
    A set of additional values added to struct, indicating the existence of
    every defined extension header type.
    Applications should use the new values for identification of existing
    extensions in the packet header.

  * Added fields ``rx_seg`` and ``rx_nseg`` to ``rte_eth_rxconf`` structure
    to provide extended description of the receiving buffer.

  * ``struct rte_eth_hairpin_conf`` has two new members:

    * ``uint32_t tx_explicit:1;``
    * ``uint32_t manual_bind:1;``

  * Added new field ``has_vlan`` to structure ``rte_flow_item_eth``,
    indicating that packet header contains at least one VLAN.

  * Added new field ``has_more_vlan`` to the structure
    ``rte_flow_item_vlan``, indicating that packet header contains
    at least one more VLAN, after this VLAN.

* eventdev: The following structures are modified to support DLB/DLB2 PMDs
  and future extensions:

  * ``rte_event_dev_info``
  * ``rte_event_dev_config``
  * ``rte_event_port_conf``

* sched: Added new fields to ``struct rte_sched_subport_port_params``.

* lpm: Removed fields other than ``tbl24`` and ``tbl8`` from the struct
  ``rte_lpm``. The removed fields were made internal.


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

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Atom\ |trade| CPU C3858 @ 2.00GHz
    * Intel\ |reg| Atom\ |trade| CPU C3958 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1541 @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 0 @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 5218N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 8.2
    * Fedora 33
    * FreeBSD 12.1
    * OpenWRT 19.07.3
    * Red Hat Enterprise Linux Server release 8.2
    * Suse 15 SP1
    * Ubuntu 18.04
    * Ubuntu 20.04
    * Ubuntu 20.10

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G）

      * Firmware version: 2.30 0x80004dcf 1.2839.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.3.0 (ice)
      * OS Default DDP: 1.3.20.0
      * COMMS DDP: 1.3.24.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G）

      * Firmware version: 2.30 0x80004dd0 1.2839.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.3.0 (ice)
      * OS Default DDP: 1.3.20.0
      * COMMS DDP: 1.3.24.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.7.1 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.7.1 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 8.00 0x80008b82 1.2766.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.13.10 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 5.00 0x800023c3 1.2766.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.13.10 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.13.10 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 8.00 0x80008c1a 1.2766.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.13.10 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 8.00 0x80008b82 1.2766.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.13.10 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000cbc
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Ethernet Controller 10-Gigabit X540-AT2

      * Firmware version: 0x800005f9
      * Device id (pf): 8086:1528
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.00 0x80008d10 1.2766.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.13.10 (i40e)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 8.2 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.8 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Red Hat Enterprise Linux Server release 7.3 (Maipo)
    * Red Hat Enterprise Linux Server release 7.2 (Maipo)
    * Ubuntu 20.04
    * Ubuntu 18.04
    * Ubuntu 16.04
    * SUSE Enterprise Linux 15 SP2
    * SUSE Enterprise Linux 12 SP4

  * OFED:

    * MLNX_OFED 5.2-0.3.3.0 and above
    * MLNX_OFED 5.1-2.5.8.0

  * upstream kernel:

    * Linux 5.10.0-rc2 and above

  * rdma-core:

    * rdma-core-31.0-1 and above

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCCT (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.29.0476 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.29.0470 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg| 2 SmartNIC MT41686 - MBF2H332A-AEEOT (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 24.29.0476 and above

  * Embedded software:

    * CentOS Linux release 7.6.1810 (AltArch)
    * MLNX_OFED 5.2-0.3.2 and above
    * DPDK application running on Arm cores

* Intel\ |reg| platforms with Broadcom\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2667 v3 @ 3.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v2 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6142 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Silver 4110 CPU @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU E3-1270 v3 @ 3.50GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6134M CPU @ 3.20GHz

  * OS:

    * Red Hat Enterprise Linux Server release 8.1
    * Red Hat Enterprise Linux Server release 7.6
    * Red Hat Enterprise Linux Server release 7.5
    * Ubuntu 16.04
    * Centos 8.1
    * Centos 7.8
    * Centos 7.7

  * upstream kernel:

    * Linux 5.3.4

  * NICs:

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P225p (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Firmware version: 214.4.114.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 216.4.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 216.1.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 216.1.259.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 216.1.259.0 and above

* Broadcom\ |reg| NetXtreme-S\ |reg| Series SmartNIC

  * Broadcom\ |reg| NetXtreme-S\ |reg| Series PS225-H16 SmartNIC (2x25G)

    * Host interface: PCI Express 3.0 x8
    * Firmware version: 217.0.59.0, 218.1.63.0

  * Embedded software:

    * Broadcom Yocto Linux
    * Kernel version: 4.14.174
    * DPDK application running on 8 Arm Cortex-A72 cores

20.11.1 Release Notes
---------------------

20.11.1 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: fix CSV output format
* app/crypto-perf: fix latency CSV output
* app/crypto-perf: fix spelling in output
* app/crypto-perf: remove always true condition
* app/eventdev: adjust event count order for pipeline test
* app/eventdev: fix SMP barrier in performance test
* app/eventdev: remove redundant enqueue in burst Tx
* app: fix build with extra include paths
* app/flow-perf: simplify objects initialization
* app/procinfo: fix check on xstats-ids
* app/procinfo: fix _filters stats reporting
* app/procinfo: fix security context info
* app/procinfo: remove useless assignment
* app/procinfo: remove useless memset
* app/testpmd: avoid exit without terminal restore
* app/testpmd: fix help of metering commands
* app/testpmd: fix IP checksum calculation
* app/testpmd: fix key for RSS flow rule
* app/testpmd: fix max Rx packet length for VLAN packets
* app/testpmd: fix packets dump overlapping
* app/testpmd: fix queue reconfig request on Rx split update
* app/testpmd: fix queue stats mapping configuration
* app/testpmd: fix setting maximum packet length
* app/testpmd: fix start index for showing FEC array
* app/testpmd: release flows left before port stop
* app/testpmd: support shared age action query
* bitrate: fix missing header include
* build: fix linker flags on Windows
* build: fix plugin load on static build
* build: force pkg-config for dependency detection
* build: provide suitable error for "both" libraries option
* bus/pci: fix build with MinGW-w64 8
* bus/pci: fix build with Windows SDK >= 10.0.20253
* bus/pci: fix hardware ID limit on Windows
* bus/pci: ignore missing NUMA node on Windows
* common/mlx5: fix completion queue entry size configuration
* common/mlx5: fix pointer cast on Windows
* common/mlx5: fix storing synced MAC to internal table
* common/octeontx2: fix build with SVE
* common/sfc_efx/base: apply mask to value on match field set
* common/sfc_efx/base: check for MAE privilege
* common/sfc_efx/base: enhance field ID check in field set API
* common/sfc_efx/base: fix MAE match spec class comparison API
* common/sfc_efx/base: fix MAE match spec validation helper
* common/sfc_efx/base: fix MPORT related byte order handling
* common/sfc_efx/base: fix signed/unsigned mismatch warnings
* common/sfc_efx/base: remove warnings about inline specifiers
* common/sfc_efx/base: support alternative MAE match fields
* common/sfc_efx/base: update MCDI headers for MAE privilege
* crypto/dpaa2_sec: fix memory allocation check
* crypto/qat: fix access to uninitialized variable
* crypto/qat: fix digest in buffer
* doc: add FEC to NIC features
* doc: add vtune profiling config to prog guide
* doc: fix figure numbering in graph guide
* doc: fix mark action zero value in mlx5 guide
* doc: fix product link in hns3 guide
* doc: fix QinQ flow rules in testpmd guide
* doc: fix RSS flow description in i40e guide
* doc: fix some statements for ice vector PMD
* doc: fix supported feature table in mlx5 guide
* doc: update flow mark action in mlx5 guide
* eal/arm: fix debug build with gcc for 128-bit atomics
* eal: fix automatic loading of drivers as shared libs
* eal: fix internal ABI tag with clang
* eal: fix MCS lock header include
* eal: fix reciprocal header include
* eal/linux: fix handling of error events from epoll
* eal/windows: fix build with MinGW-w64 8
* eal/windows: fix C++ compatibility
* eal/windows: fix debug build with MinGW
* eal/windows: fix vfprintf warning with clang
* ethdev: avoid blocking telemetry for link status
* ethdev: fix close failure handling
* ethdev: fix max Rx packet length check
* ethdev: fix missing header include
* eventdev: fix a return value comment
* event/dlb: fix accessing uninitialized variables
* examples/eventdev: add info output for main core
* examples/eventdev: check CPU core enabling
* examples/eventdev: move ethdev stop to the end
* examples/l3fwd: remove limitation on Tx queue count
* examples/pipeline: fix CLI parsing crash
* examples/pipeline: fix VXLAN script permission
* fbarray: fix overlap check
* fib: fix missing header includes
* ip_frag: remove padding length of fragment
* ipsec: fix missing header include
* lib: fix doxygen for parameters of function pointers
* license: add licenses for exception cases
* lpm: fix vector IPv4 lookup
* mbuf: add C++ include guard for dynamic fields header
* mbuf: fix missing header include
* mbuf: remove unneeded atomic generic header include
* mempool: fix panic on dump or audit
* metrics: fix variable declaration in header
* net/af_xdp: remove useless assignment
* net/avp: remove always true condition
* net/axgbe: fix jumbo frame flag condition for MTU set
* net/bnxt: disable end of packet padding for Rx
* net/bnxt: fix cleanup on mutex init failure
* net/bnxt: fix doorbell write ordering
* net/bnxt: fix error handling in device start
* net/bnxt: fix fallback mbuf allocation logic
* net/bnxt: fix format specifier for unsigned int
* net/bnxt: fix freeing mbuf
* net/bnxt: fix FW version log
* net/bnxt: fix lock init and destroy
* net/bnxt: fix max rings computation
* net/bnxt: fix memory leak when mapping fails
* net/bnxt: fix null termination of Rx mbuf chain
* net/bnxt: fix outer UDP checksum Rx offload capability
* net/bnxt: fix packet type index calculation
* net/bnxt: fix PF resource query
* net/bnxt: fix Rx completion ring size calculation
* net/bnxt: fix Rx rings in RSS redirection table
* net/bnxt: fix VNIC config on Rx queue stop
* net/bnxt: fix VNIC RSS configure function
* net/bnxt: limit Rx representor packets per poll
* net/bnxt: make offload flags mapping per-ring
* net/bnxt: propagate FW command failure to application
* net/bnxt: refactor init/uninit
* net/bnxt: release HWRM lock in error
* net/bnxt: remove redundant return
* net/bnxt: set correct checksum status in mbuf
* net/bonding: fix PCI address comparison on non-PCI ports
* net/bonding: fix port id validity check on parsing
* net/bonding: remove local variable shadowing outer one
* net/cxgbe: accept VLAN flow items without ethertype
* net/cxgbe: fix jumbo frame flag condition
* net/dpaa2: fix jumbo frame flag condition for MTU set
* net/dpaa: fix jumbo frame flag condition for MTU set
* net/e1000: fix flow control mode setting
* net/e1000: fix jumbo frame flag condition for MTU set
* net/ena: fix Tx doorbell statistics
* net/ena: fix Tx SQ free space assessment
* net/ena: flush Rx buffers memory pool cache
* net/ena: prevent double doorbell
* net/ena: validate Rx req ID upon acquiring descriptor
* net/enetc: fix jumbo frame flag condition for MTU set
* net/enic: fix filter log message
* net/enic: fix filter type used for flow API
* net: fix missing header include
* net/hinic: fix jumbo frame flag condition for MTU set
* net/hinic: restore vectorised code
* net/hns3: adjust format specifier for enum
* net/hns3: adjust some comments
* net/hns3: fix build with SVE
* net/hns3: fix crash with multi-process
* net/hns3: fix data overwriting during register dump
* net/hns3: fix dump register out of range
* net/hns3: fix error code in xstats
* net/hns3: fix FEC state query
* net/hns3: fix firmware exceptions by concurrent commands
* net/hns3: fix flow director rule residue on malloc failure
* net/hns3: fix interception with flow director
* net/hns3: fix interrupt resources in Rx interrupt mode
* net/hns3: fix jumbo frame flag condition for MTU set
* net/hns3: fix link status change from firmware
* net/hns3: fix memory leak on secondary process exit
* net/hns3: fix query order of link status and link info
* net/hns3: fix register length when dumping registers
* net/hns3: fix RSS indirection table size
* net/hns3: fix Rx/Tx errors stats
* net/hns3: fix stats flip overflow
* net/hns3: fix VF query link status in dev init
* net/hns3: fix VF reset on mailbox failure
* net/hns3: fix xstats with id and names
* net/hns3: remove MPLS from supported flow items
* net/hns3: use new opcode for clearing hardware resource
* net/hns3: validate requested maximum Rx frame length
* net/i40e: add null input checks
* net/i40e: fix flex payload rule conflict
* net/i40e: fix global register recovery
* net/i40e: fix jumbo frame flag condition
* net/i40e: fix L4 checksum flag
* net/i40e: fix returned code for RSS hardware failure
* net/i40e: fix Rx bytes statistics
* net/i40e: fix stats counters
* net/i40e: fix VLAN stripping in VF
* net/i40e: fix X722 for 802.1ad frames ability
* net/iavf: fix conflicting RSS combination rules
* net/iavf: fix GTPU UL and DL support for flow director
* net/iavf: fix jumbo frame flag condition
* net/iavf: fix memory leak in large VF
* net/iavf: fix queue pairs configuration
* net/iavf: fix symmetric flow rule creation
* net/iavf: fix vector mapping with queue
* net/ice/base: fix memory handling
* net/ice/base: fix null pointer dereference
* net/ice/base: fix tunnel destroy
* net/ice: check Rx queue number on RSS init
* net/ice: disable IPv4 checksum offload in vector Tx
* net/ice: drain out DCF AdminQ command queue
* net/ice: enlarge Rx queue rearm threshold to 64
* net/ice: fix jumbo frame flag condition
* net/ice: fix outer checksum flags
* net/ice: fix outer UDP Tx checksum offload
* net/ice: fix RSS lookup table initialization
* net/ionic: allow separate L3 and L4 checksum offload
* net/ionic: do minor logging fixups
* net/ionic: fix address handling in Tx
* net/ionic: fix link speed and autonegotiation
* net/ionic: fix up function attribute tags
* net/ipn3ke: fix jumbo frame flag condition for MTU set
* net/ixgbe: detect failed VF MTU set
* net/ixgbe: disable NFS filtering
* net/ixgbe: fix configuration of max frame size
* net/ixgbe: fix flex bytes flow director rule
* net/ixgbe: fix jumbo frame flag condition
* net/ixgbe: fix UDP zero checksum on x86
* net/liquidio: fix jumbo frame flag condition for MTU set
* net/mlx4: fix device detach
* net/mlx4: fix handling of probing failure
* net/mlx4: fix port attach in secondary process
* net/mlx5: check FW miniCQE format capabilities
* net/mlx5: fix buffer split offload advertising
* net/mlx5: fix comparison sign in flow engine
* net/mlx5: fix constant array size
* net/mlx5: fix count actions query in sample flow
* net/mlx5: fix counter and age flow action validation
* net/mlx5: fix crash on secondary process port close
* net/mlx5: fix device name size on Windows
* net/mlx5: fix Direct Verbs flow descriptor allocation
* net/mlx5: fix drop action in tunnel offload mode
* net/mlx5: fix flow action destroy wrapper
* net/mlx5: fix flow operation wrapper per OS
* net/mlx5: fix flow split combined with age action
* net/mlx5: fix flow split combined with counter
* net/mlx5: fix flow tag decompression
* net/mlx5: fix freeing packet pacing
* net/mlx5: fix hairpin flow split decision
* net/mlx5: fix leak on ASO SQ creation failure
* net/mlx5: fix leak on Rx queue creation failure
* net/mlx5: fix leak on Tx queue creation failure
* net/mlx5: fix mark action in active tunnel offload
* net/mlx5: fix mbuf freeing in vectorized MPRQ
* net/mlx5: fix miniCQE configuration for Verbs
* net/mlx5: fix multi-process port ID
* net/mlx5: fix port attach in secondary process
* net/mlx5: fix shared age action validation
* net/mlx5: fix shared RSS and mark actions combination
* net/mlx5: fix shared RSS capability check
* net/mlx5: fix shared RSS translation and cleanup
* net/mlx5: fix tunnel rules validation on VF representor
* net/mlx5: fix Tx queue size created with DevX
* net/mlx5: fix unnecessary checking for RSS action
* net/mlx5: fix Verbs memory allocation callback
* net/mlx5: fix VXLAN decap on non-VXLAN flow
* net/mlx5: fix wire vport hint
* net/mlx5: refuse empty VLAN in flow pattern
* net/mlx5: remove CQE padding device argument
* net/mlx5: unify operations for all OS
* net/mlx5: validate hash Rx queue pointer
* net/mvneta: check allocation in Rx queue flush
* net/mvpp2: fix frame size checking
* net/mvpp2: fix stack corruption
* net/mvpp2: remove CRC length from MRU validation
* net/mvpp2: remove debug log on fast-path
* net/mvpp2: remove VLAN flush
* net/netvsc: ignore unsupported packet on sync command
* net/nfp: fix jumbo frame flag condition for MTU set
* net/nfp: read chip model from PluDevice register
* net/octeontx2: fix corruption in segments list
* net/octeontx2: fix jumbo frame flag condition for MTU
* net/octeontx2: fix PF flow action for Tx
* net/octeontx: fix build with SVE
* net/octeontx: fix jumbo frame flag condition for MTU set
* net/octeontx: fix max Rx packet length
* net/pcap: fix byte stats for drop Tx
* net/pcap: fix infinite Rx with large files
* net/pcap: remove local variable shadowing outer one
* net/qede: fix jumbo frame flag condition for MTU set
* net/qede: fix promiscuous enable
* net/sfc: fix generic byte statistics to exclude FCS bytes
* net/sfc: fix jumbo frame flag condition for MTU set
* net/sfc: fix TSO and checksum offloads for EF10
* net/thunderx: fix jumbo frame flag condition for MTU set
* net/virtio: add missing backend features negotiation
* net/virtio: fix getting old status on reconnect
* net/virtio: fix memory init with vDPA backend
* net/virtio-user: fix protocol features advertising
* net/virtio-user: fix run closing stdin and close callfd
* node: fix missing header include
* pipeline: fix missing header includes
* power: clean up includes
* power: create guest channel public header file
* power: export guest channel header file
* power: fix missing header includes
* power: make channel message functions public
* power: rename constants
* power: rename public structs
* regex/mlx5: fix memory rule alignment
* regex/mlx5: fix number of supported queues
* regex/mlx5: fix support for group id
* regex/octeontx2: fix PCI table overflow
* rib: fix insertion in some cases
* rib: fix missing header include
* rib: fix missing header includes
* service: propagate init error in EAL
* table: fix missing header include
* telemetry: fix missing header include
* test/distributor: fix return buffer queue overload
* test/event_crypto: set cipher operation in transform
* test: fix buffer overflow in Tx burst
* test: fix terminal settings on exit
* test/ipsec: fix result code for not supported
* test/mcslock: remove unneeded per lcore copy
* test/ring: reduce duration of performance tests
* test/rwlock: fix spelling and missing whitespace
* usertools: fix binding built-in kernel driver
* vdpa/mlx5: fix configuration mutex cleanup
* version: 20.11.1-rc1
* vhost: fix missing header includes
* vhost: fix packed ring dequeue offloading
* vhost: fix vid allocation race

20.11.1 Validation
~~~~~~~~~~~~~~~~~~

* Canonical(R) Testing

   * Build tests on Ubuntu 21.04
   * OVS-DPDK tests on x86_64
      * 1.0.0 (07:05:12): phys (BM) tests
      *   1.1.0 (07:05:12): initialize environment
      *     1.1.1 (07:09:32): testpmd  => Pass
      *     1.1.2 (07:11:12): check testpmd output  => Pass
      * 2.0.0 (07:11:12): prep virtual test environment
      * 1.0.0 (07:14:14): virt tests
      *   1.1.0 (07:14:14): initialize environment
      * 3.0.0 (07:15:30): performance tests
      *   3.1.0 (07:15:30): prep benchmarks
      *   3.2.0 (07:15:51): performance tests
      *     3.2.1 (07:16:01): test guest-openvswitch for OVS-5CPU  => Pass
      *     3.2.2 (07:35:44): test guest-dpdk-vhost-user-client-multiq for
      * OVSDPDK-VUC  => Pass
      * 4.0.0 (07:57:11): VUC endurance checks
      *   4.1.0 (07:57:11): prep VUC endurance tests
      *     4.1.1 (08:12:38): start stop guests (client)  => Pass
      *     4.1.2 (09:25:59): add/remove ports (client)  => Pass
      *   4.2.0 (09:35:04): Final cleanup


* Red Hat(R) Testing

   * Platform

      * RHEL 8
      * Kernel 4.18
      * Qemu 5.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q       * cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* Broadcom(R) Testing

   * Functionality

      * Tx/Rx
      * Link status
      * RSS
      * TSO
      * VLAN filtering
      * MAC filtering
      * statistics
      * Checksum offload
      * MTU
      * Promiscuous mode

   * Platform

      * BCM57414 NetXtreme-E 10Gb/25Gb Ethernet Controller, Firmware: 218.1.186.0
      * BCM57508 NetXtreme-E 10Gb/25Gb/40Gb/50Gb/100Gb/200Gb Ethernet, Firmware : 219.0.0.74


* Intel(R) Testing

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing
      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* Intel(R) Testing with Open vSwitch

   * OVS testing with OVS 2.15.0

      * ICE Device

         * Jumbo frames, RSS, Kernel forwarding

      * i40e Device

         * Basic performance (RFC2544 P2P, PVP_CONT, RFC2544 PVP_TPUT, RFC2544 PVVP_TPUT, PVPV), Jumbo frames, RSS

      * Niantic Device

         * Basic performance tests (RFC2544 P2P, PVP_CONT, RFC2544 PVP_TPUT, RFC2544 PVVP_TPUT, PVPV), Jumbo frames, RSS

      * vhost

         * Port addition/deletion, Jumbo frames, RSS


* Nvidia(R) Testing

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications

   * Build tests

      * Ubuntu 20.04.1 with MLNX_OFED_LINUX-5.2-2.2.0.0.
      * Ubuntu 20.04.1 with rdma-core master (7f2d460).
      * Ubuntu 20.04.1 with rdma-core v28.0.
      * Ubuntu 18.04.5 with rdma-core v17.1.
      * Ubuntu 18.04.5 with rdma-core master (7f2d460) (i386).
      * Ubuntu 16.04.7 with rdma-core v22.7.
      * Fedora 32 with rdma-core v33.0.
      * CentOS 7 7.9.2009 with rdma-core master (7f2d460).
      * CentOS 7 7.9.2009 with MLNX_OFED_LINUX-5.2-2.2.0.0.
      * CentOS 8 8.3.2011 with rdma-core master (7f2d460).
      * openSUSE Leap 15.2 with rdma-core v27.1.

   * ConnectX-5

      * RHEL 7.4
      * Driver MLNX_OFED_LINUX-5.2-2.2.0.0
      * Kernel: 5.12.0-rc1 / Driver: rdma-core 34.0
      * fw 14.29.2002

   * ConnectX-4 Lx

      * RHEL 7.4
      * Driver MLNX_OFED_LINUX-5.2-2.2.0.0
      * Kernel: 5.12.0-rc1 / Driver: rdma-core 34.0
      * fw 16.29.2002

20.11.1 Known Issues
~~~~~~~~~~~~~~~~~~~~

* ICE

   * creating 512 acl rules after creating a full mask switch rule fails.

* vhost/virtio

   * udp-fragmentation-offload cannot be setup on Ubuntu 19.10 VMs.
     https://bugzilla.kernel.org/show_bug.cgi?id=207075
   * vm2vm virtio-net connectivity between two vms randomly fails due
     to lost connection after vhost reconnect.

20.11.2 Release Notes
---------------------

20.11.2 Fixes
~~~~~~~~~~~~~

* acl: fix build with GCC 11
* app/bbdev: check memory allocation
* app/bbdev: fix HARQ error messages
* app/crypto-perf: check memory allocation
* app/eventdev: fix lcore parsing skipping last core
* app/eventdev: fix overflow in lcore list parsing
* app/eventdev: fix timeout accuracy
* app: fix exit messages
* app/flow-perf: fix encap/decap actions
* app/regex: fix usage text
* app/testpmd: check MAC address query
* app/testpmd: fix bitmap of link speeds when force speed
* app/testpmd: fix build with musl
* app/testpmd: fix DCB forwarding configuration
* app/testpmd: fix DCB re-configuration
* app/testpmd: fix division by zero on socket memory dump
* app/testpmd: fix forward lcores number for DCB
* app/testpmd: fix max queue number for Tx offloads
* app/testpmd: fix NVGRE encap configuration
* app/testpmd: fix segment number check
* app/testpmd: fix tunnel offload flows cleanup
* app/testpmd: fix Tx/Rx descriptor query error log
* app/testpmd: fix usage text
* app/testpmd: remove unnecessary UDP tunnel check
* app/testpmd: verify DCB config during forward config
* bpf: fix JSLT validation
* build: detect execinfo library on Linux
* build: exclude meson files from examples installation
* build: fix drivers selection without Python
* build: remove redundant _GNU_SOURCE definitions
* buildtools: fix all drivers disabled on Windows
* buildtools: fix build with busybox
* bus/dpaa: fix 64-bit arch detection
* bus/dpaa: fix build with musl
* bus/dpaa: fix statistics reading
* bus/fslmc: fix random portal hangs with qbman 5.0
* bus/fslmc: remove unused debug macro
* bus/pci: fix Windows kernel driver categories
* bus/pci: skip probing some Windows NDIS devices
* bus/pci: support I/O port operations with musl
* ci: catch coredumps
* ci: enable v21 ABI checks
* ci: fix package installation in GitHub Actions
* ci: hook to GitHub Actions
* ci: ignore APT update failure in GitHub Actions
* common/dpaax/caamflib: fix build with musl
* common/dpaax: fix possible null pointer access
* common/iavf: fix duplicated offload bit
* common/mlx5: add DevX commands for queue counters
* common/mlx5: add DevX command to query WQ
* common/mlx5: add timestamp format support to DevX
* common/mlx5: fix DevX read output buffer size
* common/mlx5/linux: add glue function to query WQ
* common/qat: increase IM buffer size for GEN3
* common/sfc_efx/base: add missing MCDI response length checks
* common/sfc_efx/base: fix dereferencing null pointer
* common/sfc_efx/base: fix indication of MAE encap support
* common/sfc_efx/base: limit reported MCDI response length
* common/sfc_efx: remove GENEVE from supported tunnels
* compress/qat: enable compression on GEN3
* config/ppc: reduce number of cores and NUMA nodes
* crypto/dpaa2_sec: fix close and uninit functions
* crypto/dpaa_sec: affine the thread portal affinity
* crypto/octeontx: fix session-less mode
* crypto/qat: fix null authentication request
* crypto/qat: fix offset for out-of-place scatter-gather
* crypto/zuc: fix build with GCC 11
* devtools: fix orphan symbols check with busybox
* doc: fix build with Sphinx 4
* doc: fix formatting in testpmd guide
* doc: fix HiSilicon copyright syntax
* doc: fix matching versions in ice guide
* doc: fix multiport syntax in nfp guide
* doc: fix names of UIO drivers
* doc: fix runtime options in DLB2 guide
* doc: fix sphinx rtd theme import in GHA
* doc: remove PDF requirements
* doc: update recommended versions for i40e
* drivers: fix log level after loading
* drivers/net: fix FW version query
* eal: add C++ include guard for reciprocal header
* eal/arm64: fix platform register bit
* eal: fix build with musl
* eal: fix comment of OS-specific header files
* eal: fix evaluation of log level option
* eal: fix hang in control thread creation
* eal: fix leak in shared lib mode detection
* eal: fix memory mapping on 32-bit target
* eal: fix race in control thread creation
* eal: fix service core list parsing
* eal/windows: add missing SPDX license tag
* eal/windows: fix default thread priority
* eal/windows: fix return codes of pthread shim layer
* ethdev: add missing buses in device iterator
* ethdev: update flow item GTP QFI definition
* ethdev: validate input in EEPROM info
* ethdev: validate input in module EEPROM dump
* ethdev: validate input in register info
* eventdev: fix case to initiate crypto adapter service
* eventdev: fix memory leakage on thread creation failure
* eventdev: remove redundant thread name setting
* event/dlb2: remove references to deferred scheduling
* event/dlb: fix header includes for musl
* event/dpaa2: remove unused macros
* event/octeontx2: configure crypto adapter xaq pool
* event/octeontx2: fix crypto adapter queue pair operations
* event/octeontx2: fix device reconfigure for single slot
* event/octeontx2: fix XAQ pool reconfigure
* examples: add eal cleanup to examples
* examples/bbdev: fix header include for musl
* examples/ethtool: remove unused parsing
* examples: fix pkg-config override
* examples/flow_classify: fix NUMA check of port and core
* examples/l2fwd-cat: fix NUMA check of port and core
* examples/l2fwd-crypto: fix packet length while decryption
* examples/l2fwd-crypto: skip masked devices
* examples/l3fwd: fix LPM IPv6 subnets
* examples/l3fwd-power: fix empty poll thresholds
* examples/packet_ordering: fix port configuration
* examples/ptpclient: remove wrong comment
* examples/rxtx_callbacks: fix port ID format specifier
* examples/skeleton: fix NUMA check of port and core
* examples/timer: fix time interval
* examples/vhost: check memory table query
* examples/vhost_crypto: remove unused short option
* fbarray: fix log message on truncation error
* ipc: check malloc sync reply result
* ipc: use monotonic clock
* ip_frag: fix fragmenting IPv4 packet with header option
* kni: fix kernel deadlock with bifurcated device
* kni: refactor user request processing
* kni: support async user request
* license: fix typos
* log/linux: make default output stderr
* mbuf: check shared memory before dumping dynamic space
* mem: fix freeing segments in --huge-unlink mode
* net/af_xdp: fix error handling during Rx queue setup
* net/ark: fix leak on thread termination
* net/ark: refactor Rx buffer recovery
* net/ark: update packet director initial state
* net/bnx2x: fix build with GCC 11
* net/bnx2x: fix build with GCC 11
* net/bnxt: check kvargs parsing
* net/bnxt: check PCI config read
* net/bnxt: drop unused attribute
* net/bnxt: fix configuring LRO
* net/bnxt: fix device readiness check
* net/bnxt: fix double free in port start failure
* net/bnxt: fix dynamic VNIC count
* net/bnxt: fix firmware fatal error handling
* net/bnxt: fix FW readiness check during recovery
* net/bnxt: fix handling of null flow mask
* net/bnxt: fix health check alarm cancellation
* net/bnxt: fix HWRM and FW incompatibility handling
* net/bnxt: fix link state operations
* net/bnxt: fix memory allocation for command response
* net/bnxt: fix mismatched type comparison in MAC restore
* net/bnxt: fix mismatched type comparison in Rx
* net/bnxt: fix PCI write check
* net/bnxt: fix PTP support for Thor
* net/bnxt: fix queues per VNIC
* net/bnxt: fix resource cleanup
* net/bnxt: fix ring count calculation for Thor
* net/bnxt: fix RSS context cleanup
* net/bnxt: fix Rx and Tx timestamps
* net/bnxt: fix Rx buffer posting
* net/bnxt: fix Rx descriptor status
* net/bnxt: fix Rx queue count
* net/bnxt: fix Rx timestamp when FIFO pending bit is set
* net/bnxt: fix single PF per port check
* net/bnxt: fix timesync when PTP is not supported
* net/bnxt: fix Tx length hint threshold
* net/bnxt: fix Tx timestamp init
* net/bnxt: fix VF info allocation
* net/bnxt: fix VNIC configuration
* net/bnxt: fix xstats get
* net/bnxt: mute some failure logs
* net/bnxt: prevent device access in error state
* net/bnxt: refactor multi-queue Rx configuration
* net/bnxt: remove unnecessary forward declarations
* net/bnxt: remove unused function parameters
* net/bnxt: remove unused macro
* net/bnxt: use prefix on global function
* net/bonding: fix adding itself as its slave
* net/bonding: fix LACP system address check
* net/bonding: fix leak on remove
* net/bonding: fix socket ID check
* net/cxgbe: remove use of uint type
* net/dpaa2: fix getting link status
* net/dpaa: fix getting link status
* net/e1000/base: fix timeout for shadow RAM write
* net/e1000: fix flow error message object
* net/e1000: fix max Rx packet size
* net/e1000: fix Rx error counter for bad length
* net/e1000: remove MTU setting limitation
* net/ena/base: destroy multiple wait events
* net/ena/base: fix type conversions by explicit casting
* net/ena/base: improve style and comments
* net/ena: fix crash with unsupported device argument
* net/ena: fix parsing of large LLQ header device argument
* net/ena: fix releasing Tx ring mbufs
* net/ena: indicate Rx RSS hash presence
* net/ena: remove endian swap functions
* net/ena: report default ring size
* net/ena: switch memcpy to optimized version
* net/enic: enable GENEVE offload via VNIC configuration
* net/enic: fix flow initialization error handling
* net/failsafe: fix RSS hash offload reporting
* net/failsafe: report minimum and maximum MTU
* net: fix comment in IPv6 header
* net/hinic: fix crash in secondary process
* net/hns3: clear hash map on flow director clear
* net/hns3: delete redundant blank line
* net/hns3: fail setting FEC if one bit mode is not supported
* net/hns3: fix concurrent interrupt handling
* net/hns3: fix configure FEC when concurrent with reset
* net/hns3: fix copyright date
* net/hns3: fix DCB configuration
* net/hns3: fix DCB mode check
* net/hns3: fix DCB reconfiguration
* net/hns3: fix device capabilities for copper media type
* net/hns3: fix flow control exception
* net/hns3: fix flow control mode
* net/hns3: fix flow counter value
* net/hns3: fix flow director lock
* net/hns3: fix FLR miss detection
* net/hns3: fix handling link update
* net/hns3: fix HW buffer size on MTU update
* net/hns3: fix link speed when port is down
* net/hns3: fix link speed when VF device is down
* net/hns3: fix link status when port is stopped
* net/hns3: fix link update when failed to get link info
* net/hns3: fix log on flow director clear
* net/hns3: fix long task queue pairs reset time
* net/hns3: fix mailbox error message
* net/hns3: fix mailbox message ID in log
* net/hns3: fix mbuf leakage
* net/hns3: fix missing outer L4 UDP flag for VXLAN
* net/hns3: fix MTU config complexity
* net/hns3: fix ordering in secondary process initialization
* net/hns3: fix possible mismatched response of mailbox
* net/hns3: fix processing link status message on PF
* net/hns3: fix processing Tx offload flags
* net/hns3: fix querying flow director counter for out param
* net/hns3: fix queue state when concurrent with reset
* net/hns3: fix reporting undefined speed
* net/hns3: fix requested FC mode rollback
* net/hns3: fix rollback after setting PVID failure
* net/hns3: fix Rx/Tx queue numbers check
* net/hns3: fix secondary process request start/stop Rx/Tx
* net/hns3: fix setting default MAC address in bonding of VF
* net/hns3: fix some packet types
* net/hns3: fix time delta calculation
* net/hns3: fix timing in mailbox
* net/hns3: fix timing in resetting queues
* net/hns3: fix TM QCN error event report by MSI-X
* net/hns3: fix Tx checksum for UDP packets with special port
* net/hns3: fix typos on comments
* net/hns3: fix use of command status enumeration
* net/hns3: fix vector Rx burst limitation
* net/hns3: fix verification of NEON support
* net/hns3: fix VF alive notification after config restore
* net/hns3: fix VF handling LSC event in secondary process
* net/hns3: fix VF mailbox head field
* net/hns3: fix VMDq mode check
* net/hns3: increase readability in logs
* net/hns3: log time delta in decimal format
* net/hns3: remove meaningless packet buffer rollback
* net/hns3: remove read when enabling TM QCN error event
* net/hns3: remove redundant mailbox response
* net/hns3: remove unused macro
* net/hns3: remove unused macros
* net/hns3: remove unused macros
* net/hns3: remove unused mailbox macro and struct
* net/hns3: remove unused parameter markers
* net/hns3: remove unused VMDq code
* net/hns3: remove VLAN/QinQ ptypes from support list
* net/hns3: return error on PCI config write failure
* net/hns3: support get device version when dump register
* net/hns3: update HiSilicon copyright syntax
* net/i40e: announce request queue capability in PF
* net/i40e: fix flow director config after flow validate
* net/i40e: fix flow director for common pctypes
* net/i40e: fix input set field mask
* net/i40e: fix IPv4 fragment offload
* net/i40e: fix lack of MAC type when set MAC address
* net/i40e: fix negative VEB index
* net/i40e: fix parsing packet type for NEON
* net/i40e: fix primary MAC type when starting port
* net/i40e: fix VF RSS configuration
* net/i40e: remove redundant VSI check in Tx queue setup
* net/i40evf: fix packet loss for X722
* net/iavf: fix crash in AVX512
* net/iavf: fix lack of MAC type when set MAC address
* net/iavf: fix packet length parsing in AVX512
* net/iavf: fix primary MAC type when starting port
* net/iavf: fix TSO max segment size
* net/iavf: fix VF to PF command failure handling
* net/iavf: fix wrong Tx context descriptor
* net/ice/base: cleanup filter list on error
* net/ice/base: fix build with GCC 11
* net/ice/base: fix memory allocation for MAC addresses
* net/ice/base: fix memory allocation wrapper
* net/ice/base: fix payload indicator on ptype
* net/ice/base: fix uninitialized struct
* net/ice: check some functions return
* net/ice: fix crash in AVX512
* net/ice: fix disabling promiscuous mode
* net/ice: fix fast mbuf freeing
* net/ice: fix illegal access when removing MAC filter
* net/ice: fix leak on thread termination
* net/ice: fix RSS for L2 packet
* net/ice: fix RSS hash update
* net/ice: fix VLAN filter with PF
* net/ice: fix VSI array out of bounds access
* net/igc: fix Rx error counter for bad length
* net/igc: fix Rx packet size
* net/igc: fix Rx RSS hash offload capability
* net/igc: fix speed configuration
* net/igc: remove MTU setting limitation
* net/igc: remove use of uint type
* net/ionic: fix completion type in lif init
* net/ixgbe: fix RSS RETA being reset after port start
* net/ixgbe: fix Rx errors statistics for UDP checksum
* net/kni: check init result
* net/kni: warn on stop failure
* net/memif: fix Tx bps statistics for zero-copy
* net/mlx4: fix buffer leakage on device close
* net/mlx4: fix leak when configured repeatedly
* net/mlx4: fix RSS action with null hash key
* net/mlx4: fix secondary process initialization ordering
* net/mlx5: fix counter offset detection
* net/mlx5: fix drop action for Direct Rules/Verbs
* net/mlx5: fix external buffer pool registration for Rx queue
* net/mlx5: fix flow actions index in cache
* net/mlx5: fix flow age event triggering
* net/mlx5: fix hashed list size for tunnel flow groups
* net/mlx5: fix leak when configured repeatedly
* net/mlx5: fix loopback for Direct Verbs queue
* net/mlx5: fix metadata item validation for ingress flows
* net/mlx5: fix missing shared RSS hash types
* net/mlx5: fix probing device in legacy bonding mode
* net/mlx5: fix receiving queue timestamp format
* net/mlx5: fix redundant flow after RSS expansion
* net/mlx5: fix resource release for mirror flow
* net/mlx5: fix RSS flow item expansion for GRE key
* net/mlx5: fix RSS flow item expansion for NVGRE
* net/mlx5: fix Rx metadata leftovers
* net/mlx5: fix Rx segmented packets on mbuf starvation
* net/mlx5: fix secondary process initialization ordering
* net/mlx5: fix shared inner RSS
* net/mlx5: fix tunnel offload private items location
* net/mlx5: fix UAR allocation diagnostics messages
* net/mlx5: fix using flow tunnel before null check
* net/mlx5/linux: fix firmware version
* net/mlx5: remove drop queue function prototypes
* net/mlx5: support RSS expansion for IPv6 GRE
* net/mlx5: support timestamp format
* net/nfp: fix reporting of RSS capabilities
* net/octeontx2: fix VLAN filter
* net/pcap: fix file descriptor leak on close
* net/pcap: fix format string
* net/qede: accept bigger RSS table
* net/qede: reduce log verbosity
* net/sfc: fix buffer size for flow parse
* net/sfc: fix error path inconsistency
* net/sfc: fix mark support in EF100 native Rx datapath
* net/sfc: fix outer rule rollback on error
* net/tap: check ioctl on restore
* net/tap: fix build with GCC 11
* net/tap: fix interrupt vector array size
* net/txgbe: fix QinQ strip
* net/txgbe: fix Rx missed packet counter
* net/txgbe: remove unused functions
* net/txgbe: update packet type
* net/vhost: restore pseudo TSO support
* net/virtio: fix getline memory leakage
* net/virtio: fix interrupt unregistering for listening socket
* net/virtio: fix vectorized Rx queue rearm
* pipeline: fix endianness conversions
* pipeline: fix instruction translation
* power: do not skip saving original P-state governor
* power: fix sanity checks for guest channel read
* power: remove duplicated symbols from map file
* power: save original ACPI governor always
* raw/ifpga: fix device name format
* raw/ioat: fix script for configuring small number of queues
* raw/ntb: check memory allocations
* raw/ntb: check SPAD user index
* raw/octeontx2_dma: assign PCI device in DPI VF
* raw/skeleton: add missing check after setting attribute
* regex/mlx5: support timestamp format
* regex/octeontx2: remove unused include directory
* sched: fix traffic class oversubscription parameter
* service: clean references to removed symbol
* stack: allow lock-free only on relevant architectures
* table: fix actions with different data size
* telemetry: fix race on callbacks list
* test/bpf: fix error message
* test: check flow classifier creation
* test: check thread creation
* test/cmdline: fix inputs array
* test/cmdline: silence clang 12 warning
* test/crypto: copy offset data to OOP destination buffer
* test/crypto: fix auth-cipher compare length in OOP
* test/crypto: fix build with GCC 11
* test/crypto: fix return value of a skipped test
* test/distributor: fix burst flush on worker quit
* test/distributor: fix worker notification in burst mode
* test/event: fix timeout accuracy
* test: fix autotest handling of skipped tests
* test: fix build with GCC 11
* test: fix division by zero
* test: fix TCP header initialization
* test/kni: check init result
* test/kni: fix a comment
* test/mem: fix page size for external memory
* test/mempool: fix object initializer
* test/power: add delay before checking CPU frequency
* test/power: add turbo mode to frequency check
* test/power: fix CPU frequency check
* test/power: fix low frequency test when turbo enabled
* test/power: fix turbo test
* test/power: round CPU frequency to check
* test: proceed if timer subsystem already initialized
* test/table: fix build with GCC 11
* test/timer: check memzone allocation
* test/trace: fix race on collected perf data
* vdpa/ifc: check PCI config read
* vdpa/mlx5: fix device unplug
* vdpa/mlx5: fix virtq cleaning
* vdpa/mlx5: support timestamp format
* version: 20.11.2-rc1
* version: 20.11.2-rc2
* vfio: do not merge contiguous areas
* vfio: fix API description
* vfio: fix DMA mapping granularity for IOVA as VA
* vfio: fix duplicated user mem map
* vhost: fix batch dequeue potential buffer overflow
* vhost: fix initialization of async temporary header
* vhost: fix initialization of temporary header
* vhost: fix offload flags in Rx path
* vhost: fix packed ring potential buffer overflow
* vhost: fix queue initialization
* vhost: fix redundant vring status change notification
* vhost: fix split ring potential buffer overflow

20.11.2 Validation
~~~~~~~~~~~~~~~~~~

* Intel(R) Testing

   * Basic Intel(R) NIC testing

      * PF(i40e, ixgbe, ice)
      * VF(i40e, ixgbe, ice)
      * Compile testing
      * Intel NIC single core/NIC performance

   * Basic cryptodev and virtio testing

      * Virtio function and performance
      * Cryptodev function and performance

* Nvidia(R) Testing

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO

   * Build tests

      * Ubuntu 20.04.2 with MLNX_OFED_LINUX-5.3-1.0.0.1.
      * Ubuntu 20.04.2 with rdma-core master (a66e2a5).
      * Ubuntu 20.04.2 with rdma-core v28.0.
      * Ubuntu 18.04.5 with rdma-core v17.1.
      * Ubuntu 18.04.5 with rdma-core master (a66e2a5) (i386).
      * Ubuntu 16.04.7 with rdma-core v22.7.
      * Fedora 34 with rdma-core v35.0.
      * Fedora 35 (Rawhide) with rdma-core v35.0 (only with gcc).
      * CentOS 7 7.9.2009 with rdma-core master (a66e2a5).
      * CentOS 7 7.9.2009 with MLNX_OFED_LINUX-5.3-1.0.0.1.
      * CentOS 8 8.3.2011 with rdma-core master (7f2d460).
      * OpenSUSE Leap 15.3 with rdma-core v31.0.

   * ConnectX-4 Lx

      * OS: Ubuntu 20.04 LTS
      * Driver: MLNX_OFED_LINUX-5.3-1.0.0.1
      * Firmware: 14.30.1004

   * ConnectX-5

      * OS: Ubuntu 20.04 LTS
      * Driver: MLNX_OFED_LINUX-5.3-1.0.0.1
      * Firmware: 16.30.1004

* Broadcom(R) Testing

   * Functionality

      * Tx/Rx
      * Link status
      * RSS
      * TSO
      * VLAN filtering
      * MAC filtering
      * statistics
      * Checksum offload
      * MTU
      * Promiscuous mode
      * Multicast

   * Platform

      * BCM57414 NetXtreme-E 10Gb/25Gb Ethernet Controller, Firmware: 219.0.88.0
      * BCM57508 NetXtreme-E 10Gb/25Gb/40Gb/50Gb/100Gb/200Gb Ethernet, Firmware : 220.0.0.100

20.11.2 Known Issues
~~~~~~~~~~~~~~~~~~~~

* Build

  * Clang build is failing in Fedora 35.
    https://bugs.dpdk.org/show_bug.cgi?id=745

* I40E/IXGBE

   * Flow director does not work.
     Fixed in 21.08.

* ICE

   * Packet can't be distributed to the same queue after reconfiguration.
     Fixed in 21.08.
   * The hash value remains unchanged when the SCTP port value changed.
     Fixed in 21.08 new feature.

20.11.3 Release Notes
---------------------

20.11.3 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: fix out-of-place mempool allocation
* app/test: fix IPv6 header initialization
* app/testpmd: change port link speed without stopping all
* app/testpmd: fix help string for port reset
* app/testpmd: fix IPv4 checksum
* app/testpmd: fix MAC address after port reset
* app/testpmd: fix offloads for newly attached port
* app/testpmd: fix Tx checksum calculation for tunnel
* app/testpmd: fix type of FEC mode parsing output
* bitmap: fix buffer overrun in bitmap init
* build: support drivers symlink on Windows
* bus: clarify log for non-NUMA-aware devices
* bus/dpaa: fix freeing in FMAN interface destructor
* bus/pci: fix IOVA as VA support for PowerNV
* bus/pci: fix leak for unbound devices
* common/mlx5: fix compatibility with OFED port query API
* common/mlx5: fix memory region leak
* common/mlx5: fix Netlink port name padding in probing
* common/mlx5: fix Netlink receive message buffer size
* common/mlx5: use new port query API if available
* crypto/aesni_gcm: fix performance on some AVX512 CPUs
* cryptodev: fix freeing after device release
* crypto/mvsam: fix AES-GCM session parameters
* crypto/mvsam: fix capabilities
* crypto/mvsam: fix options parsing
* crypto/mvsam: fix session data reset
* crypto/octeontx2: fix IPsec session member overlap
* crypto/octeontx2: fix lookaside IPsec IV pointer
* crypto/octeontx: fix freeing after device release
* crypto/qat: disable asymmetric crypto on GEN3
* crypto/qat: fix Arm build with special memcpy
* devtools: fix file listing in maintainers check
* distributor: fix 128-bit write alignment
* doc: add limitation for ConnectX-4 with L2 in mlx5 guide
* doc: fix build on Windows with Meson 0.58
* doc: fix default burst size in testpmd
* doc: fix spelling
* doc: fix typo in SPDX tag
* doc: remove old deprecation notice for sched
* doc: update atomic operation deprecation
* drivers/net: fix memzone allocations for DMA memory
* eal/windows: check callback parameter of alarm functions
* eal/windows: cleanup virt2phys handle
* ethdev: fix doc of flow action
* eventdev: fix event port setup in Tx adapter
* examples/l2fwd: fix [no-]mac-updating options
* flow_classify: fix leaking rules on delete
* graph: fix memory leak in stats
* graph: fix null dereference in stats
* ipc: stop mp control thread on cleanup
* kni: fix crash on userspace VA for segmented packets
* kni: fix mbuf allocation for kernel side use
* malloc: fix size annotation for NUMA-aware realloc
* mempool/octeontx2: fix shift calculation
* net/bnxt: check access to possible null pointer
* net/bnxt: cleanup code
* net/bnxt: clear cached statistics
* net/bnxt: detect bad opaque in Rx completion
* net/bnxt: fix aarch32 build
* net/bnxt: fix auto-negociation on Whitney+
* net/bnxt: fix check for PTP support in FW
* net/bnxt: fix error handling in VNIC prepare
* net/bnxt: fix error messages in VNIC prepare
* net/bnxt: fix missing barriers in completion handling
* net/bnxt: fix nested lock during bonding
* net/bnxt: fix null dereference in interrupt handler
* net/bnxt: fix ring allocation and free
* net/bnxt: fix ring and context memory allocation
* net/bnxt: fix Rx burst size constraint
* net/bnxt: fix Rx interrupt setting
* net/bnxt: fix scalar Tx completion handling
* net/bnxt: fix Tx descriptor status implementation
* net/bnxt: fix typo in log message
* net/bnxt: improve probing log message
* net/bnxt: invoke device removal event on recovery failure
* net/bnxt: remove unnecessary code
* net/bnxt: remove unnecessary comment
* net/bnxt: remove workaround for default VNIC
* net/bnxt: set flow error after tunnel redirection free
* net/bnxt: set flow error when free filter not available
* net/bnxt: use common function to free VNIC resource
* net/bnxt: workaround spurious zero stats in Thor
* net/bonding: check flow setting
* net/bonding: fix error message on flow verify
* net/dpaa: fix headroom in VSP case
* net/ena: enable multi-segment in Tx offload flags
* net/ena: trigger reset on Tx prepare failure
* net/hinic/base: fix LRO
* net/hinic: fix MTU consistency with firmware
* net/hinic: increase protection of the VLAN
* net/hns3: fix Arm SVE build with GCC 8.3
* net/hns3: fix delay for waiting to stop Rx/Tx
* net/hns3: fix fake queue rollback
* net/hns3: fix filter parsing comment
* net/hns3: fix flow rule list in multi-process
* net/hns3: fix maximum queues on configuration failure
* net/hns3: fix residual MAC address entry
* net/hns3: fix timing of clearing interrupt source
* net/hns3: fix Tx prepare after stop
* net/hns3: fix VLAN strip log
* net/hns3: increase VF reset retry maximum
* net/i40e: fix descriptor scan on Arm
* net/i40e: fix flow director input set conflict
* net/i40e: fix multi-process shared data
* net/i40e: fix raw packet flow director
* net/i40e: fix use after free in FDIR release
* net/iavf: fix handling of unsupported promiscuous
* net/iavf: fix RSS key access out of bound
* net/iavf: fix scalar Rx
* net/iavf: fix Tx threshold check
* net/ice: fix data path in secondary process
* net/ice: fix data path selection in secondary process
* net/ice: fix default RSS key generation
* net/ice: fix memzone leak when firmware is missing
* net/ice: fix overflow in maximum packet length config
* net/ixgbe: fix flow entry access after freeing
* net/memif: fix abstract socket address length
* net/mlx5: add Tx scheduling check on queue creation
* net/mlx5: export PMD-specific API file
* net/mlx5: fix default queue number in RSS flow rule
* net/mlx5: fix flow engine type in function name
* net/mlx5: fix imissed statistics
* net/mlx5: fix indirect action modify rollback
* net/mlx5: fix IPIP multi-tunnel validation
* net/mlx5: fix match MPLS over GRE with key
* net/mlx5: fix missing RSS expandable items
* net/mlx5: fix missing RSS expansion of IPv6 frag
* net/mlx5: fix MPLS RSS expansion
* net/mlx5: fix multi-segment inline for the first segments
* net/mlx5: fix overflow in mempool argument
* net/mlx5: fix pattern expansion in RSS flow rules
* net/mlx5: fix queue leaking in hairpin auto bind check
* net/mlx5: fix representor interrupt handler
* net/mlx5: fix RoCE LAG bond device probing
* net/mlx5: fix RSS expansion for GTP
* net/mlx5: fix RSS flow rule with L4 mismatch
* net/mlx5: fix RSS pattern expansion
* net/mlx5: fix r/w lock usage in DMA unmap
* net/mlx5: fix Rx/Tx queue checks
* net/mlx5: fix switchdev mode recognition
* net/mlx5: fix threshold for mbuf replenishment in MPRQ
* net/mlx5: fix timestamp initialization on empty clock queue
* net/mlx5: fix TSO multi-segment inline length
* net/mlx5: fix typo in vectorized Rx comments
* net/mlx5: reject inner ethernet matching in GTP
* net/mlx5: remove redundant operations in NEON Rx
* net/mlx5: remove unsupported flow item MPLS over IP
* net/mlx5: workaround drop action with old kernel
* net/mvpp2: fix configured state dependency
* net/mvpp2: fix port speed overflow
* net/octeontx2: fix default MCAM allocation size
* net/octeontx2: fix flow creation limit on CN98xx
* net/octeontx2: fix TM node statistics query
* net/octeontx2: use runtime LSO format indices
* net/octeontx/base: fix debug build with clang
* net/pfe: remove unnecessary null check
* net/sfc: check ID overflow in action port ID
* net/sfc: fix aarch32 build
* net/sfc: fix MAC stats lock in xstats query by ID
* net/sfc: fix MAC stats update for stopped device
* net/sfc: fix outer L4 checksum Rx
* net/sfc: fix outer match in MAE backend
* net/sfc: fix reading adapter state without locking
* net/sfc: fix xstats query by ID according to ethdev
* net/sfc: fix xstats query by unsorted list of IDs
* net/softnic: fix connection memory leak
* net/softnic: fix memory leak as profile is freed
* net/softnic: fix memory leak in arguments parsing
* net/softnic: fix null dereference in arguments parsing
* net/tap: fix Rx checksum flags on IP options packets
* net/tap: fix Rx checksum flags on TCP packets
* net/virtio: fix aarch32 build
* net/virtio: fix default duplex mode
* net/virtio: fix interrupt handle leak
* net/virtio: fix refill order in packed ring datapath
* net/virtio: fix Rx scatter offload
* net/virtio: report maximum MTU in device info
* raw/ioat: fix config script queue size calculation
* regex/mlx5: fix redundancy in device removal
* regex/mlx5: fix size of setup constants
* rib: fix max depth IPv6 lookup
* sched: fix profile allocation failure handling
* sched: rework configuration failure handling
* table: fix bucket empty check
* test/crypto: fix autotest function parameters
* test/crypto: fix mbuf reset after null check
* test/crypto: fix mempool size for session-less
* test/crypto: fix typo in AES case
* test/crypto: fix typo in ESN case
* test/mbuf: fix virtual address conversion
* test/power: fix CPU frequency check for intel_pstate
* test/power: fix CPU frequency when turbo enabled
* tests/cmdline: fix memory leaks
* tests/eal: fix memory leak
* vdpa/mlx5: fix overflow in queue attribute
* vdpa/mlx5: fix TSO offload without checksum
* version: 20.11.3-rc1
* vfio: add stdbool include
* vhost: check header for legacy dequeue offload
* vhost/crypto: check request pointer before dereference
* vhost: fix crash on reconnect
* vhost: fix lock on device readiness notification
* vhost: fix missing guest pages table NUMA realloc
* vhost: fix missing memory table NUMA realloc
* vhost: fix NUMA reallocation with multi-queue

20.11.3 Validation
~~~~~~~~~~~~~~~~~~

* Intel(R) Testing

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing
      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* Nvidia(R) Testing

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests

   * Build tests

      * Ubuntu 20.04.2 with MLNX_OFED_LINUX-5.4-1.0.3.0.
      * Ubuntu 20.04.2 with rdma-core master (64d1ae5).
      * Ubuntu 20.04.2 with rdma-core v28.0.
      * Ubuntu 18.04.5 with rdma-core v17.1.
      * Ubuntu 18.04.5 with rdma-core master (5b0f5b2) (i386).
      * Ubuntu 16.04.7 with rdma-core v22.7.
      * Fedora 34 with rdma-core v36.0.
      * Fedora 36 (Rawhide) with rdma-core v36.0 (only with gcc).
      * CentOS 7 7.9.2009 with rdma-core master (64d1ae5).
      * CentOS 7 7.9.2009 with MLNX_OFED_LINUX-5.4-1.0.3.0.
      * CentOS 8 8.3.2011 with rdma-core master (64d1ae5).
      * OpenSUSE Leap 15.3 with rdma-core v31.0.

   * ConnectX-5

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-5.4-1.0.3.0
      * Kernel: 5.14.0-rc6 / Driver: rdma-core v36.0
      * fw 16.31.1014

   * ConnectX-4 Lx

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-5.4-1.0.3.0
      * Kernel: 5.14.0-rc6 / Driver: rdma-core v36.0
      * fw 14.31.1014


* Red Hat(R) Testing

   * Platform

      * RHEL 8
      * Kernel 4.18
      * Qemu 6.0
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
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing

* Canonical(R) Testing

   * Build tests of DPDK & OVS 2.15.0 on Ubuntu 21.04 (meson based)
   * Functional and performance tests based on OVS-DPDK on x86_64
   * Autopkgtests for DPDK and OpenvSwitch

20.11.3 Known Issues
~~~~~~~~~~~~~~~~~~~~

* ICE

   * creating 512 acl rules after creating a full mask switch rule fails.

* vhost/virtio

   * udp-fragmentation-offload cannot be setup on Ubuntu 19.10 VMs.
     https://bugzilla.kernel.org/show_bug.cgi?id=207075
   * vm2vm virtio-net connectivity between two vms randomly fails due
     to lost connection after vhost reconnect.

* unit tests

   * unit_tests_power/power_cpufreq fails.
     https://bugs.dpdk.org/show_bug.cgi?id=790

* IAVF

   * cvl_advanced_iavf_rss: after changing the SCTP port value, the hash value
     remains unchanged.
