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

* **Updated af_packet net driver.**

  * Added ability to option to configure receive packet fanout mode.
  * Added statistics for failed buffer allocation and missed packets.

* **Updated AMD axgbe driver.**

  * Added support for the TCP Segmentation Offload (TSO).

* **Updated Wangxun ngbe driver.**

  * Added support for virtual function (VF).

* **Updated ZXDH network driver.**

  * Added support for multiple queues.
  * Added support SR-IOV VF.
  * Scattered and gather for TX and RX.
  * Link state and auto-negotiation.
  * MAC address filtering.
  * Multicast and Promiscuous mode.
  * VLAN filtering and offload.
  * Receive Side Scaling (RSS).
  * Hardware statistics.
  * Jumbo frames.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


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

* build: The Intel networking drivers:
  cpfl, e1000, fm10k, i40e, iavf, ice, idpf, igc, ipn3ke and ixgbe,
  have been moved from ``drivers/net`` to a new ``drivers/net/intel`` directory.
  The resulting build output, including the driver filenames, is the same,
  but to enable/disable these drivers via Meson option requires use of the new paths.
  For example, ``-Denable_drivers=/net/i40e`` becomes ``-Denable_drivers=/net/intel/i40e``.


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
