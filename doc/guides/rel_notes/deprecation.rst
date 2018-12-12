..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* meson: The minimum supported version of meson for configuring and building
  DPDK will be increased to v0.47.1 (from 0.41) from DPDK 19.05 onwards. For
  those users with a version earlier than 0.47.1, an updated copy of meson
  can be got using the ``pip``, or ``pip3``, tool for downloading python
  packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: The ``attr_value`` parameter of ``rte_service_attr_get()``
  will be changed from ``uint32_t *`` to ``uint64_t *``
  as the attributes are of type ``uint64_t``.

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

* meter: New ``rte_color`` definition will be added in 19.02 and that will
  replace ``enum rte_meter_color`` in meter library in 19.05. This will help
  to consolidate color definition, which is currently replicated in many places,
  such as: rte_meter.h, rte_mtr.h, rte_tm.h.

* crypto/aesni_mb: the minimum supported intel-ipsec-mb library version will be
  changed from 0.49.0 to 0.52.0.
