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

* **Added cryptodev queue pair reset support.**

  A new API ``rte_cryptodev_queue_pair_reset`` is added to reset a particular
  queue pair of a device.

* **Added support for asymmetric EDDSA in cryptodev.**

  Support for asymmetric EDDSA as referenced in
  RFC: https://datatracker.ietf.org/doc/html/rfc8032 is added in cryptodev.

* **Updated IPsec_MB crypto driver.**

  * Added support for SM3 algorithm.
  * Added support for SM3 HMAC algorithm.
  * Added support for SM4 CBC, SM4 ECB and SM4 CTR algorithms.

* **Updated openssl crypto driver.**

  Added support for asymmetric crypto EDDSA algorithm.

* **Updated Marvell cnxk crypto driver.**

  Added support for asymmetric crypto EDDSA algorithm.


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

* cryptodev: New API ``rte_cryptodev_asym_xform_capability_check_opcap``
  is added to check per operation capability which is required for
  asymmetric algorithms such as SM2, EdDSA.

* ipsec: New APIs ``rte_ipsec_pkr_crypto_prepare_stateless`` and
  ``rte_ipsec_pkt_cpu_prepare_stateless`` are added to support
  stateless IPsec processing. These APIs enables user to provide
  sequence number to be used for the IPsec operation.


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

* cryptodev: The queue pair configuration structure ``rte_cryptodev_qp_conf``
  is updated to have a new parameter to set priority of that particular queue pair.

* cryptodev: The enum ``rte_crypto_asym_xform_type`` and struct ``rte_crypto_asym_op``
  are updated to include new values to support EDDSA.

* cryptodev: The ``rte_crypto_rsa_xform`` struct member to hold private key
  in either exponent or quintuple format is changed from union to struct data type.
  This change is to support ASN.1 syntax (RFC 3447 Appendix A.1.2).

* cryptodev: The padding struct ``rte_crypto_rsa_padding`` is moved from
  ``rte_crypto_rsa_op_param`` to ``rte_crypto_rsa_xform`` as the padding information
  is part of session creation instead of per packet crypto operation.
  This change is required to support virtio-crypto specifications.


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
