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

* vfio: Some functions are planned to be exported outside librte_eal in 17.05.
  VFIO APIs like ``vfio_setup_device``, ``vfio_get_group_fd`` can be used by
  subsystem other than EAL/PCI. For that, these need to be exported symbols.
  Such APIs are planned to be renamed according to ``rte_*`` naming convention
  and exported from librte_eal.

* The PCI and VDEV subsystems will be converted as drivers of the new bus model.
  It will imply some EAL API changes in 17.05.

* ``eth_driver`` is planned to be removed in 17.05. This currently serves as
  a placeholder for PMDs to register themselves. Changes for ``rte_bus`` will
  provide a way to handle device initialization currently being done in
  ``eth_driver``. Similarly, ``rte_pci_driver`` is planned to be removed from
  ``rte_cryptodev_driver`` in 17.05.

* ethdev: An API change is planned for 17.05 for the function
  ``_rte_eth_dev_callback_process``. In 17.05 the function will return an ``int``
  instead of ``void`` and a fourth parameter ``void *ret_param`` will be added.

* ethdev: for 17.05 it is planned to deprecate the following nine rte_eth_dev_*
  functions and move them into the ixgbe PMD:

  ``rte_eth_dev_bypass_init``, ``rte_eth_dev_bypass_state_set``,
  ``rte_eth_dev_bypass_state_show``, ``rte_eth_dev_bypass_event_store``,
  ``rte_eth_dev_bypass_event_show``, ``rte_eth_dev_wd_timeout_store``,
  ``rte_eth_dev_bypass_wd_timeout_show``, ``rte_eth_dev_bypass_ver_show``,
  ``rte_eth_dev_bypass_wd_reset``.

  The following fields will be removed from ``struct eth_dev_ops``:

  ``bypass_init_t``, ``bypass_state_set_t``, ``bypass_state_show_t``,
  ``bypass_event_set_t``, ``bypass_event_show_t``, ``bypass_wd_timeout_set_t``,
  ``bypass_wd_timeout_show_t``, ``bypass_ver_show_t``, ``bypass_wd_reset_t``.

  The functions will be renamed to the following, and moved to the ``ixgbe`` PMD:

  ``rte_pmd_ixgbe_bypass_init``, ``rte_pmd_ixgbe_bypass_state_set``,
  ``rte_pmd_ixgbe_bypass_state_show``, ``rte_pmd_ixgbe_bypass_event_set``,
  ``rte_pmd_ixgbe_bypass_event_show``, ``rte_pmd_ixgbe_bypass_wd_timeout_set``,
  ``rte_pmd_ixgbe_bypass_wd_timeout_show``, ``rte_pmd_ixgbe_bypass_ver_show``,
  ``rte_pmd_ixgbe_bypass_wd_reset``.

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

* vhost: API/ABI changes are planned for 17.05, for making DPDK vhost library
  generic enough so that applications can build different vhost-user drivers
  (instead of vhost-user net only) on top of that.
  Specifically, ``virtio_net_device_ops`` will be renamed to ``vhost_device_ops``.
  Correspondingly, some API's parameter need be changed. Few more functions also
  need be reworked to let it be device aware. For example, different virtio device
  has different feature set, meaning functions like ``rte_vhost_feature_disable``
  need be changed. Last, file rte_virtio_net.h will be renamed to rte_vhost.h.

* kni: Remove :ref:`kni_vhost_backend-label` feature (KNI_VHOST) in 17.05 release.
  :doc:`Vhost Library </prog_guide/vhost_lib>` is currently preferred method for
  guest - host communication. Just for clarification, this is not to remove KNI
  or VHOST feature, but KNI_VHOST which is a KNI feature enabled via a compile
  time option, and disabled by default.

* ABI changes are planned for 17.05 in the ``rte_cryptodev_ops`` structure.
  A pointer to a rte_cryptodev_config structure will be added to the
  function prototype ``cryptodev_configure_t``, as a new parameter.

* cryptodev: A new parameter ``max_nb_sessions_per_qp`` will be added to
  ``rte_cryptodev_info.sym``. Some drivers may support limited number of
  sessions per queue_pair. With this new parameter application will know
  how many sessions can be mapped to each queue_pair of a device.

* distributor: library API will be changed to incorporate a burst-oriented
  API. This will include a change to ``rte_distributor_create``
  to specify which type of instance to create (single or burst), and
  additional calls for ``rte_distributor_poll_pkt_burst`` and
  ``rte_distributor_return_pkt_burst``, among others.

* The architecture TILE-Gx and the associated mpipe driver are not
  maintained and will be removed in 17.05.
