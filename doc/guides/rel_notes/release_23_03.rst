.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2022 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 23.03
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_23_03.html


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

* **Introduced lock annotations.**

  Added lock annotations attributes so that clang can statically analyze lock
  correctness.

* **Added support for reporting lcore usage in applications.**

  * The ``/eal/lcore/list``, ``/eal/lcore/usage`` and ``/eal/lcore/info``
    telemetry endpoints have been added to provide information similar to
    ``rte_lcore_dump()``.
  * Applications can register a callback at startup via
    ``rte_lcore_register_usage_cb()`` to provide lcore usage information.

* **Added flow matching of IPv6 routing extension.**

  Added ``RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT``
  to match IPv6 routing extension header fields.

* **Updated AMD axgbe driver.**

  * Added multi-process support.

* **Updated Corigine nfp driver.**

  * Added support for meter options.
  * Added support for rte_flow meter action.

* **Updated Wangxun ngbe driver.**

  * Added chip overheat detection support.

* **Added new algorithms to cryptodev.**

  Added SHAKE-128 and SHAKE-256 symmetric secure hash algorithm.

* **Updated Marvell cnxk crypto driver.**

  Added support for SHAKE hash algorithm in cn9k and cn10k.

* **Updated Intel QuickAssist Technology (QAT) crypto driver.**

  * Added support for SHA3 224/256/384/512 plain hash in QAT GEN 3.
  * Added support for SHA3 256 plain hash in QAT GEN 2.
  * Added support for asymmetric crypto in QAT GEN3.

* **Updated the eventdev reconfigure logic for service based adapters.**

  * eventdev reconfig logic is enhanced to increment the
    ``rte_event_dev_config::nb_single_link_event_port_queues`` parameter
    if event port config is of type ``RTE_EVENT_PORT_CFG_SINGLE_LINK``.
  * With this change, the application no longer needs to account for the
    ``rte_event_dev_config::nb_single_link_event_port_queues`` parameter
    required for eth_rx, eth_tx, crypto and timer eventdev adapters.

* **Added pcap trace support in graph library.**

  * Added support to capture packets at each graph node with packet metadata and
    node name.


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

* The experimental function ``rte_pcapng_copy`` was updated to support comment
  section in enhanced packet block in the pcapng library.

* The experimental structures ``struct rte_graph_param``, ``struct rte_graph``
  and ``struct graph`` were updated to support pcap trace in the graph library.


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

* No ABI change that would break compatibility with 22.11.


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
