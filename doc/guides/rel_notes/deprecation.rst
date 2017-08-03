ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* eal: the following functions are deprecated starting from 17.05 and will
  be removed in 17.11:

  - ``rte_set_log_level``, replaced by ``rte_log_set_global_level``
  - ``rte_get_log_level``, replaced by ``rte_log_get_global_level``
  - ``rte_set_log_type``, replaced by ``rte_log_set_level``
  - ``rte_get_log_type``, replaced by ``rte_log_get_level``

* eal: several API and ABI changes are planned for ``rte_devargs`` in v17.11.
  The format of device command line parameters will change. The bus will need
  to be explicitly stated in the device declaration. The enum ``rte_devtype``
  was used to identify a bus and will disappear.
  The structure ``rte_devargs`` will change.
  The ``rte_devargs_list`` will be made private.
  The following functions are deprecated starting from 17.08 and will either be
  modified or removed in 17.11:

  - ``rte_eal_devargs_add``
  - ``rte_eal_devargs_type_count``
  - ``rte_eal_parse_devargs_str``, replaced by ``rte_eal_devargs_parse``

* eal: the support of Xen dom0 will be removed from EAL in 17.11; and with
  that, drivers/net/xenvirt and examples/vhost_xen will also be removed.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 17.08 and will be removed in 17.11.

* ethdev: Tx offloads will no longer be enabled by default in 17.11.
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

* cryptodev: the following function is deprecated starting from 17.08 and will
  be removed in 17.11:

  - ``rte_cryptodev_create_vdev``
