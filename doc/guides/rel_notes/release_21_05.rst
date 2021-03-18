.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html
      xdg-open build/doc/html/guides/rel_notes/release_21_05.html


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

* **Enhanced ethdev representor syntax.**

  * Introduced representor type of VF, SF and PF.
  * Supported sub-function and multi-host in representor syntax::

      representor=#            [0,2-4]      /* Legacy VF compatible.         */
      representor=[[c#]pf#]vf# c1pf2vf3     /* VF 3 on PF 2 of controller 1. */
      representor=[[c#]pf#]sf# sf[0,2-1023] /* 1023 SFs.                     */
      representor=[c#]pf#      c2pf[0,1]    /* 2 PFs on controller 2.        */

* **Updated Arkville PMD driver.**

  Updated Arkville net driver with new features and improvements, including:

  * Updated dynamic PMD extensions API using standardized names.

  * Added support for new Atomic Rules PCI device IDs ``0x100f, 0x1010, 0x1017,
    0x1018, 0x1019``.

* **Updated Broadcom bnxt driver.**

  * Updated HWRM structures to 1.10.2.15 version.

* **Updated Hisilicon hns3 driver.**

  * Added support for module EEPROM dumping.
  * Added support for freeing Tx mbuf on demand.
  * Added support for copper port in Kunpeng930.

* **Updated NXP DPAA driver.**

  * Added support for shared ethernet interface.
  * Added support for external buffers in Tx.

* **Updated NXP DPAA2 driver.**

  * Added support for traffic management.
  * Added support for configurable Tx confirmation.
  * Added support for external buffers in Tx.

* **Updated Wangxun txgbe driver.**

  * Added support for txgbevf PMD.

* **Updated the AF_XDP driver.**

  * Added support for preferred busy polling.

* **Updated testpmd.**

  * Added a command line option to configure forced speed for Ethernet port.
    ``dpdk-testpmd -- --eth-link-speed N``
  * Added command to display Rx queue used descriptor count.
    ``show port (port_id) rxq (queue_id) desc used count``


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

* eal: The experimental TLS API added in ``rte_thread.h`` has been renamed
  from ``rte_thread_tls_*`` to ``rte_thread_*`` to avoid naming redundancy
  and confusion with the transport layer security term.


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

* No ABI change that would break compatibility with 20.11.

* The experimental function ``rte_telemetry_legacy_register`` has been
  removed from the public API and is now an internal-only function. This
  function was already marked as internal in the API documentation for it,
  and was not for use by external applications.


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
