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

* **Reorganized the crypto operation structure.**

  The crypto operation (``rte_crypto_op``) has been reorganized as follows:

  * Added field ``rte_crypto_op_sess_type``.
  * Enumerations ``rte_crypto_op_status`` and ``rte_crypto_op_type``
    have been modified to be uint8_t values.
  * Removed the field ``opaque_data``.
  * Pointer to ``rte_crypto_sym_op`` has been replaced with a zero length array.


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

* **Reorganized the ``rte_crypto_sym_cipher_xform`` structure.**

  * Added cipher IV length and offset parameters.

* **Reorganized the ``rte_crypto_sym_auth_xform`` structure.**

  * Added authentication IV length and offset parameters.
  * Changed field size of AAD length from uint32_t to uint16_t.
  * Changed field size of digest length from uint32_t to uint16_t.


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
