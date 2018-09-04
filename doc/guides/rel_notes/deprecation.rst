..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* eal: certain structures will change in EAL on account of upcoming external
  memory support. Aside from internal changes leading to an ABI break, the
  following externally visible changes will also be implemented:

  - ``rte_memseg_list`` will change to include a boolean flag indicating
    whether a particular memseg list is externally allocated. This will have
    implications for any users of memseg-walk-related functions, as they will
    now have to skip externally allocated segments in most cases if the intent
    is to only iterate over internal DPDK memory.
  - ``socket_id`` parameter across the entire DPDK will gain additional meaning,
    as some socket ID's will now be representing externally allocated memory. No
    changes will be required for existing code as backwards compatibility will
    be kept, and those who do not use this feature will not see these extra
    socket ID's.

* eal: both declaring and identifying devices will be streamlined in v18.11.
  New functions will appear to query a specific port from buses, classes of
  device and device drivers. Device declaration will be made coherent with the
  new scheme of device identification.
  As such, ``rte_devargs`` device representation will change.

  - The enum ``rte_devtype`` was used to identify a bus and will disappear.
  - Functions previously deprecated will change or disappear:

    + ``rte_eal_devargs_type_count``

* pci: Several exposed functions are misnamed.
  The following functions are deprecated starting from v17.11 and are replaced:

  - ``eal_parse_pci_BDF`` replaced by ``rte_pci_addr_parse``
  - ``eal_parse_pci_DomBDF`` replaced by ``rte_pci_addr_parse``
  - ``rte_eal_compare_pci_addr`` replaced by ``rte_pci_addr_cmp``

* mbuf: The opaque ``mbuf->hash.sched`` field will be updated to support generic
  definition in line with the ethdev TM and MTR APIs. Currently, this field
  is defined in librte_sched in a non-generic way. The new generic format
  will contain: queue ID, traffic class, color. Field size will not change.

* mbuf: the macro ``RTE_MBUF_INDIRECT()`` will be removed in v18.08 or later and
  replaced with ``RTE_MBUF_CLONED()`` which is already added in v18.05. As
  ``EXT_ATTACHED_MBUF`` is newly introduced in v18.05, ``RTE_MBUF_INDIRECT()``
  can no longer be mutually exclusive with ``RTE_MBUF_DIRECT()`` if the new
  experimental API ``rte_pktmbuf_attach_extbuf()`` is used. Removal of the macro
  is to fix this semantic inconsistency.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* ethdev: In v18.11 ``rte_eth_dev_attach()`` and ``rte_eth_dev_detach()``
  will be removed.
  Hotplug functions ``rte_eal_hotplug_add()`` and ``rte_eal_hotplug_remove()``
  should be used instread.
  Function ``rte_eth_dev_get_port_by_name()`` may be used to find
  identifier of the added port.

* eal: In v18.11 ``rte_eal_dev_attach()`` and ``rte_eal_dev_detach()``
  will be removed.
  Hotplug functions ``rte_eal_hotplug_add()`` and ``rte_eal_hotplug_remove()``
  should be used directly.

* pdump: As we changed to use generic IPC, some changes in APIs and structure
  are expected in subsequent release.

  - ``rte_pdump_set_socket_dir`` will be removed;
  - The parameter, ``path``, of ``rte_pdump_init`` will be removed;
  - The enum ``rte_pdump_socktype`` will be removed.

* ethdev: flow API function ``rte_flow_copy()`` will be deprecated in v18.11
  in favor of ``rte_flow_conv()`` (which will appear in that version) and
  subsequently removed for v19.02.

  This is due to a lack of flexibility and reliance on a type unusable with
  C++ programs (struct rte_flow_desc).
