..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`. API and ABI deprecation notices are to be posted
here.

Deprecation Notices
-------------------

* build: The macros defined to indicate which DPDK libraries and drivers
  are included in the meson build are changing to a standardized format of
  ``RTE_LIB_<NAME>`` and ``RTE_<CLASS>_<NAME>``, where ``NAME`` is the
  upper-case component name, e.g. EAL, ETHDEV, IXGBE, and ``CLASS`` is the
  upper-case name of the device class to which a driver belongs e.g.
  ``NET``, ``CRYPTO``, ``VDPA``. The old macros are deprecated and will be
  removed in a future release.

* meson: The minimum supported version of meson for configuring and building
  DPDK will be increased to v0.47.1 (from 0.41) from DPDK 19.05 onwards. For
  those users with a version earlier than 0.47.1, an updated copy of meson
  can be got using the ``pip3`` tool (or ``python3 -m pip``) for downloading
  python packages.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

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
  As a consequence, the layout of the ``struct rte_mbuf`` will be re-arranged,
  avoiding impact on vectorized implementation of the driver datapaths,
  while evaluating performance gains of a better use of the first cache line.


* ethdev: the legacy filter API, including
  ``rte_eth_dev_filter_supported()``, ``rte_eth_dev_filter_ctrl()`` as well
  as filter types FLEXIBLE, SYN, NTUPLE, TUNNEL, FDIR,
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

* ethdev: New offload flags ``DEV_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::fdir``.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the field even when the offload is not
  enabled.

* ethdev: ``rx_descriptor_done`` dev_ops and ``rte_eth_rx_descriptor_done``
  will be removed in 21.11.
  Existing ``rte_eth_rx_descriptor_status`` and ``rte_eth_tx_descriptor_status``
  APIs can be used as replacement.

* ethdev: The port mirroring API can be replaced with a more fine grain flow API.
  The structs ``rte_eth_mirror_conf``, ``rte_eth_vlan_mirror`` and the functions
  ``rte_eth_mirror_rule_set``, ``rte_eth_mirror_rule_reset`` will be marked
  as deprecated in DPDK 20.11, along with the associated macros ``ETH_MIRROR_*``.
  This API will be fully removed in DPDK 21.11.

* ethdev: Queue specific stats fields will be removed from ``struct rte_eth_stats``.
  Mentioned fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``,
  ``q_errors``.
  Instead queue stats will be received via xstats API. Current method support
  will be limited to maximum 256 queues.
  Also compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` will be removed.

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

* cmdline: ``cmdline`` structure will be made opaque to hide platform-specific
  content. On Linux and FreeBSD, supported prior to DPDK 20.11,
  original structure will be kept until DPDK 21.11.

* dpdk-setup.sh: This old script relies on deprecated stuff, and especially
  ``make``. Given environments are too much variables for such a simple script,
  it will be removed in DPDK 20.11.
  Some useful parts may be converted into specific scripts.
