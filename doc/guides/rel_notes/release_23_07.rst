.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 23.07
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_23_07.html


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

* **Added AMD CDX bus support.**

  CDX bus driver has been added to support AMD CDX bus,
  which operates on FPGA based CDX devices.
  The CDX devices are memory mapped on system bus for embedded CPUs.

* **Added MMIO read and write API to PCI bus.**

  Introduced ``rte_pci_mmio_read()`` and ``rte_pci_mmio_write()`` API
  to PCI bus so that PCI drivers can access PCI memory resources
  when they are not mapped to process address space.

* **Added LLRS FEC mode in ethdev.**

  Added LLRS algorithm to Forward Error Correction (FEC) modes.

* **Added flow matching of Tx queue.**

  Added ``RTE_FLOW_ITEM_TYPE_TX_QUEUE`` rte_flow pattern
  to match the Tx queue of the sent packet.

* **Added flow matching of Infiniband BTH.**

  Added ``RTE_FLOW_ITEM_TYPE_IB_BTH`` to match Infiniband BTH fields.

* **Added actions to push or remove IPv6 extension.**

  Added ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH`` and ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH``
  to push or remove the specific IPv6 extension into or from the packets.
  Push always put the new extension as the last one due to the next header awareness.

* **Added indirect list flow action.**

  Added API to manage (create, destroy, update) a list of indirect actions.

* **Added flow rule update.**

  * Added API for updating the action list in the already existing rule.
    Introduced both ``rte_flow_actions_update()`` and
    ``rte_flow_async_actions_update()`` functions.

* **Added vmxnet3 version 7 support.**

  Added support for vmxnet3 version 7 which includes support
  for uniform passthrough(UPT). The patches also add support
  for new capability registers, large passthrough BAR and some
  performance enhancements for UPT.


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

* ethdev: Ensured all entries in MAC address list are uniques.
  When setting a default MAC address with the function
  ``rte_eth_dev_default_mac_addr_set``,
  the default one needs to be removed by the user
  if it was already in the address list.


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

* ethdev: In the experimental ``struct rte_flow_action_modify_data``:

  * ``level`` field was reduced to 8 bits.
  * ``tag_index`` field replaced ``level`` field in representing tag array for
    ``RTE_FLOW_FIELD_TAG`` type.


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
