.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 24.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_24_11.html


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

* **Extended service cores statistics.**

  Two new per-service counters are added to the service cores framework.

  * ``RTE_SERVICE_ATTR_IDLE_CALL_COUNT`` tracks the number of service function
    invocations where no actual work was performed.

  * ``RTE_SERVICE_ATTR_ERROR_CALL_COUNT`` tracks the number invocations
    resulting in an error.

  The new statistics are useful for debugging and profiling.

* **Added a new insertion by index with pattern table insertion type.**

  Extended rte_flow_table_insertion_type enum with new
  RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN type.

* **Added flow rule insertion by index with pattern to the Flow API.**

  Added API for inserting the rule by index with pattern.
  Introduced rte_flow_async_create_by_index_with_pattern() function.

* **Added the action to redirect packets to a particular index in a flow table.**

  Introduced RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX action type.

* **Added support for dumping registers with names and filtering by modules.**

  Added new API functions ``rte_eth_dev_get_reg_info_ext()`` to filter the
  registers by module names and get the information (names, values and other
  attributes) of the filtered registers.

* **Updated Cisco enic driver.**

  * Added SR-IOV VF support.
  * Added recent 1400/14000 and 15000 models to the supported list.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* ethdev: Removed the __rte_ethdev_trace_rx_burst symbol, as the corresponding
  tracepoint was split into two separate ones for empty and non-empty calls.


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

* ethdev: Added ``filter`` and ``names`` fields to ``rte_dev_reg_info``
  structure for filtering by modules and reporting names of registers.


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
