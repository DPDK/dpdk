ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* ring: Changes are planned to rte_ring APIs in release 17.05. Proposed
  changes include:

    - Removing build time options for the ring:
      CONFIG_RTE_RING_SPLIT_PROD_CONS
      CONFIG_RTE_RING_PAUSE_REP_COUNT
    - Adding an additional parameter to enqueue functions to return the
      amount of free space in the ring
    - Adding an additional parameter to dequeue functions to return the
      number of remaining elements in the ring
    - Removing direct support for watermarks in the rings, since the
      additional return value from the enqueue function makes it
      unneeded
    - Adjusting the return values of the bulk() enq/deq functions to
      make them consistent with the burst() equivalents. [Note, parameter
      to these functions are changing too, per points above, so compiler
      will flag them as needing update in legacy code]
    - Updates to some library functions e.g. rte_ring_get_memsize() to
      allow for variably-sized ring elements.

* igb_uio: iomem mapping and sysfs files created for iomem and ioport in
  igb_uio will be removed, because we are able to detect these from what Linux
  has exposed, like the way we have done with uio-pci-generic. This change
  targets release 17.05.

* The PCI and VDEV subsystems will be converted as drivers of the new bus model.
  It will imply some EAL API changes in 17.05.

* ``eth_driver`` is planned to be removed in 17.05. This currently serves as
  a placeholder for PMDs to register themselves. Changes for ``rte_bus`` will
  provide a way to handle device initialization currently being done in
  ``eth_driver``. Similarly, ``rte_pci_driver`` is planned to be removed from
  ``rte_cryptodev_driver`` in 17.05.

* ethdev: an API change is planned for 17.02 for the function
  ``_rte_eth_dev_callback_process``. In 17.02 the function will return an ``int``
  instead of ``void`` and a fourth parameter ``void *ret_param`` will be added.

* ABI changes are planned for 17.05 in the ``rte_mbuf`` structure: some fields
  may be reordered to facilitate the writing of ``data_off``, ``refcnt``, and
  ``nb_segs`` in one operation, because some platforms have an overhead if the
  store address is not naturally aligned. Other mbuf fields, such as the
  ``port`` field, may be moved or removed as part of this mbuf work. A
  ``timestamp`` will also be added.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 17.02 and will be removed in 17.05.

* mempool: The functions ``rte_mempool_count`` and ``rte_mempool_free_count``
  will be removed in 17.05.
  They are replaced by ``rte_mempool_avail_count`` and
  ``rte_mempool_in_use_count`` respectively.

* mempool: The functions for single/multi producer/consumer are deprecated
  and will be removed in 17.05.
  It is replaced by ``rte_mempool_generic_get/put`` functions.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* distributor: library API will be changed to incorporate a burst-oriented
  API. This will include a change to ``rte_distributor_create``
  to specify which type of instance to create (single or burst), and
  additional calls for ``rte_distributor_poll_pkt_burst`` and
  ``rte_distributor_return_pkt_burst``, among others.

* The architecture TILE-Gx and the associated mpipe driver are not
  maintained and will be removed in 17.05.
