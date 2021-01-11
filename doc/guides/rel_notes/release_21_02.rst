.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.02
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html
      xdg-open build/doc/html/guides/rel_notes/release_21_02.html


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

* **Added new ethdev API for PMD power management.**

  Added ``rte_eth_get_monitor_addr()``, to be used in conjunction with
  ``rte_power_monitor()`` to enable automatic power management for PMDs.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with fixes and improvements, including:

  * Added support for Stingray2 device.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Introduced basic support on Windows.
  * Added GTP PDU session container matching and raw encap/decap.

* **Updated GSO support.**

  * Added inner UDP/IPv4 support for VXLAN IPv4 GSO.

* **Added enqueue & dequeue callback APIs for cryptodev library.**

  Cryptodev library is added with enqueue & dequeue callback APIs to
  enable applications to add/remove user callbacks which gets called
  for every enqueue/dequeue operation.

* **Updated the OCTEON TX2 crypto PMD.**

  * Updated the OCTEON TX2 crypto PMD lookaside protocol offload for IPsec with
    ESN and anti-replay support.
  * Updated the OCTEON TX2 crypto PMD with CN98xx support.
  * Added support for aes-cbc sha1-hmac cipher combination in OCTEON TX2 crypto
    PMD lookaside protocol offload for IPsec.
  * Added support for aes-cbc sha256-128-hmac cipher combination in OCTEON TX2
    crypto PMD lookaside protocol offload for IPsec.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* Removed support for NetXtreme devices belonging to ``BCM573xx and
  BCM5740x`` families. Specifically the support for the following Broadcom
  PCI device IDs ``0x16c8, 0x16c9, 0x16ca, 0x16ce, 0x16cf, 0x16df, 0x16d0,``
  ``0x16d1, 0x16d2, 0x16d4, 0x16d5, 0x16e7, 0x16e8, 0x16e9`` has been removed.

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

* cryptodev: The structure ``rte_cryptodev`` has been updated with pointers
  for adding enqueue and dequeue callbacks.


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
