..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`. API and ABI deprecation notices are to be posted
here.

Deprecation Notices
-------------------

* meson: The minimum supported version of meson for configuring and building
  DPDK will be increased to v0.47.1 (from 0.41) from DPDK 19.05 onwards. For
  those users with a version earlier than 0.47.1, an updated copy of meson
  can be got using the ``pip``, or ``pip3``, tool for downloading python
  packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: The function ``rte_eal_remote_launch`` will return new error codes
  after read or write error on the pipe, instead of calling ``rte_panic``.

* eal: both declaring and identifying devices will be streamlined in v18.11.
  New functions will appear to query a specific port from buses, classes of
  device and device drivers. Device declaration will be made coherent with the
  new scheme of device identification.
  As such, ``rte_devargs`` device representation will change.

  - The enum ``rte_devtype`` was used to identify a bus and will disappear.
  - Functions previously deprecated will change or disappear:

    + ``rte_eal_devargs_type_count``

* eal: The ``rte_logs`` struct and global symbol will be made private to
  remove it from the externally visible ABI and allow it to be updated in the
  future.

* dpaa2: removal of ``rte_dpaa2_memsegs`` structure which has been replaced
  by a pa-va search library. This structure was earlier being used for holding
  memory segments used by dpaa2 driver for faster pa->va translation. This
  structure would be made internal (or removed if all dependencies are cleared)
  in future releases.

* mempool: starting from v20.05, the API of rte_mempool_populate_iova()
  and rte_mempool_populate_virt() will change to return 0 instead
  of -EINVAL when there is not enough room to store one object. The ABI
  will be preserved until 20.11.

* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  Target release for removal of the legacy API will be defined once most
  PMDs have switched to rte_flow.

* ethdev: Update API functions returning ``void`` to return ``int`` with
  negative errno values to indicate various error conditions (e.g.
  invalid port ID, unsupported operation, failed operation):

  - ``rte_eth_dev_stop``
  - ``rte_eth_dev_close``

* ethdev: New offload flags ``DEV_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::fdir``.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the field even when the offload is not
  enabled.

* cryptodev: support for using IV with all sizes is added, J0 still can
  be used but only when IV length in following structs ``rte_crypto_auth_xform``,
  ``rte_crypto_aead_xform`` is set to zero. When IV length is greater or equal
  to one it means it represents IV, when is set to zero it means J0 is used
  directly, in this case 16 bytes of J0 need to be passed.

* sched: To allow more traffic classes, flexible mapping of pipe queues to
  traffic classes, and subport level configuration of pipes and queues
  changes will be made to macros, data structures and API functions defined
  in "rte_sched.h". These changes are aligned to improvements suggested in the
  RFC https://mails.dpdk.org/archives/dev/2018-November/120035.html.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* power: ``rte_power_set_env`` function will no longer return 0 on attempt
  to set new power environment if power environment was already initialized.
  In this case the function will return -1 unless the environment is unset first
  (using ``rte_power_unset_env``). Other function usage scenarios will not change.
