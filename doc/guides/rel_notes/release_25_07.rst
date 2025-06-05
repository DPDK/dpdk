.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.07
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_25_07.html


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

* **Added PMU library.**

  Added a Performance Monitoring Unit (PMU) library which allows Linux applications
  to perform self monitoring activities without depending on external utilities like perf.

* **Added Mucse rnp net driver.**

  Added a new network PMD which supports Mucse 10 Gigabit Ethernet NICs.
  See the :doc:`../nics/rnp` for more details.

* **Added RSS type for RoCE v2.**

  Added ``RTE_ETH_RSS_IB_BTH`` flag so that the RoCE InfiniBand Base Transport Header
  can be used as input for RSS.

* **Added burst mode query function to Intel drivers.**

  Added support for Rx and Tx burst mode query to the following drivers:

    * e1000 (igb)
    * ixgbe
    * iavf

* **Updated NVIDIA mlx5 driver.**

  * Support matching on IPv6 frag extension header with async flow template API.

* **Updated Solarflare network driver.**

  * Added support for AMD Solarflare X45xx adapters.

* **Updated virtio driver.**

  * Added support for Rx and Tx burst mode query.

* **Added ZTE Storage Data Accelerator (ZSDA) crypto driver.**

  Added a crypto driver for ZSDA devices
  to support some encrypt, decrypt and hash algorithms.

  See the :doc:`../cryptodevs/zsda` guide for more details on the new driver.

* **Updated Marvell cnxk crypto driver.**

  * Added support for CN20K SoC in cnxk CPT driver.

* **Added eventdev vector adapter.**

  Added the event vector adapter library.
  This library extends the event-based model by introducing API
  that allow applications to offload creation of event vectors,
  thereby reducing the scheduling latency.

  Added vector adapter producer mode in eventdev test to measure performance.

  See the :doc:`../prog_guide/eventdev/event_vector_adapter` guide
  for more details on the new library.

* **Added feature arc support in graph library.**

  Feature arc helps ``rte_graph`` based applications
  to manage multiple network protocols/features with runtime configurability,
  in-built node-reusability and optimized control/data plane synchronization.

  See section ``Graph feature arc`` in :doc:`../prog_guide/graph_lib` for more details.

  * Added ``ip4 output`` feature arc processing in ``ip4_rewrite`` node.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* eal: Removed the ``rte_function_versioning.h`` header from the exported headers.


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

* graph: Added ``graph`` field to the ``rte_node.dispatch`` structure.


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
