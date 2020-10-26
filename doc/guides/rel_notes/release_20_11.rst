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
  The functions are provided as a generic stubs and
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
  current FEC mode from device. Also, API for configuring FEC mode is also provided.

* **Added thread safety to rte_flow functions.**

  Added ``RTE_ETH_DEV_FLOW_OPS_THREAD_SAFE`` device flag to indicate
  whether PMD supports thread safe operations. If PMD doesn't set the flag,
  rte_flow API level functions will protect the flow operations with mutex.

* **Added flow-based traffic sampling support.**

  Added new action: ``RTE_FLOW_ACTION_TYPE_SAMPLE`` to duplicate the matching
  packets with specified ratio, and apply with own set of actions with a fate
  action. When the ratio is set to 1 then the packets will be 100% mirrored.

* **Added support of shared action in flow API.**

  Added shared action support to utilize single flow action in multiple flow
  rules. An update of shared action configuration alters the behavior of all
  flow rules using it.

  * Added new action: ``RTE_FLOW_ACTION_TYPE_SHARED`` to use shared action
    as flow action.
  * Added new flow APIs to create/update/destroy/query shared action.

* **Flow rules allowed to use private PMD items / actions.**

  * Flow rule verification was updated to accept private PMD
    items and actions.

* **Added generic API to offload tunneled traffic and restore missed packet.**

  * Added a new hardware independent helper to flow API that
    offloads tunneled traffic and restores missed packets.

* **Updated the ethdev library to support hairpin between two ports.**

  New APIs are introduced to support binding / unbinding 2 ports hairpin.
  Hairpin Tx part flow rules can be inserted explicitly.
  New API is added to get the hairpin peer ports list.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with new features and improvements, including:

  * Added support for 200G PAM4 link speed.
  * Added support for RSS hash level selection.
  * Updated HWRM structures to 1.10.1.70 version.
  * Added TRUFLOW support for Stingray devices.

* **Updated Cisco enic driver.**

  * Added support for VF representors with single-queue Tx/Rx and flow API
  * Added support for egress PORT_ID action
  * Added support for non-zero priorities for group 0 flows
  * Added support for VXLAN decap combined with VLAN pop

* **Added hns3 FEC PMD, for supporting query and config FEC mode.**

  Added the FEC PMD which provides functions for query FEC capabilities and
  current FEC mode from device. Also, PMD for configuring FEC mode is also provided.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added SR-IOV PF support
  * Added Alveo SN1000 SmartNICs (EF100 architecture) support

* **Updated Virtio driver.**

  * Added support for Vhost-vDPA backend to Virtio-user PMD.
  * Changed default link speed to unknown.
  * Added support for 200G link speed.

* **Updated Intel i40e driver.**

  Updated the Intel i40e driver to use write combining stores.

* **Updated Intel ixgbe driver.**

  Updated the Intel ixgbe driver to use write combining stores.

* **Updated Intel ice driver.**

  Updated the Intel ice driver to use write combining stores.

* **Updated Intel qat driver.**

  Updated the Intel qat driver to use write combining stores.

* **Updated Memif PMD.**

  * Added support for abstract socket address.
  * Changed default socket address type to abstract.

* **Added Ice Lake (Gen4) support for Intel NTB.**

  Added NTB device support (4th generation) for Intel Ice Lake platform.

* **Added UDP/IPv4 GRO support for VxLAN and non-VxLAN packets.**

  For VxLAN packets, added inner UDP/IPv4 support.
  For non-VxLAN packets, added UDP/IPv4 support.

* **Extended flow-perf application.**

  * Started supporting user order instead of bit mask:
    Now the user can create any structure of rte_flow
    using flow performance application with any order,
    moreover the app also now starts to support inner
    items matching as well.
  * Added header modify actions.
  * Added flag action.
  * Added raw encap/decap actions.
  * Added VXLAN encap/decap actions.
  * Added ICMP(code/type/identifier/sequence number) and ICMP6(code/type) matching items.
  * Added option to set port mask for insertion/deletion:
    ``--portmask=N``
    where N represents the hexadecimal bitmask of ports used.

* **Added raw data-path APIs for cryptodev library.**

  Cryptodev is added with raw data-path APIs to accelerate external
  libraries or applications which need to avail fast cryptodev
  enqueue/dequeue operations but does not necessarily depends on
  mbufs and cryptodev operation mempools.

* **Updated the aesni_mb crypto PMD.**

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

* **Updated DPAA2_SEC crypto PMD.**

  * Added DES-CBC support for cipher_only, chain and ipsec protocol.
  * Added support for non-HMAC auth algorithms
    (MD5, SHA1, SHA224, SHA256, SHA384, SHA512).

* **Updated Marvell NITROX symmetric crypto PMD.**

  * Added AES-GCM support.
  * Added cipher only offload support.

* **Updated the OCTEON TX2 crypto PMD.**

  * Updated the OCTEON TX2 crypto PMD lookaside protocol offload for IPsec with
    IPv6 support.

* **Updated QAT crypto PMD.**

  * Added Raw Data-path APIs support.

* **Added Intel ACC100 bbdev PMD.**

  Added a new ``acc100`` bbdev driver for the Intel\ |reg| ACC100 accelerator
  also known as Mount Bryce.  See the
  :doc:`../bbdevs/acc100` BBDEV guide for more details on this new driver.

* **Updated rte_security library to support SDAP.**

  ``rte_security_pdcp_xform`` in ``rte_security`` lib is updated to enable
  5G NR processing of SDAP header in PMDs.

* **Added Marvell OCTEON TX2 regex PMD.**

  Added a new PMD driver for hardware regex offload block for OCTEON TX2 SoC.

  See the :doc:`../regexdevs/octeontx2` for more details.

* **Updated Software Eventdev driver.**

  Added performance tuning arguments to allow tuning the scheduler for
  better throughtput in high core count use cases.

* **Added a new driver for the Intel Dynamic Load Balancer v1.0 device.**

  Added the new ``dlb`` eventdev driver for the Intel DLB V1.0 device. See the
  :doc:`../eventdevs/dlb` eventdev guide for more details on this new driver.

* **Added a new driver for the Intel Dynamic Load Balancer v2.0 device.**

  Added the new ``dlb2`` eventdev driver for the Intel DLB V2.0 device. See the
  :doc:`../eventdevs/dlb2` eventdev guide for more details on this new driver.

* **Updated ioat rawdev driver**

  The ioat rawdev driver has been updated and enhanced. Changes include:

  * Added support for Intel\ |reg| Data Streaming Accelerator hardware.
    For more information, see https://01.org/blogs/2019/introducing-intel-data-streaming-accelerator
  * Added support for the fill operation via the API ``rte_ioat_enqueue_fill()``,
    where the hardware fills an area of memory with a repeating pattern.
  * Added a per-device configuration flag to disable management
    of user-provided completion handles.
  * Renamed the ``rte_ioat_do_copies()`` API to ``rte_ioat_perform_ops()``,
    and renamed the ``rte_ioat_completed_copies()`` API to ``rte_ioat_completed_ops()``
    to better reflect the APIs' purposes, and remove the implication that
    they are limited to copy operations only.
    [Note: The old API is still provided but marked as deprecated in the code]
  * Added a new API ``rte_ioat_fence()`` to add a fence between operations.
    This API replaces the ``fence`` flag parameter in the ``rte_ioat_enqueue_copies()`` function,
    and is clearer as there is no ambiguity as to whether the flag should be
    set on the last operation before the fence or the first operation after it.

* **Updated the pipeline library for alignment with the P4 language.**

  Added new Software Switch (SWX) pipeline type that provides more
  flexibility through API and feature alignment with the P4 language.

  * The packet headers, meta-data, actions, tables and pipelines are
    dynamically defined instead of selected from pre-defined set.
  * The actions and the pipeline are defined with instructions.
  * Extern objects and functions can be plugged into the pipeline.
  * Transaction-oriented table updates.

* **Add new AVX512 specific classify algorithms for ACL library.**

  * Added new ``RTE_ACL_CLASSIFY_AVX512X16`` vector implementation,
    which can process up to 16 flows in parallel. Requires AVX512 support.

  * Added new ``RTE_ACL_CLASSIFY_AVX512X32`` vector implementation,
    which can process up to 32 flows in parallel. Requires AVX512 support.

* **Added AVX512 lookup implementation for FIB.**

  Added a AVX512 lookup functions implementation into FIB and FIB6 libraries.

* **Added support to update subport bandwidth dynamically.**

   * Added new API ``rte_sched_port_subport_profile_add`` to add new
     subport bandwidth profile to subport porfile table at runtime.

   * Added support to update subport rate dynamically.

* **Updated FIPS validation sample application.**

  * Added scatter gather support.
  * Added NIST GCMVS complaint GMAC test method support.

* **Updated l3wfd-acl sample application.**

  * Added new optional parameter ``--eth-dest`` for the ``l3fwd-acl`` to allow
    the user to specify the destination mac address for each ethernet port
    used.
  * Replaced ``--scalar`` command-line option with ``--alg=<value>``, to allow
    the user to select the desired classify method.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* build: Support for the Make build system was removed for compiling DPDK,
  Meson is now the primary build system.
  Sample applications can still be built with Make standalone, using pkg-config.

* vhost: Dequeue zero-copy support has been removed.

* kernel: The module ``igb_uio`` has been moved to the git repository
  ``dpdk-kmods`` in a new directory ``linux/igb_uio``.

* Removed Python 2 support since it was EOL'd in January 2020.

* Removed TEP termination sample application.


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

* build macros: The macros defining ``RTE_MACHINE_CPUFLAG_*`` are removed.
  The information provided by these macros is available through standard
  compiler macros.

* eal: Replaced the function ``rte_get_master_lcore()`` to
  ``rte_get_main_lcore()``. The old function is deprecated.

  The iterator for worker lcores is also changed:
  ``RTE_LCORE_FOREACH_SLAVE`` is replaced with
  ``RTE_LCORE_FOREACH_WORKER``.

* eal: The ``rte_logs`` struct and global symbol was made private
  and is no longer part of the API.

* eal: Made the ``rte_dev_event`` structure private to the EAL as no public API
  used it.

* eal: ``rte_cio_rmb()`` and ``rte_cio_wmb()`` were deprecated since 20.08
  and are removed in this release.

* mem: Removed the unioned field ``phys_addr`` from
  the structures ``rte_memseg`` and ``rte_memzone``.
  The field ``iova`` is remaining from the old unions.

* mempool: Removed the unioned fields ``phys_addr`` and ``physaddr`` from
  the structures ``rte_mempool_memhdr`` and ``rte_mempool_objhdr``.
  The field ``iova`` is remaining from the old unions.
  The flag name ``MEMPOOL_F_NO_PHYS_CONTIG`` is removed,
  while the aliased flag ``MEMPOOL_F_NO_IOVA_CONTIG`` is kept.

* mbuf: Removed the functions ``rte_mbuf_data_dma_addr*``
  and the macros ``rte_pktmbuf_mtophys*``.
  The same functionality is still available with the functions and macros
  having ``iova`` in their names instead of ``dma_addr`` or ``mtophys``.

* mbuf: Removed the unioned field ``buf_physaddr`` from ``rte_mbuf``.
  The field ``buf_iova`` is remaining from the old union.

* mbuf: Removed the unioned field ``refcnt_atomic`` from
  the structures ``rte_mbuf`` and ``rte_mbuf_ext_shared_info``.
  The field ``refcnt`` is remaining from the old unions.

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
  As the data of ``uint8_t`` will be truncated when queue number under
  a TC is greater than 256.

* vhost: Moved vDPA APIs from experimental to stable.

* vhost: Add a new function ``rte_vhost_crypto_driver_start`` to be called
  instead of ``rte_vhost_driver_start`` by crypto applications.

* cryptodev: The structure ``rte_crypto_sym_vec`` is updated to support both
  cpu_crypto synchrounous operation and asynchronous raw data-path APIs.

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

* security: ``hfn_ovrd`` field in ``rte_security_pdcp_xform`` is changed from
  ``uint32_t`` to ``uint8_t`` so that a new field ``sdap_enabled`` can be added
  to support SDAP.

* security: The API ``rte_security_session_create`` is updated to take two
  mempool objects one for session and other for session private data.
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
  This enum value was not used inside DPDK, while it prevented to add new
  classify algorithms without causing an ABI breakage.

* sched: Added ``subport_profile_id`` as argument
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

* eal: Removed the not implemented function ``rte_dump_registers()``.

* ``ethdev`` changes

  * Following device operation function pointers moved
    from ``struct eth_dev_ops`` to ``struct rte_eth_dev``:

    * ``eth_rx_queue_count_t       rx_queue_count;``
    * ``eth_rx_descriptor_done_t   rx_descriptor_done;``
    * ``eth_rx_descriptor_status_t rx_descriptor_status;``
    * ``eth_tx_descriptor_status_t tx_descriptor_status;``

  * ``struct eth_dev_ops`` is no more accessible by applications,
    which was already internal data structure.

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

  * Added new field ``has_more_vlan`` to structure
    ``rte_flow_item_vlan``, indicating that packet header contains
    at least one more VLAN, after this VLAN.

* eventdev: Following structures are modified to support DLB/DLB2 PMDs
  and future extensions:

  * ``rte_event_dev_info``
  * ``rte_event_dev_config``
  * ``rte_event_port_conf``

* sched: Added new fields to ``struct rte_sched_subport_port_params``.

* lpm: Removed fields other than ``tbl24`` and ``tbl8`` from the struct
  ``rte_lpm``. The removed fields were made internal.


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
