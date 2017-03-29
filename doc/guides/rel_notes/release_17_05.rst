DPDK Release 17.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_17_05.html


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

* **Added free Tx mbuf on demand API.**

  Added a new function ``rte_eth_tx_done_cleanup()`` which allows an application
  to request the driver to release mbufs from their Tx ring that are no longer
  in use, independent of whether or not the ``tx_rs_thresh`` has been crossed.

* **Increased number of next hops for LPM IPv6 to 2^21.**

  The next_hop field is extended from 8 bits to 21 bits for IPv6.

* **Added powerpc support in pci probing for vfio-pci devices.**

  sPAPR IOMMU based pci probing enabled for vfio-pci devices.


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

* The LPM ``next_hop`` field is extended from 8 bits to 21 bits for IPv6
  while keeping ABI compatibility.

* **Reworked rte_ring library**

  The rte_ring library has been reworked and updated. The following changes
  have been made to it:

  * removed the build-time setting ``CONFIG_RTE_RING_SPLIT_PROD_CONS``
  * removed the build-time setting ``CONFIG_RTE_LIBRTE_RING_DEBUG``
  * removed the build-time setting ``CONFIG_RTE_RING_PAUSE_REP_COUNT``
  * removed the function ``rte_ring_set_water_mark`` as part of a general
    removal of watermarks support in the library.
  * added an extra parameter to the burst/bulk enqueue functions to
    return the number of free spaces in the ring after enqueue. This can
    be used by an application to implement its own watermark functionality.
  * added an extra parameter to the burst/bulk dequeue functions to return
    the number elements remaining in the ring after dequeue.
  * changed the return value of the enqueue and dequeue bulk functions to
    match that of the burst equivalents. In all cases, ring functions which
    operate on multiple packets now return the number of elements enqueued
    or dequeued, as appropriate. The updated functions are:

    - ``rte_ring_mp_enqueue_bulk``
    - ``rte_ring_sp_enqueue_bulk``
    - ``rte_ring_enqueue_bulk``
    - ``rte_ring_mc_dequeue_bulk``
    - ``rte_ring_sc_dequeue_bulk``
    - ``rte_ring_dequeue_bulk``

    NOTE: the above functions all have different parameters as well as
    different return values, due to the other listed changes above. This
    means that all instances of the functions in existing code will be
    flagged by the compiler. The return value usage should be checked
    while fixing the compiler error due to the extra parameter.

ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item in the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* KNI vhost support removed.


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
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_cryptodev.so.2
     librte_distributor.so.1
     librte_eal.so.3
     librte_ethdev.so.6
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
     librte_mempool.so.2
     librte_meter.so.1
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
