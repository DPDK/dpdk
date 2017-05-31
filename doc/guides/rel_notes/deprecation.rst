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

* ethdev: An API change is planned for 17.08 for the function
  ``_rte_eth_dev_callback_process``. In 17.08 the function will return an ``int``
  instead of ``void`` and a fourth parameter ``void *ret_param`` will be added.

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

* cryptodev: All PMD names definitions will be moved to the individual PMDs
  in 17.08.

* cryptodev: The following changes will be done in in 17.08:

  - the device type enumeration ``rte_cryptodev_type`` will be removed
  - the following structures will be changed: ``rte_cryptodev_session``,
    ``rte_cryptodev_sym_session``, ``rte_cryptodev_info``, ``rte_cryptodev``
  - the function ``rte_cryptodev_count_devtype`` will be replaced by
    ``rte_cryptodev_device_count_by_driver``

* cryptodev: API changes are planned for 17.08 for the sessions management
  to make it agnostic to the underlying devices, removing coupling with
  crypto PMDs, so a single session can be used on multiple devices.

  - ``struct rte_cryptodev_sym_session``, dev_id, dev_type will be removed,
    _private field changed to the indirect array of private data pointers of
    all supported devices

  An API of followed functions will be changed to allow operate on multiple
  devices with one session:

  - ``rte_cryptodev_sym_session_create``
  - ``rte_cryptodev_sym_session_free``
  - ``rte_cryptodev_sym_session_pool_create``

  While dev_id will not be stored in the ``struct rte_cryptodev_sym_session``,
  directly, the change of followed API is required:

  - ``rte_cryptodev_queue_pair_attach_sym_session``
  - ``rte_cryptodev_queue_pair_detach_sym_session``

* cryptodev: the structures ``rte_crypto_op``, ``rte_crypto_sym_op``
  and ``rte_crypto_sym_xform`` will be restructured in 17.08,
  for correctness and improvement.

* crypto/scheduler: the following two functions are deprecated starting
  from 17.05 and will be removed in 17.08:

  - ``rte_crpytodev_scheduler_mode_get``, replaced by ``rte_cryptodev_scheduler_mode_get``
  - ``rte_crpytodev_scheduler_mode_set``, replaced by ``rte_cryptodev_scheduler_mode_set``

* librte_table: The ``key_mask`` parameter will be added to all the hash tables
  that currently do not have it, as well as to the hash compute function prototype.
  The non-"do-sig" versions of the hash tables will be removed
  (including the ``signature_offset`` parameter)
  and the "do-sig" versions renamed accordingly.
