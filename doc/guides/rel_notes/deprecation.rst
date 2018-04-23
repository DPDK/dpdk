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

* eal: a new set of mbuf mempool ops name APIs for user, platform and best
  mempool names have been defined in ``rte_mbuf`` in v18.02. The uses of
  ``rte_eal_mbuf_default_mempool_ops`` shall be replaced by
  ``rte_mbuf_best_mempool_ops``.
  The following function is now redundant and it is target to be deprecated
  in 18.05:

  - ``rte_eal_mbuf_default_mempool_ops``

* mempool: several API and ABI changes are planned in v18.05.

  The following changes are planned:

  - addition of new op to allocate contiguous
    block of objects if underlying driver supports it.

* mbuf: The opaque ``mbuf->hash.sched`` field will be updated to support generic
  definition in line with the ethdev TM and MTR APIs. Currently, this field
  is defined in librte_sched in a non-generic way. The new generic format
  will contain: queue ID, traffic class, color. Field size will not change.

* ethdev: a new Tx and Rx offload API was introduced on 17.11.
  In the new API, offloads are divided into per-port and per-queue offloads.
  Offloads are disabled by default and enabled per application request.

  The old ethdev - drivers offload interface will be deprecated on 18.05.
  This includes:
  - removal of the conversion in ethdev from new offloading API to old API for drivers.

  In later releases the old offloading API will be deprecated, which will include:
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

* ethdev: A new rss level field planned in 18.05.
  The new API add rss_level field to ``rte_eth_rss_conf`` to enable a choice
  of RSS hash calculation on outer or inner header of tunneled packet.

* ethdev: A work is being planned for 18.05 to expose VF port representors
  as a mean to perform control and data path operation on the different VFs.
  As VF representor is an ethdev port, new fields are needed in order to map
  between the VF representor and the VF or the parent PF. Those new fields
  are to be included in ``rte_eth_dev_info`` struct.

* i40e: The default flexible payload configuration which extracts the first 16
  bytes of the payload for RSS will be deprecated starting from 18.02. If
  required the previous behavior can be configured using existing flow
  director APIs. There is no ABI/API break. This change will just remove a
  global configuration setting and require explicit configuration.

* pdump: As we changed to use generic IPC, some changes in APIs and structure
  are expected in subsequent release.

  - ``rte_pdump_set_socket_dir`` will be removed;
  - The parameter, ``path``, of ``rte_pdump_init`` will be removed;
  - The enum ``rte_pdump_socktype`` will be removed.
