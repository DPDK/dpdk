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

* **Added Ethernet PMD power management helper API.**

  A new helper API has been added to make using Ethernet PMD power management
  easier for the user: ``rte_power_ethdev_pmgmt_queue_enable()``. Three power
  management schemes are supported initially:

  * Power saving based on UMWAIT instruction (x86 only)
  * Power saving based on ``rte_pause()`` (generic) or TPAUSE instruction (x86 only)
  * Power saving based on frequency scaling through the ``librte_power`` library

* **Added GENEVE TLV option in rte_flow.**

  Added support for matching and raw encap/decap of GENEVE TLV option.

* **Added support of modify field action in the flow API.**

  Added modify action support to perform various operations on
  any arbitrary header field (as well as mark, metadata or tag values):
  ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``.
  Supported operations are: overwriting a field with the content from
  another field, addition and subtraction using an immediate value.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with fixes and improvements, including:

  * Added support for Stingray2 device.

* **Updated Cisco enic driver.**

  * Added support for 64B completion queue entries

* **Updated Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added Double VLAN support for DCF switch QinQ filtering.
  * Added support for UDP dynamic port assignment for eCPRI tunnel in DCF.

* **Updated Intel iavf driver.**

  Updated iavf PMD with new features and improvements, including:

  * Added support for FDIR/RSS packet steering for eCPRI flow.
  * Added support for FDIR TCP/UDP pattern without input set.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Introduced basic support on Windows.
  * Added GTP PDU session container matching and raw encap/decap.
  * Added support for RSS action in the sample sub-actions list.
  * Added support for E-Switch mirroring and jump action in the same flow.
  * Added support to handle modify action in correct order regarding the
    mirroring action on E-Switch.

* **Updated Wangxun txgbe driver.**

  Updated the Wangxun txgbe driver with new features and improvements, including:

  * Add support for generic flow API.
  * Add support for traffic manager.
  * Add support for IPsec.

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

* **Added mlx5 compress PMD.**

  Added a new compress PMD driver for Bluefield 2 adapters.

  See the :doc:`../compressdevs/mlx5` for more details.

* **Added support for build-time checking of header includes.**

  A new build option ``check_includes`` has been added, which, when enabled,
  will perform build-time checking on DPDK public header files, to ensure none
  are missing dependent header includes. This feature, disabled by default, is
  intended for use by developers contributing to the DPDK SDK itself, and is
  integrated into the build scripts and automated CI for patch contributions.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* The internal header files ``rte_ethdev_driver.h``, ``rte_ethdev_vdev.h`` and
  ``rte_ethdev_pci.h`` are no longer installed as part of the DPDK
  ``ninja install`` action and are renamed to ``ethdev_driver.h``,
  ``ethdev_vdev.h`` and ``ethdev_pci.h`` respectively in the source tree, to
  reflect the fact that they are non-public headers.

* The internal header files ``rte_eventdev_pmd.h``, ``rte_eventdev_pmd_vdev.h``
  and ``rte_eventdev_pmd_pci.h`` are no longer installed as part of the DPDK
  ``ninja install`` action and are renamed to ``eventdev_pmd.h``,
  ``eventdev_pmd_vdev.h`` and ``eventdev_pmd_pci.h`` respectively in the source
  tree, to reflect the fact that they are non-public headers.

* Removed support for NetXtreme devices belonging to ``BCM573xx and
  BCM5740x`` families. Specifically the support for the following Broadcom
  PCI device IDs ``0x16c8, 0x16c9, 0x16ca, 0x16ce, 0x16cf, 0x16df, 0x16d0,``
  ``0x16d1, 0x16d2, 0x16d4, 0x16d5, 0x16e7, 0x16e8, 0x16e9`` has been removed.

* The ``check-includes.sh`` script for checking DPDK header files has been
  removed, being replaced by the ``check_includes`` build option described
  above.


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

* config: Removed the old macros, included in ``rte_config.h``,
  to indicate which DPDK libraries and drivers are built.
  The new macros are generated by meson in a standardized format:
  ``RTE_LIB_<NAME>`` and ``RTE_<CLASS>_<NAME>``, where ``NAME`` is
  the upper-case component name, e.g. ``EAL``, ``ETHDEV``, ``VIRTIO``,
  and ``CLASS`` is the upper-case driver class, e.g. ``NET``, ``CRYPTO``.

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
