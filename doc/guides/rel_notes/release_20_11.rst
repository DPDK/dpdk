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

* **Updated CRC modules of the net library.**

  * Added runtime selection of the optimal architecture-specific CRC path.
  * Added optimized implementations of CRC32-Ethernet and CRC16-CCITT
    using the AVX512 and VPCLMULQDQ instruction sets.

* **Added the FEC API, for a generic FEC query and config.**

  Added the FEC API which provides functions for query FEC capabilities and
  current FEC mode from device. Also, API for configuring FEC mode is also provided.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with new features and improvements, including:

  * Added support for 200G PAM4 link speed.
  * Added support for RSS hash level selection.
  * Updated HWRM structures to 1.10.1.70 version.

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
  * Added ICMP and ICMP6 matching items.
  * Added option to set port mask for insertion/deletion:
    ``--portmask=N``
    where N represents the hexadecimal bitmask of ports used.

* **Added support for AES-ECB in aesni_mb crypto PMD.**

  * Added support for AES-ECB 128, 192 and 256 in aesni_mb PMD.

* **Updated the OCTEON TX2 crypto PMD.**

  * Updated the OCTEON TX2 crypto PMD lookaside protocol offload for IPsec with
    IPv6 support.

* **Added Intel ACC100 bbdev PMD.**

  Added a new ``acc100`` bbdev driver for the Intel\ |reg| ACC100 accelerator
  also known as Mount Bryce.  See the
  :doc:`../bbdevs/acc100` BBDEV guide for more details on this new driver.

* **Added Marvell OCTEON TX2 regex PMD.**

  Added a new PMD driver for hardware regex offload block for OCTEON TX2 SoC.

  See the :doc:`../regexdevs/octeontx2` for more details.

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

* **Added support to update subport bandwidth dynamically.**

   * Added new API ``rte_sched_port_subport_profile_add`` to add new
     subport bandwidth profile to subport porfile table at runtime.

   * Added support to update subport rate dynamically.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* vhost: Dequeue zero-copy support has been removed.

* kernel: The module ``igb_uio`` has been moved to the git repository
  ``dpdk-kmods`` in a new directory ``linux/igb_uio``.

* Removed Python 2 support since it was EOL'd in January 2020.

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

* Renamed internal ethdev APIs:

  * ``_rte_eth_dev_callback_process()`` -> ``rte_eth_dev_callback_process()``
  * ``_rte_eth_dev_reset`` -> ``rte_eth_dev_internal_reset()``

* ethdev: Modified field type of ``base`` and ``nb_queue`` in struct
  ``rte_eth_dcb_tc_queue_mapping`` from ``uint8_t`` to ``uint16_t``.
  As the data of ``uint8_t`` will be truncated when queue number under
  a TC is greater than 256.

* vhost: Moved vDPA APIs from experimental to stable.

* scheduler: Renamed functions ``rte_cryptodev_scheduler_slave_attach``,
  ``rte_cryptodev_scheduler_slave_detach`` and
  ``rte_cryptodev_scheduler_slaves_get`` to
  ``rte_cryptodev_scheduler_worker_attach``,
  ``rte_cryptodev_scheduler_worker_detach`` and
  ``rte_cryptodev_scheduler_workers_get`` accordingly.

* scheduler: Renamed the configuration value
  ``RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES`` to
  ``RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS``.

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

* sched: Added new fields to ``struct rte_sched_subport_port_params``.


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
