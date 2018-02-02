ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* eal: both declaring and identifying devices will be streamlined in v18.05.
  New functions will appear to query a specific port from buses, classes of
  device and device drivers. Device declaration will be made coherent with the
  new scheme of device identification.
  As such, ``rte_devargs`` device representation will change.

  - removal of ``name`` and ``args`` fields.
  - The enum ``rte_devtype`` was used to identify a bus and will disappear.
  - The ``rte_devargs_list`` will be made private.
  - Functions previously deprecated will change or disappear:

    + ``rte_eal_devargs_add``
    + ``rte_eal_devargs_type_count``
    + ``rte_eal_parse_devargs_str``, replaced by ``rte_eal_devargs_parse``
    + ``rte_eal_devargs_parse`` will change its format and use.
    + all ``rte_devargs`` related functions will be renamed, changing the
      ``rte_eal_devargs_`` prefix to ``rte_devargs_``.

* pci: Several exposed functions are misnamed.
  The following functions are deprecated starting from v17.11 and are replaced:

  - ``eal_parse_pci_BDF`` replaced by ``rte_pci_addr_parse``
  - ``eal_parse_pci_DomBDF`` replaced by ``rte_pci_addr_parse``
  - ``rte_eal_compare_pci_addr`` replaced by ``rte_pci_addr_cmp``

* eal: The semantics of the return value for the ``rte_lcore_has_role`` function
  are planned to change in v18.05. The function currently returns 0 and <0 for
  success and failure, respectively.  This will change to 1 and 0 for true and
  false, respectively, to make use of the function more intuitive.

* eal: new ``numa_node_count`` member will be added to ``rte_config`` structure
  in v18.05.

* eal: due to internal data layout reorganization, there will be changes to
  several structures and functions as a result of coming changes to support
  memory hotplug in v18.05.
  ``rte_eal_get_physmem_layout`` will be deprecated and removed in subsequent
  releases.
  ``rte_mem_config`` contents will change due to switch to memseg lists.
  ``rte_memzone`` member ``memseg_id`` will no longer serve any useful purpose
  and will be removed.

* eal: a new set of mbuf mempool ops name APIs for user, platform and best
  mempool names have been defined in ``rte_mbuf`` in v18.02. The uses of
  ``rte_eal_mbuf_default_mempool_ops`` shall be replaced by
  ``rte_mbuf_best_mempool_ops``.
  The following function is now redundant and it is target to be deprecated
  in 18.05:

  - ``rte_eal_mbuf_default_mempool_ops``

* ethdev: a new Tx and Rx offload API was introduced on 17.11.
  In the new API, offloads are divided into per-port and per-queue offloads.
  Offloads are disabled by default and enabled per application request.
  The old offloads API is target to be deprecated on 18.05. This includes:

  - removal of ``ETH_TXQ_FLAGS_NO*`` flags.
  - removal of ``txq_flags`` field from ``rte_eth_txconf`` struct.
  - removal of the offloads bit-field from ``rte_eth_rxmode`` struct.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* i40e: The default flexible payload configuration which extracts the first 16
  bytes of the payload for RSS will be deprecated starting from 18.02. If
  required the previous behavior can be configured using existing flow
  director APIs. There is no ABI/API break. This change will just remove a
  global configuration setting and require explicit configuration.

* librte_meter: The API will change to accommodate configuration profiles.
  Most of the API functions will have an additional opaque parameter.

* ring: The alignment constraints on the ring structure will be relaxed
  to one cache line instead of two, and an empty cache line padding will
  be added between the producer and consumer structures. The size of the
  structure and the offset of the fields will remain the same on
  platforms with 64B cache line, but will change on other platforms.
