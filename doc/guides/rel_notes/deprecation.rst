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

* igb_uio: iomem mapping and sysfs files created for iomem and ioport in
  igb_uio will be removed, because we are able to detect these from what Linux
  has exposed, like the way we have done with uio-pci-generic. This change
  targets release 17.05.

* vfio: Some functions are planned to be exported outside librte_eal in 17.05.
  VFIO APIs like ``vfio_setup_device``, ``vfio_get_group_fd`` can be used by
  subsystem other than EAL/PCI. For that, these need to be exported symbols.
  Such APIs are planned to be renamed according to ``rte_*`` naming convention
  and exported from librte_eal.

* The VDEV subsystem will be converted as driver of the new bus model.
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

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 17.02 and will be removed in 17.05.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* crypto/scheduler: the following two functions are deprecated starting
  from 17.05 and will be removed in 17.08:

  - ``rte_crpytodev_scheduler_mode_get``, replaced by ``rte_cryptodev_scheduler_mode_get``
  - ``rte_crpytodev_scheduler_mode_set``, replaced by ``rte_cryptodev_scheduler_mode_set``
