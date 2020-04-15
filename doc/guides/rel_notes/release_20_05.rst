.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_20_05.html


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
     =========================================================

* **Added Trace Library and Tracepoints**

  A native implementation of ``common trace format(CTF)`` based trace library
  has been added to provide the ability to add tracepoints in
  application/library to get runtime trace/debug information for control and
  fast APIs with minimum impact on fast path performance.
  Typical trace overhead is ~20 cycles and instrumentation overhead is 1 cycle.
  Added tracepoints in ``EAL``, ``ethdev``, ``cryptodev``, ``eventdev`` and
  ``mempool`` libraries for important functions.

* **Added new API for rte_ring.**

  * New synchronization modes for rte_ring.

  Introduced new optional MT synchronization modes for rte_ring:
  Relaxed Tail Sync (RTS) mode and Head/Tail Sync (HTS) mode.
  With these mode selected, rte_ring shows significant improvements for
  average enqueue/dequeue times on overcommitted systems.

  * Added peek style API for rte_ring.

  For rings with producer/consumer in RTE_RING_SYNC_ST, RTE_RING_SYNC_MT_HTS
  mode, provide an ability to split enqueue/dequeue operation into two phases
  (enqueue/dequeue start; enqueue/dequeue finish). That allows user to inspect
  objects in the ring without removing them from it (aka MT safe peek).

* **Updated Amazon ena driver.**

  Updated ena PMD with new features and improvements, including:

  * Added support for large LLQ (Low-latency queue) headers.
  * Added Tx drops as new extended driver statistic.
  * Added support for accelerated LLQ mode.
  * Handling of the 0 length descriptors on the Rx path.

* **Updated Intel i40e driver.**

  Updated i40e PMD with new features and improvements, including:

  * Enable MAC address as FDIR input set for ipv4-other, ipv4-udp and ipv4-tcp.

* **Updated the Intel iavf driver.**

  Update the Intel iavf driver with new features and improvements, including:

  * Added generic filter support.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for DCF (Device Config Function) feature.
  * Added switch filter support for intel DCF.

* **Updated Marvell OCTEON TX2 ethdev driver.**

  Updated Marvell OCTEON TX2 ethdev driver with traffic manager support with
  below features.

  * Hierarchial Scheduling with DWRR and SP.
  * Single rate - Two color, Two rate - Three color shaping.

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added support for matching on IPv4 Time To Live and IPv6 Hop Limit.
  * Added support for creating Relaxed Ordering Memory Regions.

* **Updated the AESNI MB crypto PMD.**

  * Added support for intel-ipsec-mb version 0.54.
  * Updated the AESNI MB PMD with AES-256 DOCSIS algorithm.
  * Added support for synchronous Crypto burst API.

* **Updated the AESNI GCM crypto PMD.**

  * Added support for intel-ipsec-mb version 0.54.

* **Added a new driver for Intel Foxville I225 devices.**

  Added the new ``igc`` net driver for Intel Foxville I225 devices. See the
  :doc:`../nics/igc` NIC guide for more details on this new driver.

* **Added handling of mixed crypto algorithms in QAT PMD for GEN2.**

  Enabled handling of mixed algorithms in encrypted digest hash-cipher
  (generation) and cipher-hash (verification) requests in QAT PMD
  when running on GEN2 QAT hardware with particular firmware versions
  (GEN3 support was added in DPDK 20.02).

* **Added plain SHA-1,224,256,384,512 support to QAT PMD.**

  Added support for plain SHA-1, SHA-224, SHA-256, SHA-384 and SHA-512 hashes
  to QAT PMD.

* **Added QAT intermediate buffer too small handling in QAT compression PMD.**

  Added a special way of buffer handling when internal QAT intermediate buffer
  is too small for Huffman dynamic compression operation. Instead of falling
  back to fixed compression, the operation is now split into multiple smaller
  dynamic compression requests (possible to execute on QAT) and their results
  are then combined and copied into the output buffer. This is not possible if
  any checksum calculation was requested - in such case the code falls back to
  fixed compression as before.

* **Updated the turbo_sw bbdev PMD.**

  Supported large size code blocks which does not fit in one mbuf segment.

* **Added Intel FPGA_5GNR_FEC bbdev PMD.**

  Added a new ``fpga_5gnr_fec`` bbdev driver for the Intel\ |reg| FPGA PAC
  (Programmable  Acceleration Card) N3000.  See the
  :doc:`../bbdevs/fpga_5gnr_fec` BBDEV guide for more details on this new driver.

* **Updated ipsec-secgw sample application with following features.**

  * Updated ipsec-secgw application to add event based packet processing.
    The worker thread(s) would receive events and submit them back to the
    event device after the processing. This way, multicore scaling and HW
    assisted scheduling is achieved by making use of the event device
    capabilities. The event mode currently supports only inline IPsec
    protocol offload.

  * Updated ipsec-secgw application to support key sizes for AES-192-CBC,
    AES-192-GCM, AES-256-GCM algorithms.

  * Added IPsec inbound load-distribution support for ipsec-secgw application
    using NIC load distribution feature(Flow Director).


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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
   =========================================================


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
   =========================================================

* No ABI change that would break compatibility with DPDK 20.02 and 19.11.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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
   =========================================================
