.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.08
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_20_08.html


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
     =========================================================

* **Added non-EAL threads registration API.**

  Added a new API to register non-EAL threads as lcores. This can be used by
  applications to have its threads known of DPDK without suffering from the
  non-EAL previous limitations in terms of performance.

* **rte_*mb APIs are updated to use DMB instruction for ARMv8.**

  ARMv8 memory model has been strengthened to require other-multi-copy
  atomicity. This allows for using DMB instruction instead of DSB for IO
  barriers. rte_*mb APIs, for ARMv8 platforms, are changed to use DMB
  instruction to reflect this.

* **Added the support for vfio-pci new VF token interface.**

  From Linux 5.7, vfio-pci supports to bind both SR-IOV PF and the created VFs,
  it uses a shared VF token (UUID) to represent the collaboration between PF
  and VFs. Update DPDK PCI driver to gain the access to the PF and VFs devices
  by appending the VF token parameter.

* **Added the RegEx Library, a generic RegEx service library.**

  Added the RegEx library which provides an API for offload of regular
  expressions search operations to hardware or software accelerator devices.

* **Added eCPRI protocol support in rte_flow.**

  The ``ECPRI`` item has been added to support eCPRI packet offloading for
  5G network.

* **Introduced send packet scheduling on the timestamps.**

   Added the new mbuf dynamic field and flag to provide timestamp on what packet
   transmitting can be synchronized. The device Tx offload flag is added to
   indicate the PMD supports send scheduling.

* **Updated PCAP driver.**

  Updated PCAP driver with new features and improvements, including:

  * Support software Tx nanosecond timestamps precision.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with new features and improvements, including:

  * Added support for VF representors.
  * Added support for multiple devices.
  * Added support for new resource manager API.
  * Added support for VXLAN encap/decap.
  * Added support for rte_flow_query for COUNT action.
  * Added support for rx_burst_mode_get and tx_burst_mode_get.
  * Added vector mode support for ARM CPUs.
  * Added support for VLAN push and pop actions.
  * Added support for NAT action items.

* **Updated Cisco enic driver.**

  * Added support for VLAN push and pop flow actions.

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added new PMD devarg ``reclaim_mem_mode``.
  * Added new devarg ``lacp_by_user``.

* **Added vDPA device APIs to query virtio queue statistics.**

     A new 3 APIs has been added to query virtio queue statistics, to get their
     names and to reset them by a vDPA device.

* **Updated Mellanox mlx5 vDPA driver.**

  Updated Mellanox mlx5 vDPA driver with new features, including:

  * Added support for virtio queue statistics.
  * Added support for MTU update.

* **Updated Marvell octeontx2 ethdev PMD.**

  Updated Marvell octeontx2 driver with cn98xx support.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for DCF datapath configuration.
  * Added support for more PPPoE packet type for switch filter.

* **Updated Intel i40e driver.**

  Updated i40e PMD with new features and improvements, including:

  * Supported cloud filter for IPv4/6_TCP/UDP/SCTP with SRC port only or DST port only.
  * Re-implemented get_fdir_info and get_fdir_stat in private API.
  * Re-implemented set_gre_key_len in private API.
  * Added support for flow query RSS.

* **Updated the Intel ixgbe driver.**

  Updated the Intel ixgbe driver with new features and improvements, including:

  * Re-implemented get_fdir_info and get_fdir_stat in private API.

* **Updated NXP dpaa ethdev PMD.**

  Updated the NXP dpaa ethdev with new features and improvements, including:

  * Added support for link status and interrupt
  * Added support to use datapath APIs from non-EAL pthread

* **Updated NXP dpaa2 ethdev PMD.**

  Updated the NXP dpaa2 ethdev with new features and improvements, including:

  * Added support to use datapath APIs from non-EAL pthread
  * Added support for dynamic flow management

* **Added DOCSIS protocol to rte_security.**

  Added support for combined crypto and CRC operations for the DOCSIS protocol
  to ``rte_security`` API.

* **Updated the AESNI MB crypto PMD.**

  Added support for lookaside protocol offload for DOCSIS through the
  ``rte_security`` API.

* **Updated the QuickAssist Technology (QAT) PMD.**

  * Added support for lookaside protocol offload for DOCSIS through the
    ``rte_security`` API.
  * Added Chacha20-Poly1305 AEAD algorithm.
  * Improved handling of multi process in QAT crypto and compression PMDs.

* **Updated the OCTEON TX2 crypto PMD.**

  Added Chacha20-Poly1305 AEAD algorithm support in OCTEON TX2 crypto PMD.

* **Added support for BPF_ABS/BPF_IND load instructions.**

  Added support for two BPF non-generic instructions:
  ``(BPF_ABS | <size> | BPF_LD)`` and ``(BPF_IND | <size> | BPF_LD)``
  which are used to access packet data in a safe manner. Currently JIT support
  for these instructions is implemented for x86 only.

* **Added new testpmd forward mode.**

  Added new ``5tswap`` forward mode to testpmd.
  the  ``5tswap`` swaps source and destination in layers 2,3,4
  for ipv4 and ipv6 in L3 and UDP and TCP in L4.

* **Added flow performance test application.**

  Added new application to test ``rte_flow`` performance, including:

  * Measure ``rte_flow`` insertion rate.
  * Measure ``rte_flow`` deletion rate.
  * Dump ``rte_flow`` memory consumption.
  * Measure packet per second forwarding.

* **Added --portmap command line parameter to l2fwd example.**

  Added new command line option ``--portmap="(port, port)[,(port, port)]"`` to
  pass forwarding port details.
  See the :doc:`../sample_app_ug/l2_forward_real_virtual` for more
  details of this parameter usage.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Removed ``RTE_KDRV_NONE`` based PCI device driver probing.


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
   =========================================================

* ``rte_page_sizes`` enumeration is replaced with ``RTE_PGSIZE_xxx`` defines.

* vhost: The API of ``rte_vhost_host_notifier_ctrl`` was changed to be per
  queue and not per device, a qid parameter was added to the arguments list.


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
   =========================================================

* No ABI change that would break compatibility with 19.11.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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
   =========================================================
