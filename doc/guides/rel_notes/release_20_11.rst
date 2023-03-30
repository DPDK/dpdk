.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.11
==================

New Features
------------

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

  Added a new PMD for Wangxun 10 Gigabit Ethernet NICs.

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

  Added a new PMD for the hardware regex offload block for OCTEON TX2 SoC.

  See ``regexdevs/octeontx2`` for more details.

* **Updated Software Eventdev driver.**

  Added performance tuning arguments to allow tuning the scheduler for
  better throughput in high core count use cases.

* **Added a new driver for the Intel Dynamic Load Balancer v1.0 device.**

  Added the new ``dlb`` eventdev driver for the Intel DLB V1.0 device.

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

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT (2x25G)

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
