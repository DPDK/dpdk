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

* **Added mailbox interrupt support for ixgbe VF.**

  When the physical NIC link comes down or up, the PF driver will send a
  mailbox message to notify each VF. To handle this link up/down event,
  add mailbox interrupts support to receive the message and allow the app to
  register a callback for it.

* **Updated the i40e base driver.**

  Updated the i40e base driver, which includes support for new devices IDs.

* **Supported virtio on IBM POWER8.**

  The ioports are mapped in memory when using Linux UIO.

* **Virtio support for containers.**

  Add a new virtual device, named virtio-user, to support virtio for containers.

  Known limitations:

  * Control queue and multi-queue are not supported yet.
  * Cannot work with --huge-unlink.
  * Cannot work with --no-huge.
  * Cannot work when there are more than VHOST_MEMORY_MAX_NREGIONS(8) hugepages.
  * Root privilege is a must for sorting hugepages by physical address.
  * Can only be used with vhost user backend.

* **Added vhost-user client mode.**

  DPDK vhost-user could be the server as well as the client. It supports
  server mode only before, now it also supports client mode. Client mode
  is enabled when ``RTE_VHOST_USER_CLIENT`` flag is set while calling
  ``rte_vhost_driver_register``.

  When DPDK vhost-user restarts from normal or abnormal quit (say crash),
  the client mode would allow DPDK to establish the connect again.  Note
  that a brand new QEMU version (v2.7 or above) is needed, otherwise, the
  reconnect won't work.

  DPDK vhost-user will also try to reconnect by default when

  * the first connect fails (when QEMU is not started yet)
  * the connection is broken (when QEMU restarts)

  It can be turned off if flag ``RTE_VHOST_USER_NO_RECONNECT`` is set.

* **Added NSH packet recognition in i40e.**

* **Added AES-CTR support to AESNI MB PMD.**

  Now AESNI MB PMD supports 128/192/256-bit counter mode AES encryption and
  decryption.

* **Added support of AES counter mode for Intel QuickAssist devices.**

  Enabled support for the AES CTR algorithm for Intel QuickAssist devices.
  Provided support for algorithm-chaining operations.

* **Added KASUMI SW PMD.**

  A new Crypto PMD has been added, which provides KASUMI F8 (UEA1) ciphering
  and KASUMI F9 (UIA1) hashing.

* **Added multi-writer support for RTE Hash with Intel TSX.**

  The following features/modifications have been added to rte_hash library:

  * Enabled application developers to use an extra flag for rte_hash creation
    to specify default behavior (multi-thread safe/unsafe) with rte_hash_add_key
    function.
  * Changed Cuckoo search algorithm to breadth first search for multi-writer
    routine and split Cuckoo Search and Move operations in order to reduce
    transactional code region and improve TSX performance.
  * Added a hash multi-writer test case for test app.

* **Improved IP Pipeline Application.**

  The following features have been added to ip_pipeline application:

  * Configure the MAC address in the routing pipeline and automatic routes
    updates with change in link state.
  * Enable RSS per network interface through the configuration file.
  * Streamline the CLI code.

* **Added keepalive enhancements.**

  Adds support for reporting of core states other than dead to
  monitoring applications, enabling the support of broader liveness
  reporting to external processes.

* **Added packet capture framework.**

  * A new library ``librte_pdump`` is added to provide packet capture API.
  * A new ``app/pdump`` tool is added to capture packets in DPDK.


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

* The extended statistics are fetched by ids with ``rte_eth_xstats_get``
  after a lookup by name ``rte_eth_xstats_get_names``.

* The function ``rte_eth_dev_info_get`` fill the new fields ``nb_rx_queues``
  and ``nb_tx_queues`` in the structure ``rte_eth_dev_info``.

* The vhost function ``rte_vring_available_entries`` is renamed to
  ``rte_vhost_avail_entries``.

* All existing vhost APIs and callbacks with ``virtio_net`` struct pointer
  as the parameter have been changed due to the ABI refactoring mentioned
  below: it's replaced by ``int vid``.

* The function ``rte_vhost_enqueue_burst`` no longer supports concurrent enqueuing
  packets to the same queue.

* The function ``rte_eth_dev_set_mtu`` adds a new return value ``-EBUSY``, which
  indicates the operation is forbidden because the port is running.


ABI Changes
-----------

.. * Add a short 1-2 sentence description of the ABI change that was announced in
     the previous releases and made in this release. Use fixed width quotes for
     ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

* The ``rte_port_source_params`` structure has new fields to support PCAP file.
  It was already in release 16.04 with ``RTE_NEXT_ABI`` flag.

* The ``rte_eth_dev_info`` structure has new fields ``nb_rx_queues`` and ``nb_tx_queues``
  to support number of queues configured by software.

* vhost ABI refactoring has been made: ``virtio_net`` structure is never
  exported to application any more. Instead, a handle, ``vid``, has been
  used to represent this structure internally.


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
   + librte_vhost.so.3


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
