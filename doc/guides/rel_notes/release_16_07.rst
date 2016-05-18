DPDK Release 16.07
==================

.. **Read this first.**

   The text below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text: ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      firefox build/doc/html/guides/rel_notes/release_16_07.html


New Features
------------

.. This section should contain new features added in this release. Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to understand
     the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like this.

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

* **Removed mempool cache if not needed.**

  The size of the mempool structure is reduced if the per-lcore cache is disabled.

* **Changed the memory allocation in mempool library.**

  * Added ability to allocate a large mempool in virtually fragmented memory.
  * Added new APIs to populate a mempool with memory.
  * Added an API to free a mempool.
  * Modified the API of rte_mempool_obj_iter() function.
  * Dropped specific Xen Dom0 code.
  * Dropped specific anonymous mempool code in testpmd.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past tense.
     The title should contain the code/lib section like a commit message.
     Add the entries in alphabetic order in the relevant sections below.


EAL
~~~


Drivers
~~~~~~~

* **i40e: Fixed vlan stripping from inner header.**

  Previously, for tunnel packets, such as VXLAN/NVGRE, the vlan
  tags of the inner header will be stripped without putting vlan
  info to descriptor.
  Now this issue is fixed by disabling vlan stripping from inner header.


Libraries
~~~~~~~~~

* **mbuf: Fixed refcnt update when detaching.**

  Fix the ``rte_pktmbuf_detach()`` function to decrement the direct
  mbuf's reference counter. The previous behavior was not to affect
  the reference counter. It lead a memory leak of the direct mbuf.


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


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

* The following counters are removed from ``rte_eth_stats`` structure:
  ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff.


ABI Changes
-----------

.. * Add a short 1-2 sentence description of the ABI change that was announced in
     the previous releases and made in this release. Use fixed width quotes for
     ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

* The ``rte_port_source_params`` structure has new fields to support PCAP file.
  It was already in release 16.04 with ``RTE_NEXT_ABI`` flag.


Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+`` sign.

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.4
     librte_acl.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_distributor.so.1
     librte_eal.so.2
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
   + librte_mempool.so.2
     librte_meter.so.1
     librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
   + librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.2


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested with this
   release.

   The format is:

   #. Platform name.

      - Platform details.
      - Platform details.


Tested NICs
-----------

.. This section should contain a list of NICs that were tested with this release.

   The format is:

   #. NIC name.

      - NIC details.
      - NIC details.
