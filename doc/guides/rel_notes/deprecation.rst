ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

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

* pci: Several exposed functions are misnamed.
  The following functions are deprecated starting from v17.11 and are replaced:

  - ``eal_parse_pci_BDF`` replaced by ``rte_pci_addr_parse``
  - ``eal_parse_pci_DomBDF`` replaced by ``rte_pci_addr_parse``
  - ``rte_eal_compare_pci_addr`` replaced by ``rte_pci_addr_cmp``

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

* ethdev: new parameters - ``rte_security_capabilities`` and
  ``rte_security_ops`` will be added to ``rte_eth_dev_info`` and
  ``rte_eth_dev`` respectively  to support security operations like
  ipsec inline.

* cryptodev: new parameters - ``rte_security_capabilities`` and
  ``rte_security_ops`` will be added to ``rte_cryptodev_info`` and
  ``rte_cryptodev`` respectively to support security protocol offloaded
  operations.

* librte_meter: The API will change to accommodate configuration profiles.
  Most of the API functions will have an additional opaque parameter.
