.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 26.03
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_26_03.html


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

* **Updated AMD axgbe ethernet driver.**

  * Added support for V4000 Krackan2e.

* **Updated Intel iavf driver.**

  * Added support for pre and post VF reset callbacks.

* **Added 256-NEA/NCA/NIA algorithms in cryptodev library.**

  Added support for following wireless algorithms:
  * NEA4, NIA4, NCA4: Snow 5G confidentiality, integrity and AEAD modes.
  * NEA5, NIA5, NCA5: AES 256 confidentiality, integrity and AEAD modes.
  * NEA6, NIA6, NCA6: ZUC 256 confidentiality, integrity and AEAD modes.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* **Discontinued support for AMD Solarflare SFN7xxx family boards.**

  7000 series adaptors are out of support in terms of hardware.

* **Removed the SSE vector paths from some Intel drivers.**

  The SSE path was not widely used, so it was removed
  from the i40e, iavf and ice drivers.
  Each of these drivers have faster vector paths (AVX2 and AVX-512)
  which have feature parity with the SSE paths,
  and a fallback scalar path which also has feature parity.


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

* **Added additional length checks for name parameter lengths.**

  Several library functions now have additional name length checks
  instead of silently truncating.

  * lpm: name must be less than RTE_LPM_NAMESIZE.
  * hash: name parameter must be less than RTE_HASH_NAMESIZE.
  * efd: name must be less than RTE_EFD_NAMESIZE.
  * tailq: name must be less than RTE_TAILQ_NAMESIZE.
  * cfgfile: name must be less than CFG_NAME_LEN
    and value must be less than CFG_VALUE_LEN.


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

* No ABI change that would break compatibility with 25.11.


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
