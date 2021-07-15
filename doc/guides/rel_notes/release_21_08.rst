.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.08
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html
      xdg-open build/doc/html/guides/rel_notes/release_21_08.html


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

* **Added auxiliary bus support.**

  Auxiliary bus provides a way to split function into child-devices
  representing sub-domains of functionality. Each auxiliary device
  represents a part of its parent functionality.

* **Added XZ compressed firmware support.**

  Using ``rte_firmware_read``, a driver can now handle XZ compressed firmware
  in a transparent way, with EAL uncompressing using libarchive if this library
  is available when building DPDK.

* **Updated Intel iavf driver.**

  * Added Tx QoS VF queue TC mapping.
  * Added FDIR and RSS for GTPoGRE, support filter based on GTPU TEID/QFI,
    outer most L3 or inner most l3/l4. 

* **Updated Intel ice driver.**

  * In AVX2 code, added the new RX and TX paths to use the HW offload
    features. When the HW offload features are configured to be used, the
    offload paths are chosen automatically. In parallel the support for HW
    offload features was removed from the legacy AVX2 paths.
  * Added Tx QoS TC bandwidth configuration in DCF.

* **Added support for Marvell CN10K SoC ethernet device.**

  * Added net/cnxk driver which provides the support for the integrated ethernet
    device.

* **Updated Mellanox mlx5 driver.**

  * Added support for meter hierarchy.
  * Added devargs options ``allow_duplicate_pattern``.
  * Added matching on IPv4 Internet Header Length (IHL).
  * Added support for matching on VXLAN header last 8-bits reserved field.
  * Optimized multi-thread flow rule insertion rate.

* **Added Wangxun ngbe PMD.**

  Added a new PMD driver for Wangxun 1 Gigabit Ethernet NICs.
  See the :doc:`../nics/ngbe` for more details.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added COUNT action support for SN1000 NICs

* **Updated Intel QuickAssist crypto PMD.**

  Added fourth generation of QuickAssist Technology(QAT) devices support.
  Only symmetric crypto has been currently enabled, compression and asymmetric
  crypto PMD will fail to create.

* **Added support for Marvell CNXK crypto driver.**

  * Added cnxk crypto PMD which provides support for an integrated
    crypto driver for CN9K and CN10K series of SOCs. Support for
    symmetric crypto algorithms is added to both the PMDs.
  * Added support for lookaside protocol (IPsec) offload in cn10k PMD.
  * Added support for asymmetric crypto operations in cn9k and cn10k PMD.

* **Updated Marvell OCTEON TX crypto PMD.**

  Added support for crypto adapter OP_FORWARD mode.

* **Updated ISAL compress device PMD.**

  The ISAL compress device PMD now supports Arm platforms.

* **Added Baseband PHY CNXK PMD.**

  Added Baseband PHY PMD which allows to configure BPHY hardware block
  comprising accelerators and DSPs specifically tailored for 5G/LTE inline
  usecases. Configuration happens via standard rawdev enq/deq operations. See
  the :doc:`../rawdevs/cnxk_bphy` rawdev guide for more details on this driver.

* **Added support for Marvell CN10K, CN9K, event Rx/Tx adapter.**

  * Added Rx/Tx adapter support for event/cnxk when the ethernet device requested
    is net/cnxk.
  * Added support for event vectorization for Rx/Tx adapter.

* **Added cppc_cpufreq support to Power Management library.**

  Added support for cppc_cpufreq driver which works on most arm64 platforms.

* **Added multi-queue support to Ethernet PMD Power Management**

  The experimental PMD power management API now supports managing
  multiple Ethernet Rx queues per lcore.


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

* eal: ``rte_strscpy`` sets ``rte_errno`` to ``E2BIG`` in case of string
  truncation.

* eal: ``rte_power_monitor`` and the ``rte_power_monitor_cond`` struct changed
  to use a callback mechanism.

* rte_power: The experimental PMD power management API is no longer considered
  to be thread safe; all Rx queues affected by the API will now need to be
  stopped before making any changes to the power management scheme.


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
