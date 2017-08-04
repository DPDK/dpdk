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

* eal: An ABI change is planned for 17.11 to make DPDK aware of IOVA address
  translation scheme.
  Reference to phys address in EAL data-structure or functions may change to
  IOVA address or more appropriate name.
  The change will be only for the name.
  Functional aspects of the API or data-structure will remain same.

* The mbuf flags PKT_RX_VLAN_PKT and PKT_RX_QINQ_PKT are deprecated and
  are respectively replaced by PKT_RX_VLAN_STRIPPED and
  PKT_RX_QINQ_STRIPPED, that are better described. The old flags and
  their behavior will be kept until 17.08 and will be removed in 17.11.

* mempool: The following will be modified in 17.11:

  - ``rte_mempool_xmem_size`` and ``rte_mempool_xmem_usage`` need to know
    the mempool flag status so adding new param rte_mempool in those API.
  - Removing __rte_unused int flag param from ``rte_mempool_generic_put``
    and ``rte_mempool_generic_get`` API.
  - ``rte_mempool`` flags data type will changed from int to
    unsigned int.

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

* ethdev: The device flag advertizing hotplug capability
  ``RTE_ETH_DEV_DETACHABLE`` is not needed anymore and will be removed in
  v17.11.
  This capability is verified upon calling the relevant hotplug functions in EAL
  by checking that the ``unplug`` ops is set in the bus. This verification is
  done by the EAL and not by the ``ethdev`` layer anymore. Users relying on this
  flag being present only have to remove their checks to follow the change.

* ABI/API changes are planned for 17.11 in all structures which include port_id
  definition such as "rte_eth_dev_data", "rte_port_ethdev_reader_params",
  "rte_port_ethdev_writer_params", and so on. The definition of port_id will be
  changed from 8 bits to 16 bits in order to support more than 256 ports in
  DPDK. All APIs which have port_id parameter will be changed at the same time.

* ethdev: An ABI change is planned for 17.11 for the structure rte_eth_dev_data.
  The size of the unique name will increase RTE_ETH_NAME_MAX_LEN from 32 to
  64 characters to allow using a globally unique identifier (GUID) in this field.

* ethdev: new parameters - ``rte_security_capabilities`` and
  ``rte_security_ops`` will be added to ``rte_eth_dev_info`` and
  ``rte_eth_dev`` respectively  to support security operations like
  ipsec inline.

* cryptodev: new parameters - ``rte_security_capabilities`` and
  ``rte_security_ops`` will be added to ``rte_cryptodev_info`` and
  ``rte_cryptodev`` respectively to support security protocol offloaded
  operations.

* cryptodev: the following function is deprecated starting from 17.08 and will
  be removed in 17.11:

  - ``rte_cryptodev_create_vdev``

* cryptodev: the following function will be static in 17.11 and included
  by all crypto drivers, therefore, will not be public:

  - ``rte_cryptodev_vdev_pmd_init``

* cryptodev: the following function will have an extra parameter, passing a
  statically allocated crypto driver structure, instead of calling malloc,
  in 17.11:

  - ``rte_cryptodev_allocate_driver``

* librte_meter: The API will change to accommodate configuration profiles.
  Most of the API functions will have an additional opaque parameter.

* librte_table: The ``key_mask`` parameter will be added to all the hash tables
  that currently do not have it, as well as to the hash compute function prototype.
  The non-"do-sig" versions of the hash tables will be removed
  (including the ``signature_offset`` parameter)
  and the "do-sig" versions renamed accordingly.
