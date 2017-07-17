DPDK Release 17.08
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_17_08.html


New Features
------------

.. This section should contain new features added in this release. Sample
   format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to
     understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like
     this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     This section is a comment. do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added Service Core functionality.**

  The service core functionality added to EAL allows DPDK to run services such
  as SW PMDs on lcores without the application manually running them. The
  service core infrastructure allows flexibility of running multiple services
  on the same service lcore, and provides the application with powerful APIs to
  configure the mapping from service lcores to services.

* **Added Generic Receive Offload API.**

  Generic Receive Offload (GRO) API supports to reassemble TCP/IPv4
  packets. GRO API assumes all inputted packets are with correct
  checksums. GRO API doesn't update checksums for merged packets. If
  inputted packets are IP fragmented, GRO API assumes they are complete
  packets (i.e. with L4 headers).

* **Added support for generic flow API (rte_flow) on igb NIC.**

  This API provides a generic means to configure hardware to match specific
  ingress or egress traffic, alter its behavior and query related counters
  according to any number of user-defined rules.

  * Generic flow API support for Ethernet, IPv4, UDP, TCP and
    RAW pattern items with QUEUE actions. There are four
    type of filter support for this feature on igb.

* **Added Generic Flow API support to enic.**

  Flow API support for outer Ethernet, VLAN, IPv4, IPv6, UDP, TCP, SCTP, VxLAN
  and inner Ethernet, VLAN, IPv4, IPv6, UDP and TCP pattern items with QUEUE,
  MARK, FLAG and VOID actions for ingress traffic.

* **Added support for Chelsio T6 family of adapters**

  CXGBE PMD updated to run Chelsio T6 family of adapters.

* **Added latency and performance improvements for cxgbe**

  TX and RX path reworked to improve performance.  Also reduced latency
  for slow traffic.

* **Updated bnxt PMD.**

  Major enhancements include:
  * Support MTU modification.
  * Support LRO.
  * Support VLAN filter and strip functionality.
  * Other enhancements to add support for more dev_ops.
  * Add PMD specific APIs mainly to control VF from PF.
  * Update HWRM version to 1.7.7

* **Added support for Rx interrupts on mlx4 driver.**

  Rx queues can be armed with an interrupt which will trigger on the
  next packet arrival.

* **Updated szedata2 PMD.**

  Added support for firmwares with multiple Ethernet ports per physical port.

* **Reorganized the symmetric crypto operation structure.**

  The crypto operation (``rte_crypto_sym_op``) has been reorganized as follows:

  * Removed field ``rte_crypto_sym_op_sess_type``.
  * Replaced pointer and physical address of IV with offset from the start
    of the crypto operation.
  * Moved length and offset of cipher IV to ``rte_crypto_cipher_xform``.
  * Removed Additional Authentication Data (AAD) length.
  * Removed digest length.
  * Removed AAD pointer and physical address from ``auth`` structure.
  * Added ``aead`` structure, containing parameters for AEAD algorithms.

* **Reorganized the crypto operation structure.**

  The crypto operation (``rte_crypto_op``) has been reorganized as follows:

  * Added field ``rte_crypto_op_sess_type``.
  * Enumerations ``rte_crypto_op_status`` and ``rte_crypto_op_type``
    have been modified to be uint8_t values.
  * Removed the field ``opaque_data``.
  * Pointer to ``rte_crypto_sym_op`` has been replaced with a zero length array.

* **Reorganized the crypto symmetric session structure.**

  The crypto symmetric session structure (``rte_cryptodev_sym_session``) has
  been reorganized as follows:

  * ``dev_id`` field has been removed.
  * ``driver_id`` field has been removed.
  * Mempool pointer ``mp`` has been removed.
  * Replaced ``private`` marker with array of pointers to private data sessions
    ``sess_private_data``.

* **Updated cryptodev library.**

  * Added AEAD algorithm specific functions and structures, so it is not
    necessary to use a combination of cipher and authentication
    structures anymore.
  * Added helper functions for crypto device driver identification.
  * Added support for multi-device sessions, so a single session can be
    used in multiple drivers.
  * Added functions to initialize and free individual driver private data
    with a same session.

* **Updated dpaa2_sec crypto PMD.**

  Added support for AES-GCM and AES-CTR

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

    * 12-byte IV on AES Counter Mode, apart from the previous 16-byte IV.

* **Updated the AES-NI GCM PMD.**

  The AES-NI GCM PMD was migrated from the ISA-L library to the Multi Buffer
  library, as the latter library has Scatter Gather List support
  now. The migration entailed adding additional support for:

  * 192-bit key.

* **Updated the Cryptodev Scheduler PMD.**

  Added a multicore based distribution mode, which distributes the enqueued
  crypto operations among several slaves, running on different logical cores.

* **Added dpdk-test-eventdev test application.**

  The dpdk-test-eventdev tool is a Data Plane Development Kit (DPDK) application
  that allows exercising various eventdev use cases.
  This application has a generic framework to add new eventdev based test cases
  to verify functionality and measure the performance parameters of DPDK
  eventdev devices.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant
   sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past
     tense.

     The title should contain the code/lib section like a commit message.

     Add the entries in alphabetic order in the relevant sections below.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


EAL
~~~


Drivers
~~~~~~~


Libraries
~~~~~~~~~


Examples
~~~~~~~~


Other
~~~~~


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Moved bypass functions from the rte_ethdev library to ixgbe PMD**

  * The following rte_ethdev library functions were removed:

    * ``rte_eth_dev_bypass_event_show``
    * ``rte_eth_dev_bypass_event_store``
    * ``rte_eth_dev_bypass_init``
    * ``rte_eth_dev_bypass_state_set``
    * ``rte_eth_dev_bypass_state_show``
    * ``rte_eth_dev_bypass_ver_show``
    * ``rte_eth_dev_bypass_wd_reset``
    * ``rte_eth_dev_bypass_wd_timeout_show``
    * ``rte_eth_dev_wd_timeout_store``

  * The following ixgbe PMD functions were added:

    * ``rte_pmd_ixgbe_bypass_event_show``
    * ``rte_pmd_ixgbe_bypass_event_store``
    * ``rte_pmd_ixgbe_bypass_init``
    * ``rte_pmd_ixgbe_bypass_state_set``
    * ``rte_pmd_ixgbe_bypass_state_show``
    * ``rte_pmd_ixgbe_bypass_ver_show``
    * ``rte_pmd_ixgbe_bypass_wd_reset``
    * ``rte_pmd_ixgbe_bypass_wd_timeout_show``
    * ``rte_pmd_ixgbe_bypass_wd_timeout_store``

* **Reworked rte_cryptodev library.**

  The rte_cryptodev library has been reworked and updated. The following changes
  have been made to it:

  * The crypto device type enumeration has been removed from cryptodev library.
  * The function ``rte_crypto_count_devtype()`` has been removed, and replaced
    by the new function ``rte_crypto_count_by_driver()``.
  * Moved crypto device driver names definitions to the particular PMDs.
    These names are not public anymore.
  * ``rte_cryptodev_configure()`` does not create the session mempool
    for the device anymore.
  * ``rte_cryptodev_queue_pair_attach_sym_session()`` and
    ``rte_cryptodev_queue_pair_dettach_sym_session()`` functions require
    the new parameter ``device id``.
  * Modified parameters of ``rte_cryptodev_sym_session_create()``, to accept
    ``mempool``, instead of ``device id`` and ``rte_crypto_sym_xform``.
  * Remove ``device id`` parameter from ``rte_cryptodev_sym_session_free()``.
  * Added new field ``session_pool`` to ``rte_cryptodev_queue_pair_setup()``.
  * Removed ``aad_size`` parameter from ``rte_cryptodev_sym_capability_check_auth()``.
  * Added ``iv_size`` parameter to ``rte_cryptodev_sym_capability_check_auth()``.
  * Removed ``RTE_CRYPTO_OP_STATUS_ENQUEUED`` from enum ``rte_crypto_op_status``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Reorganized the crypto operation structures.**

  Some fields have been modified in the ``rte_crypto_op`` and ``rte_crypto_sym_op``
  structures, as described in the `New Features`_ section.

* **Reorganized the crypto symmetric session structure.**

  Some fields have been modified in the ``rte_cryptodev_sym_session``
  structure, as described in the `New Features`_ section.

* **Reorganized the ``rte_crypto_sym_cipher_xform`` structure.**

  * Added cipher IV length and offset parameters.
  * Changed field size of key length from size_t to uint16_t.

* **Reorganized the ``rte_crypto_sym_auth_xform`` structure.**

  * Added authentication IV length and offset parameters.
  * Changed field size of AAD length from uint32_t to uint16_t.
  * Changed field size of digest length from uint32_t to uint16_t.
  * Removed AAD length.
  * Changed field size of key length from size_t to uint16_t.

* Replaced ``dev_type`` enumeration with uint8_t ``driver_id`` in
  ``rte_cryptodev_info`` and  ``rte_cryptodev`` structures.

* Removed ``session_mp`` from ``rte_cryptodev_config``.


Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2

   This section is a comment. do not overwrite or remove it.
   =========================================================


The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bitratestats.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_cryptodev.so.3
     librte_distributor.so.1
     librte_eal.so.4
     librte_ethdev.so.6
   + librte_gro.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.3
     librte_mempool.so.2
     librte_meter.so.1
     librte_metrics.so.1
     librte_net.so.1
     librte_pdump.so.1
     librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested with this
   release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================
