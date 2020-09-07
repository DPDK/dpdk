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
  can be got using the ``pip3`` tool (or ``python3 -m pip``) for downloading
  python packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: To be more inclusive in choice of naming, the DPDK project
  will replace uses of master/slave in the API's and command line arguments.

  References to master/slave in relation to lcore will be renamed
  to initial/worker.  The function ``rte_get_master_lcore()``
  will be renamed to ``rte_get_initial_lcore()``.
  For the 20.11 release, both names will be present and the
  old function will be marked with the deprecated tag.
  The old function will be removed in a future version.

  The iterator for worker lcores will also change:
  ``RTE_LCORE_FOREACH_SLAVE`` will be replaced with
  ``RTE_LCORE_FOREACH_WORKER``.

  The ``master-lcore`` argument to testpmd will be replaced
  with ``initial-lcore``. The old ``master-lcore`` argument
  will produce a runtime notification in 20.11 release, and
  be removed completely in a future release.

* eal: The terms blacklist and whitelist to describe devices used
  by DPDK will be replaced in the 20.11 relase.
  This will apply to command line arguments as well as macros.

  The macro ``RTE_DEV_BLACKLISTED`` will be replaced with ``RTE_DEV_EXCLUDED``
  and ``RTE_DEV_WHITELISTED`` will be replaced with ``RTE_DEV_INCLUDED``
  ``RTE_BUS_SCAN_BLACKLIST`` and ``RTE_BUS_SCAN_WHITELIST`` will be
  replaced with ``RTE_BUS_SCAN_EXCLUDED`` and ``RTE_BUS_SCAN_INCLUDED``
  respectively. Likewise ``RTE_DEVTYPE_BLACKLISTED_PCI`` and
  ``RTE_DEVTYPE_WHITELISTED_PCI`` will be replaced with
  ``RTE_DEVTYPE_EXCLUDED`` and ``RTE_DEVTYPE_INCLUDED``.

  The old macros will be marked as deprecated in 20.11 and any
  usage will cause a compile warning. They will be removed in
  a future release.

  The command line arguments to ``rte_eal_init`` will change from
  ``-b, --pci-blacklist`` to ``-x, --exclude`` and
  ``-w, --pci-whitelist`` to ``-i, --include``.
  The old command line arguments will continue to be accepted in 20.11
  but will cause a runtime warning message. The old arguments will
  be removed in a future release.

* eal: The function ``rte_eal_remote_launch`` will return new error codes
  after read or write error on the pipe, instead of calling ``rte_panic``.

* rte_atomicNN_xxx: These APIs do not take memory order parameter. This does
  not allow for writing optimized code for all the CPU architectures supported
  in DPDK. DPDK will adopt C11 atomic operations semantics and provide wrappers
  using C11 atomic built-ins. These wrappers must be used for patches that
  need to be merged in 20.08 onwards. This change will not introduce any
  performance degradation.

* rte_smp_*mb: These APIs provide full barrier functionality. However, many
  use cases do not require full barriers. To support such use cases, DPDK will
  adopt C11 barrier semantics and provide wrappers using C11 atomic built-ins.
  These wrappers must be used for patches that need to be merged in 20.08
  onwards. This change will not introduce any performance degradation.

* lib: will fix extending some enum/define breaking the ABI. There are multiple
  samples in DPDK that enum/define terminated with a ``.*MAX.*`` value which is
  used by iterators, and arrays holding these values are sized with this
  ``.*MAX.*`` value. So extending this enum/define increases the ``.*MAX.*``
  value which increases the size of the array and depending on how/where the
  array is used this may break the ABI.
  ``RTE_ETH_FLOW_MAX`` is one sample of the mentioned case, adding a new flow
  type will break the ABI because of ``flex_mask[RTE_ETH_FLOW_MAX]`` array
  usage in following public struct hierarchy:
  ``rte_eth_fdir_flex_conf -> rte_fdir_conf -> rte_eth_conf (in the middle)``.
  Need to identify this kind of usages and fix in 20.11, otherwise this blocks
  us extending existing enum/define.
  One solution can be using a fixed size array instead of ``.*MAX.*`` value.

* mbuf: Some fields will be converted to dynamic API in DPDK 20.11
  in order to reserve more space for the dynamic fields, as explained in
  `this presentation <https://www.youtube.com/watch?v=Ttl6MlhmzWY>`_.
  The following static fields will be moved as dynamic:

  - ``timestamp``
  - ``userdata`` / ``udata64``
  - ``seqn``

  As a consequence, the layout of the ``struct rte_mbuf`` will be re-arranged,
  avoiding impact on vectorized implementation of the driver datapaths,
  while evaluating performance gains of a better use of the first cache line.


* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types MACVLAN, ETHERTYPE, FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
  HASH and L2_TUNNEL, is superseded by the generic flow API (rte_flow) in
  PMDs that implement the latter.
  The legacy API will be removed in DPDK 20.11.

* ethdev: The flow director API, including ``rte_eth_conf.fdir_conf`` field,
  and the related structures (``rte_fdir_*`` and ``rte_eth_fdir_*``),
  will be removed in DPDK 20.11.

* ethdev: The legacy L2 tunnel filtering API is deprecated as the rest of
  the legacy filtering API.
  The functions ``rte_eth_dev_l2_tunnel_eth_type_conf`` and
  ``rte_eth_dev_l2_tunnel_offload_set`` which were not marked as deprecated,
  will be removed in DPDK 20.11.

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

* ethdev: Add new fields to ``rte_eth_rxconf`` to configure the receiving
  queues to split ingress packets into multiple segments according to the
  specified lengths into the buffers allocated from the specified
  memory pools. The backward compatibility to existing API is preserved.

* ethdev: ``rx_descriptor_done`` dev_ops and ``rte_eth_rx_descriptor_done``
  will be removed in 21.11.
  Existing ``rte_eth_rx_descriptor_status`` and ``rte_eth_tx_descriptor_status``
  APIs can be used as replacement.

* ethdev: The port mirroring API can be replaced with a more fine grain flow API.
  The structs ``rte_eth_mirror_conf``, ``rte_eth_vlan_mirror`` and the functions
  ``rte_eth_mirror_rule_set``, ``rte_eth_mirror_rule_reset`` will be marked
  as deprecated in DPDK 20.11, along with the associated macros ``ETH_MIRROR_*``.
  This API will be fully removed in DPDK 21.11.

* ethdev: The ``struct rte_flow_item_eth`` and ``struct rte_flow_item_vlan``
  structs will be modified, to include an additional value, indicating existence
  or absence of a VLAN header following the current header, as proposed in RFC
  https://mails.dpdk.org/archives/dev/2020-August/177536.html.

* ethdev: The ``struct rte_flow_item_ipv6`` struct will be modified to include
  additional values, indicating existence or absence of IPv6 extension headers
  following the IPv6 header, as proposed in RFC
  https://mails.dpdk.org/archives/dev/2020-August/177257.html.

* security: The API ``rte_security_session_create`` takes only single mempool
  for session and session private data. So the application need to create
  mempool for twice the number of sessions needed and will also lead to
  wastage of memory as session private data need more memory compared to session.
  Hence the API will be modified to take two mempool pointers - one for session
  and one for private data.

* cryptodev: ``RTE_CRYPTO_AEAD_LIST_END`` from ``enum rte_crypto_aead_algorithm``,
  ``RTE_CRYPTO_CIPHER_LIST_END`` from ``enum rte_crypto_cipher_algorithm`` and
  ``RTE_CRYPTO_AUTH_LIST_END`` from ``enum rte_crypto_auth_algorithm``
  will be removed.

* cryptodev: support for using IV with all sizes is added, J0 still can
  be used but only when IV length in following structs ``rte_crypto_auth_xform``,
  ``rte_crypto_aead_xform`` is set to zero. When IV length is greater or equal
  to one it means it represents IV, when is set to zero it means J0 is used
  directly, in this case 16 bytes of J0 need to be passed.

* scheduler: The functions ``rte_cryptodev_scheduler_slave_attach``,
  ``rte_cryptodev_scheduler_slave_detach`` and
  ``rte_cryptodev_scheduler_slaves_get`` will be replaced in 20.11 by
  ``rte_cryptodev_scheduler_worker_attach``,
  ``rte_cryptodev_scheduler_worker_detach`` and
  ``rte_cryptodev_scheduler_workers_get`` accordingly.

* eventdev: Following structures will be modified to support DLB PMD
  and future extensions:

  - ``rte_event_dev_info``
  - ``rte_event_dev_config``
  - ``rte_event_port_conf``

  Patches containing justification, documentation, and proposed modifications
  can be found at:

  - https://patches.dpdk.org/patch/71457/
  - https://patches.dpdk.org/patch/71456/

* acl: ``RTE_ACL_CLASSIFY_NUM`` enum value will be removed.
  This enum value is not used inside DPDK, while it prevents to add new
  classify algorithms without causing an ABI breakage.

* sched: To allow more traffic classes, flexible mapping of pipe queues to
  traffic classes, and subport level configuration of pipes and queues
  changes will be made to macros, data structures and API functions defined
  in "rte_sched.h". These changes are aligned to improvements suggested in the
  RFC https://mails.dpdk.org/archives/dev/2018-November/120035.html.

* sched: To allow dynamic configuration of the subport bandwidth profile,
  changes will be made to data structures ``rte_sched_subport_params``,
  ``rte_sched_port_params`` and new data structure, API functions will be
  defined in ``rte_sched.h``. These changes are aligned as suggested in the
  RFC https://mails.dpdk.org/archives/dev/2020-July/175161.html

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* power: ``rte_power_set_env`` function will no longer return 0 on attempt
  to set new power environment if power environment was already initialized.
  In this case the function will return -1 unless the environment is unset first
  (using ``rte_power_unset_env``). Other function usage scenarios will not change.

* dpdk-setup.sh: This old script relies on deprecated stuff, and especially
  ``make``. Given environments are too much variables for such a simple script,
  it will be removed in DPDK 20.11.
  Some useful parts may be converted into specific scripts.
