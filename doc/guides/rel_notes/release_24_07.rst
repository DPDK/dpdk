.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.07
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_24_07.html


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

* **Introduced pointer compression library.**

  Library provides functions to compress and decompress arrays of pointers
  which can improve application performance under certain conditions.
  Performance test was added to help users evaluate performance on their setup.

* **Added API to retrieve memory locations of objects in a mempool.**

  Added mempool API ``rte_mempool_get_mem_range`` and
  ``rte_mempool_get_obj_alignment`` to retrieve information about the memory
  range and the alignment of objects stored in a mempool.

* **Updated AF_XDP driver.**

  * Enabled multi-interface (UDS) support with AF_XDP Device Plugin.

    The argument ``use_cni`` was limiting a pod to a single netdev/interface.
    The new ``dp_path`` parameter removed this limitation
    and maintains backward compatibility for applications using the ``use_cni``
    vdev argument with the AF_XDP Device Plugin.

  * Integrated AF_XDP Device Plugin eBPF map pinning support.

    The argument ``use_map_pinning`` was added to allow Kubernetes Pods
    to use AF_XDP with DPDK, and run with limited privileges,
    without having to do a full handshake over a Unix Domain Socket
    with the Device Plugin.

* **Updated Amazon ena (Elastic Network Adapter) driver.**

  * Reworked the driver logger usage in order to improve Tx performance.
  * Reworked the device uninitialization flow to ensure complete resource cleanup
    and lay the groundwork for hot-unplug support.

* **Updated Intel ice driver.**

  * Added support of E830 device family.
  * Added support for configuring the Forward Error Correction (FEC) mode,
    querying FEC capabilities and current FEC mode from a device.

* **Updated Intel i40e driver.**

  * Added support for configuring the Forward Error Correction (FEC) mode,
    querying FEC capabilities and current FEC mode from a device.

* **Updated Intel ixgbe driver.**

  * Updated base code with E610 device family support.

* **Added Napatech ntnic net driver [EXPERIMENTAL].**

  * Added the PMD driver for Napatech smartNIC

    - Ability to initialize the NIC (NT200A02)
    - Supporting only one FPGA firmware (9563.55.39)
    - Ability to bring up the 100G link
    - Supporting QSFP/QSFP+/QSFP28 NIM
    - Does not support datapath

* **Updated Marvell cnxk net driver.**

  * Added support disabling custom meta aura
    and separately use custom SA action support.
  * Added MTU update for port representor.
  * Added multi-segment support for port representor.

* **Updated NVIDIA mlx5 driver.**

  * Added match with Tx queue.
  * Added match with external Tx queue.
  * Added match with E-Switch manager.
  * Added async flow item and actions validation.
  * Added global and per-port out of buffer counter for hairpin queues.
  * Added hardware queue object context dump for Rx/Tx debugging.

* **Updated TAP driver.**

  * Updated to support up to 8 queues when used by secondary process.

  * Fixed support of RSS flow action to work with current Linux kernels
    and BPF tooling.
    Will only be enabled if clang, libbpf 1.0 and bpftool are available.

* **Updated Wangxun ngbe driver.**

  * Added SSE/NEON vector datapath.

* **Updated Wangxun txgbe driver.**

  * Added SSE/NEON vector datapath.

* **Added AMD Pensando ionic crypto driver.**

  Added a new crypto driver for AMD Pensando hardware accelerators.

* **Updated NVIDIA mlx5 crypto driver.**

  * Added AES-GCM IPsec operation optimization.

* **Updated IPsec_MB crypto driver.**

  * Made Kasumi and ChaCha-Poly PMDs to share the job code path
    with AESNI_MB PMD.

* **Added UADK compress driver.**

  Added a new compress driver for the UADK library. See the
  :doc:`../compressdevs/uadk` guide for more details on this new driver.

* **Updated Marvell CNXK DMA driver.**

  * Updated DMA driver internal pool to use higher chunk size,
    effectively reducing the number of mempool allocs needed,
    thereby increasing DMA performance.

* **Added Marvell Odyssey ODM DMA driver.**

  * Added Marvell Odyssey ODM DMA device PMD.

* **Updated the DSW event device.**

  * Added support for ``RTE_EVENT_DEV_CAP_IMPLICIT_RELEASE_DISABLE``,
    allowing applications to take on new tasks without having completed
    (released) the previous event batch. This in turn facilities DSW
    use alongside high-latency look-aside hardware accelerators.

* **Updated the hash library.**

  * Added defer queue reclamation via RCU.
  * Added SVE support for bulk lookup.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* **Disabled the BPF library and net/af_xdp for 32-bit x86.**

  BPF is not supported and the librte-bpf test fails on 32-bit x86 kernels.
  So disable the library and the pmd.

* **Removed hisilicon DMA support for HIP09 platform.**

  The DMA for HIP09 is no longer available,
  so the support is removed from hisilicon driver for HIP09 platform.


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

* mbuf: ``RTE_MARKER`` fields ``cacheline0`` and ``cacheline1``
  have been removed from ``struct rte_mbuf``.

* hash: The ``rte_hash_sig_compare_function`` internal enum is not exposed
  in the public API anymore.


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

* No ABI change that would break compatibility with 23.11.

* eventdev/dma: Reorganize the experimental fastpath structure ``rte_event_dma_adapter_op``
  to optimize the memory layout and improve performance.


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
