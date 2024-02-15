.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.03
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_24_03.html


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

* **Added HiSilicon UACCE bus support.**

  UACCE (Unified/User-space-access-intended Accelerator Framework) bus driver
  has been added, so that the accelerator devices could be seen in DPDK and could
  be further registered such as a compress, crypto, DMA and ethernet devices.

* **Introduced argument parsing library.**

  The argparse library was added to ease writing user-friendly applications,
  replacing ``getopt()`` usage.

* **Improved RSS hash algorithm support.**

  Added new function ``rte_eth_find_rss_algo``
  to get RSS hash algorithm by its name.

* **Added query of used descriptors number in Tx queue.**

  * Added a fath path function ``rte_eth_tx_queue_count``
    to get the number of used descriptors of a Tx queue.

* **Added hash calculation of an encapsulated packet as done by the HW.**

  Added function to calculate hash when doing tunnel encapsulation:
  ``rte_flow_calc_encap_hash()``

* **Added flow matching items.**

  * Added ``RTE_FLOW_ITEM_TYPE_COMPARE`` to allow matching
    on comparison result between packet fields or value.
  * Added ``RTE_FLOW_ITEM_TYPE_RANDOM`` to match a random value,
    and ``RTE_FLOW_FIELD_RANDOM`` to represent it with a field ID.

* **Added flow template table resizing.**

  * ``RTE_FLOW_TABLE_SPECIALIZE_RESIZABLE_TABLE`` table configuration bit.
    Set at table creation to allow future resizing.
  * ``rte_flow_template_table_resizable()``.
    Query whether template table can be resized.
  * ``rte_flow_template_table_resize()``.
    Reconfigure template table for new flows capacity.
  * ``rte_flow_async_update_resized()``.
    Reconfigure flows for the updated table configuration.
  * ``rte_flow_template_table_resize_complete()``.
    Complete table resize.

* **Updated Atomic Rules' Arkville driver.**

  * Added support for Atomic Rules' TK242 packet-capture family of devices
    with PCI IDs: ``0x1024, 0x1025, 0x1026``.

* **Updated NVIDIA mlx5 driver.**

  * Added support for comparing result between packet fields or value.

* **Updated Marvell cnxk crypto driver.**

  * Added support for Rx inject in crypto_cn10k.
  * Added support for TLS record processing in crypto_cn10k
    to support TLS v1.2, TLS v1.3 and DTLS v1.2.
  * Added PMD API to allow raw submission of instructions to CPT.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* log: Removed the statically defined logtypes that were used internally by DPDK.
  All code should be using the dynamic logtypes (see ``RTE_LOG_REGISTER()``).
  The application reserved statically defined logtypes ``RTE_LOGTYPE_USER1..RTE_LOGTYPE_USER8``
  are still defined.


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

* eal: Removed ``typeof(type)`` from the expansion of ``RTE_DEFINE_PER_LCORE``
  and ``RTE_DECLARE_PER_LCORE`` macros aligning them with their intended design.
  If use with an expression is desired applications can adapt by supplying
  ``typeof(e)`` as an argument.

* eal: Improved ``RTE_BUILD_BUG_ON`` by using C11 ``static_assert``.
  Non-constant expressions are now rejected instead of being silently ignored.

* gso: ``rte_gso_segment`` now returns -ENOTSUP for unknown protocols.

* ethdev: Renamed structure ``rte_flow_action_modify_data`` to be
  ``rte_flow_field_data`` for more generic usage.


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
