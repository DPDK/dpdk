..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* linux: Linux kernel version 3.2 (which is the current minimum required
  version for the DPDK) is not maintained anymore. Therefore the planned
  minimum required kernel version for DPDK 19.02 will be the next oldest
  Long Term Stable (LTS) version which is 3.16, but compatibility for
  recent distribution kernels will be kept.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: function ``rte_bsf64`` in ``rte_bitmap.h`` has been renamed to
  ``rte_bsf64_safe`` and moved to ``rte_common.h``. A new ``rte_bsf64`` function
  will be added in the next release in ``rte_common.h`` that follows convention
  set by existing ``rte_bsf32`` function.

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

* dpaa2: removal of ``rte_dpaa2_memsegs`` structure which has been replaced
  by a pa-va search library. This structure was earlier being used for holding
  memory segments used by dpaa2 driver for faster pa->va translation. This
  structure would be made internal (or removed if all dependencies are cleared)
  in future releases.

* mbuf: The opaque ``mbuf->hash.sched`` field will be updated to support generic
  definition in line with the ethdev TM and MTR APIs. Currently, this field
  is defined in librte_sched in a non-generic way. The new generic format
  will contain: queue ID, traffic class, color. Field size will not change.

* sched: Some API functions will change prototype due to the above
  deprecation note for mbuf->hash.sched, e.g. ``rte_sched_port_pkt_write()``
  and ``rte_sched_port_pkt_read()`` will likely have an additional parameter
  of type ``struct rte_sched_port``.

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

* ethdev: Maximum and minimum MTU values vary between hardware devices. In
  hardware agnostic DPDK applications access to such information would allow
  a more accurate way of validating and setting supported MTU values on a per
  device basis rather than using a defined default for all devices. To
  resolve this, the following members will be added to ``rte_eth_dev_info``.
  Note: these can be added to fit a hole in the existing structure for amd64
  but not for 32-bit, as such ABI change will occur as size of the structure
  will increase.

  - Member ``uint16_t min_mtu`` the minimum MTU allowed.
  - Member ``uint16_t max_mtu`` the maximum MTU allowed.

* security: New field ``uint64_t opaque_data`` is planned to be added into
  ``rte_security_session`` structure. That would allow upper layer to easily
  associate/de-associate some user defined data with the security session.

* cryptodev: several API and ABI changes are planned for rte_cryptodev
  in v19.02:

  - The size and layout of ``rte_cryptodev_sym_session`` will change
    to fix existing issues.
  - The size and layout of ``rte_cryptodev_qp_conf`` and syntax of
    ``rte_cryptodev_queue_pair_setup`` will change to to allow to use
    two different mempools for crypto and device private sessions.

* pdump: As we changed to use generic IPC, some changes in APIs and structure
  are expected in subsequent release.

  - ``rte_pdump_set_socket_dir`` will be removed;
  - The parameter, ``path``, of ``rte_pdump_init`` will be removed;
  - The enum ``rte_pdump_socktype`` will be removed.
