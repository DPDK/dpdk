.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.03
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_25_03.html


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

* **Added Staged-Ordered-Ring (SORING) API to the ring library.**

  New API was added to the ring library to provide a SW abstraction
  for ordered queues with multiple processing stages.
  It is based on conventional DPDK rte_ring, re-uses many of its concepts,
  and even substantial part of its code.
  It can be viewed as an extension of rte_ring functionality.

* **Hardened of more allocation functions.**

  Added allocation attributes to functions that allocate data:

  * ``rte_acl_create()``
  * ``rte_comp_op_pool_create()``
  * ``rte_event_ring_create()``
  * ``rte_fbk_hash_create()``
  * ``rte_fib_create()``
  * ``rte_fib6_create()``
  * ``rte_hash_create()``
  * ``rte_lpm_create()``
  * ``rte_lpm6_create()``
  * ``rte_member_create()``
  * ``rte_mempool_create()``
  * ``rte_mempool_create_empty()``
  * ``rte_reorder_create()``
  * ``rte_rib_create()``
  * ``rte_rib6_create()``
  * ``rte_ring_create()``
  * ``rte_sched_port_config()``
  * ``rte_stats_bitrate_create()``
  * ``rte_tel_data_alloc()``

  This can catch some obvious bugs at compile time (with GCC 11.0 or later).
  For example, calling ``free`` on a pointer that was allocated with one
  of those functions (and vice versa); freeing the same pointer twice
  in the same routine or freeing an object that was not created by allocation.

* **Updated af_packet net driver.**

  * Added ability to option to configure receive packet fanout mode.
  * Added statistics for failed buffer allocation and missed packets.

* **Updated Amazon ENA (Elastic Network Adapter) net driver.**

  * Added support for mutable RSS table size based on device capabilities.

* **Updated AMD axgbe driver.**

  * Added support for the TCP Segmentation Offload (TSO).

* **Updated Intel e1000 driver.**

  * Added support for the Intel i225-series NICs (previously handled by net/igc).
  * Updated base code to the latest version.

* **Updated Intel ipdf driver.**

  * Added support for AVX2 instructions in single queue Rx and Tx path.
    (The single queue model processes all packets in order within one Rx queue,
    while the split queue model separates packet data and metadata into different queues
    for parallel processing and improved performance.)

* **Updated Marvell cnxk net driver.**

  * Added flow rules support for CN20K SoC.
  * Added inline IPsec support for CN20K SoC.

* **Updated Napatech ntnic driver.**

  * Added support for the NT400D13 adapter.

* **Updated NVIDIA mlx5 driver.**

  * Added support for NVIDIA ConnectX-8 adapters.
  * Optimized port probing in large scale.
    This feature enhances the efficiency of probing VF/SFs on a large scale
    by significantly reducing the probing time.

* **Updated Wangxun ngbe driver.**

  * Added support for virtual function (VF).

* **Updated ZTE zxdh network driver.**

  * Added support for multiple queues.
  * Added support for SR-IOV VF.
  * Scatter and gather for Tx and Rx.
  * Link state and auto-negotiation.
  * MAC address filtering.
  * Multicast and promiscuous mode.
  * VLAN filtering and offload.
  * Receive Side Scaling (RSS).
  * Hardware and extended statistics.
  * Jumbo frames.
  * Checksum offload.
  * LRO and TSO.
  * Ingress metering.

* **Added Yunsilicon xsc net driver [EXPERIMENTAL].**

  Added network driver for the Yunsilicon metaScale serials NICs.

* **Updated vhost library.**

  Updated vhost library to support RSA crypto operations.

* **Updated virtio crypto driver.**

  * Added support for RSA crypto operations.

* **Updated IPsec_MB crypto driver.**

  * Added support for the SM4 GCM algorithm.

* **Added ZTE Storage Data Accelerator (ZSDA) driver.**

  Added a compress driver for ZSDA devices
  to support the deflate compression and decompression algorithm.

  See the :doc:`../compressdevs/zsda` guide for more details on the new driver.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* **Dropped support for Intel\ |reg| C++ Compiler (icc) (replaced by "icx" support)**

  Support for the older Intel\ |reg| C++ Compiler "icc" has been dropped.
  The newer Intel\ |reg| oneAPI DPC++/C++ Compiler, "icx", can be used to compile DPDK instead.


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

* eal: The ``__rte_packed`` macro for packing data is replaced with
  ``__rte_packed_begin`` / ``__rte_packed_end``.

* eal: The ``__rte_weak`` macro is deprecated and will be removed in a future release.

* net: Changed the API for CRC calculation to be thread-safe.
  An opaque context argument was introduced to the net CRC API
  containing the algorithm type and length.
  This argument is added to ``rte_net_crc_calc``, ``rte_net_crc_set_alg``
  and freed with ``rte_net_crc_free``.
  These functions are versioned to retain binary compatibility until the next LTS release.

* build: The Intel networking drivers:
  cpfl, e1000, fm10k, i40e, iavf, ice, idpf, ipn3ke and ixgbe,
  have been moved from ``drivers/net`` to a new ``drivers/net/intel`` directory.
  The resulting build output, including the driver filenames, is the same,
  but to enable/disable these drivers via Meson option requires use of the new paths.
  For example, ``-Denable_drivers=/net/i40e`` becomes ``-Denable_drivers=/net/intel/i40e``.

* build: The Intel IGC networking driver was merged with e1000 driver
  and is no longer provided as a separate driver.
  The resulting build output will not have the ``librte_net_igc.*`` driver files any more,
  but the ``librte_net_e1000.*`` driver files will provide support
  for all of the devices and features of the old driver.
  In addition, to enable/disable the driver via Meson option,
  the path has changed from ``-Denable_drivers=net/igc``
  to ``-Denable_drivers=net/intel/e1000``.

* build: The driver ``common/idpf`` has been merged into the ``net/intel/idpf`` driver.
  Similarly, the ``common/iavf`` driver has been merged into the ``net/intel/iavf`` driver.
  These changes should have no impact to end applications, but,
  when specifying the ``idpf`` or ``cpfl`` net drivers to Meson via ``-Denable_drivers`` option,
  there is no longer any need to also specify the ``common/idpf`` driver.
  In the same way, when specifying the ``iavf`` or ``ice`` net drivers,
  there is no need to also specify the ``common/iavf`` driver.
  Note, however, ``net/intel/cpfl`` driver now depends upon the ``net/intel/idpf`` driver.


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

* No ABI change that would break compatibility with 24.11.


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
