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

* **Added new bit manipulation API.**

  The support for bit-level operations on single 32- and 64-bit words in
  <rte_bitops.h> has been extended with semantically well-defined functions.

  * ``rte_bit_[test|set|clear|assign|flip]`` functions provide excellent
    performance (by avoiding restricting the compiler and CPU), but give
    no guarantees in regards to memory ordering or atomicity.

  * ``rte_bit_atomic_*`` provide atomic bit-level operations, including
    the possibility to specify memory ordering constraints.

  The new public API elements are polymorphic, using the _Generic-based
  macros (for C) and function overloading (in C++ translation units).

* **Added multi-word bitset API.**

  A new multi-word bitset API has been introduced in the EAL.
  The RTE bitset is optimized for scenarios where the bitset size exceeds the
  capacity of a single word (e.g., larger than 64 bits), but is not large
  enough to justify the overhead and complexity of the more scalable,
  yet slower, <rte_bitmap.h> API.
  This addition provides an efficient and straightforward alternative
  for handling bitsets of intermediate sizes.

* **Extended service cores statistics.**

  Two new per-service counters are added to the service cores framework.

  * ``RTE_SERVICE_ATTR_IDLE_CALL_COUNT`` tracks the number of service function
    invocations where no actual work was performed.

  * ``RTE_SERVICE_ATTR_ERROR_CALL_COUNT`` tracks the number invocations
    resulting in an error.

  The new statistics are useful for debugging and profiling.

* **Hardened rte_malloc and related functions.**

  Added function attributes to ``rte_malloc`` and similar functions
  that can catch some obvious bugs at compile time (with GCC 11.0 or later).
  Examples: calling ``free`` on pointer that was allocated with ``rte_malloc``
  (and vice versa); freeing the same pointer twice in the same routine;
  freeing an object that was not created by allocation; etc.

* **Updated Marvell cnxk mempool driver.**

  * Added mempool driver support for CN20K SoC.

* **Updated Marvell cnxk net driver.**

  * Added ethdev driver support for CN20K SoC.

* **Added cryptodev queue pair reset support.**

  A new API ``rte_cryptodev_queue_pair_reset`` is added
  to reset a particular queue pair of a device.

* **Added cryptodev asymmetric EdDSA support.**

  Added asymmetric EdDSA as referenced in `RFC 8032
  <https://datatracker.ietf.org/doc/html/rfc8032>`_.

* **Added cryptodev SM4-XTS support.**

  Added symmetric cipher algorithm ShangMi 4 (SM4) in XTS mode.

* **Updated IPsec_MB crypto driver.**

  * Added support for SM3 algorithm.
  * Added support for SM3 HMAC algorithm.
  * Added support for SM4 CBC, SM4 ECB and SM4 CTR algorithms.

* **Updated openssl crypto driver.**

  * Added support for asymmetric crypto EdDSA algorithm.

* **Updated Marvell cnxk crypto driver.**

  * Added support for asymmetric crypto EdDSA algorithm.

* **Added stateless IPsec processing.**

  New functions were added to enable
  providing sequence number to be used for the IPsec operation.

* **Added event device pre-scheduling support.**

  Added support for pre-scheduling of events to event ports
  to improve scheduling performance and latency.

  * Added ``rte_event_dev_config::preschedule_type``
    to configure the device level pre-scheduling type.

  * Added ``rte_event_port_preschedule_modify``
    to modify pre-scheduling type on a given event port.

  * Added ``rte_event_port_preschedule``
    to allow applications provide explicit pre-schedule hints to event ports.

* **Updated event device library for independent enqueue feature.**

  Added support for independent enqueue feature.
  With this feature eventdev supports enqueue in any order
  or specifically in a different order than dequeue.
  The feature is intended for eventdevs supporting burst mode.
  Applications should use ``RTE_EVENT_PORT_CFG_INDEPENDENT_ENQ`` to enable
  the feature if the capability ``RTE_EVENT_DEV_CAP_INDEPENDENT_ENQ`` exists.

* **Updated DLB2 event driver.**

  * Added independent enqueue feature.

* **Updated DSW event driver.**

  * Added independent enqueue feature.

* **Added IPv4 network order lookup in the FIB library.**

  A new flag field is introduced in ``rte_fib_conf`` structure.
  This field is used to pass an extra configuration settings such as ability
  to lookup IPv4 addresses in network byte order.

* **Added new API to register telemetry endpoint callbacks with private arguments.**

  A new ``rte_telemetry_register_cmd_arg`` function is available to pass an opaque value to
  telemetry endpoint callback.


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

* kvargs: reworked the process API.

  * The already existing ``rte_kvargs_process`` now only handles key=value cases and
    rejects if only a key is present in the parsed string.
  * ``rte_kvargs_process_opt`` has been added to behave as ``rte_kvargs_process`` in previous
    releases: it handles key=value and only-key cases.
  * Both ``rte_kvargs_process`` and ``rte_kvargs_process_opt`` reject a NULL ``kvlist`` parameter.


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

* eal: The maximum number of file descriptors that can be passed to a secondary process
  has been increased from 8 to 253 (which is the maximum possible with Unix domain socket).
  This allows for more queues when using software devices such as TAP and XDP.

* cryptodev: The queue pair configuration structure ``rte_cryptodev_qp_conf``
  is updated to have a new parameter to set priority of that particular queue pair.

* cryptodev: The list end enumerators ``RTE_CRYPTO_ASYM_XFORM_TYPE_LIST_END``
  and ``RTE_CRYPTO_RSA_PADDING_TYPE_LIST_END`` are removed
  to allow subsequent addition of new asymmetric algorithms and RSA padding types.

* cryptodev: The enum ``rte_crypto_asym_xform_type`` and struct ``rte_crypto_asym_op``
  are updated to include new values to support EdDSA.

* cryptodev: The ``rte_crypto_rsa_xform`` struct member to hold private key
  in either exponent or quintuple format is changed from union to struct data type.
  This change is to support ASN.1 syntax (RFC 3447 Appendix A.1.2).

* cryptodev: The padding struct ``rte_crypto_rsa_padding`` is moved
  from ``rte_crypto_rsa_op_param`` to ``rte_crypto_rsa_xform``
  as the padding information is part of session creation
  instead of per packet crypto operation.
  This change is required to support virtio-crypto specifications.

* bbdev: The structure ``rte_bbdev_stats`` was updated to add a new parameter
  to optionally report the number of enqueue batch available ``enqueue_depth_avail``.

* eventdev: Added ``preschedule_type`` field to ``rte_event_dev_config`` structure.


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
