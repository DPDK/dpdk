ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* eal: the following functions are deprecated starting from 17.05 and will
  be removed in 17.08:

  - ``rte_set_log_level``, replaced by ``rte_log_set_global_level``
  - ``rte_get_log_level``, replaced by ``rte_log_get_global_level``
  - ``rte_set_log_type``, replaced by ``rte_log_set_level``
  - ``rte_get_log_type``, replaced by ``rte_log_get_level``

* devargs: An ABI change is planned for 17.08 for the structure ``rte_devargs``.
  The current version is dependent on bus-specific device identifier, which will
  be made generic and abstracted, in order to make the EAL bus-agnostic.

  Accompanying this evolution, device command line parameters will thus support
  explicit bus definition in a device declaration.

* igb_uio: iomem mapping and sysfs files created for iomem and ioport in
  igb_uio will be removed, because we are able to detect these from what Linux
  has exposed, like the way we have done with uio-pci-generic. This change
  targets release 17.05.

* The VDEV subsystem will be converted as driver of the new bus model.
  It may imply some EAL API changes in 17.08.

* The struct ``rte_pci_driver`` is planned to be removed from
  ``rte_cryptodev_driver`` and ``rte_eventdev_driver`` in 17.08.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 17.05 and will be removed in 17.08.

* ethdev: Tx offloads will no longer be enabled by default in 17.08.
  Instead, the ``rte_eth_txmode`` structure will be extended with
  bit field to enable each Tx offload.
  Besides of making the Rx/Tx configuration API more consistent for the
  application, PMDs will be able to provide a better out of the box performance.
  As part of the work, ``ETH_TXQ_FLAGS_NO*`` will be superseded as well.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* librte_table: The ``key_mask`` parameter will be added to all the hash tables
  that currently do not have it, as well as to the hash compute function prototype.
  The non-"do-sig" versions of the hash tables will be removed
  (including the ``signature_offset`` parameter)
  and the "do-sig" versions renamed accordingly.
