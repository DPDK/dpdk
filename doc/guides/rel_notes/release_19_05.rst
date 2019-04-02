..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 The DPDK contributors

DPDK Release 19.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_19_05.html


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

* **Updated KNI module and PMD.**

  Updated the KNI kernel module to set the max_mtu according to the given
  initial MTU size. Without it, the maximum MTU was 1500.

  Updated the KNI PMD driver to set the mbuf_size and MTU based on
  the given mb-pool. This provide the ability to pass jumbo frames
  if the mb-pool contains suitable buffers' size.

* **Updated Solarflare network PMD.**

  Updated the sfc_efx driver including the following changes:

  * Added support for Rx descriptor status and related API in a secondary
    process.
  * Added support for Tx descriptor status API in a secondary process.
  * Added support for RSS RETA and hash configuration get API in a secondary
    process.
  * Added support for Rx packet types list in a secondary process.

* **Updated Mellanox drivers.**

   New features and improvements were done in mlx4 and mlx5 PMDs:

   * Added firmware version reading.

* **Renamed avf to iavf.**

  Renamed Intel Ethernet Adaptive Virtual Function driver ``avf`` to ``iavf``,
  which includes the directory name, lib name, filenames, makefile, docs,
  macros, functions, structs and any other strings in the code.

* **Updated the enic driver.**

  * Fixed several flow (director) bugs related to MARK, SCTP, VLAN, VXLAN, and
    inner packet matching.
  * Added limited support for RAW.
  * Added limited support for RSS.
  * Added limited support for PASSTHRU.

* **Updated the ixgbe driver.**

  New features for VF:

  * Added promiscuous mode support.

* **Updated the ice driver.**

  * Added support of SSE and AVX2 instructions in Rx and Tx paths.
  * Added package download support.
  * Added Safe Mode support.
  * Supported RSS for UPD/TCP/SCTP+IPV4/IPV6 packets.

* **Updated the QuickAssist Technology PMD.**

  Added support for AES-XTS with 128 and 256 bit AES keys.

* **Updated AESNI-MB PMD.**

  Added support for out-of-place operations.

* **Updated the IPsec library.**

  The IPsec library has been updated with AES-CTR and 3DES-CBC cipher algorithms
  support. The related ipsec-secgw test scripts have been added.

* **Updated the testpmd application.**

  Improved testpmd application performance on ARM platform. For ``macswap``
  forwarding mode, NEON intrinsics were used to do swap to save CPU cycles.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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

* eal: the type of the ``attr_value`` parameter of the function
  ``rte_service_attr_get()`` has been changed
  from ``uint32_t *`` to ``uint64_t *``.

* vfio: Functions ``rte_vfio_container_dma_map`` and
  ``rte_vfio_container_dma_unmap`` have been extended with an option to
  request mapping or un-mapping to the default vfio container fd.


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

* ethdev: Additional fields in rte_eth_dev_info.

  The ``rte_eth_dev_info`` structure has had two extra fields
  added: ``min_mtu`` and ``max_mtu``. Each of these are of type ``uint16_t``.
  The values of these fields can be set specifically by the PMD drivers as
  supported values can vary from device to device.

* cryptodev: in 18.08 new structure ``rte_crypto_asym_op`` was introduced and
  included into ``rte_crypto_op``. As ``rte_crypto_asym_op`` structure was
  defined as cache-line aligned that caused unintended changes in
  ``rte_crypto_op`` structure layout and alignment. Remove cache-line
  alignment for ``rte_crypto_asym_op`` to restore expected ``rte_crypto_op``
  layout and alignment.


Shared Library Versions
-----------------------

.. Update any library version updated in this release
   and prepend with a ``+`` sign, like this:

     libfoo.so.1
   + libupdated.so.2
     libbar.so.1

   This section is a comment. Do not overwrite or remove it.
   =========================================================

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bpf.so.1
     librte_bus_dpaa.so.2
     librte_bus_fslmc.so.2
     librte_bus_ifpga.so.2
     librte_bus_pci.so.2
     librte_bus_vdev.so.2
     librte_bus_vmbus.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_compressdev.so.1
   + librte_cryptodev.so.7
     librte_distributor.so.1
   + librte_eal.so.10
     librte_efd.so.1
   + librte_ethdev.so.12
     librte_eventdev.so.6
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ipsec.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.5
     librte_member.so.1
     librte_mempool.so.5
     librte_meter.so.2
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.3
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
     librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.2
     librte_sched.so.2
     librte_security.so.2
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **No software AES-XTS implementation.**

  There are currently no cryptodev software PMDs available which implement
  support for the AES-XTS algorithm, so this feature can only be used
  if compatible hardware and an associated PMD is available.


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
