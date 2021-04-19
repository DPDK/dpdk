..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`. API and ABI deprecation notices are to be posted
here.

Deprecation Notices
-------------------

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

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

* ethdev: The flow director API, including ``rte_eth_conf.fdir_conf`` field,
  and the related structures (``rte_fdir_*`` and ``rte_eth_fdir_*``),
  will be removed in DPDK 20.11.

* ethdev: New offload flags ``DEV_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::fdir``.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the field even when the offload is not
  enabled.

* ethdev: ``uint32_t max_rx_pkt_len`` field of ``struct rte_eth_rxmode``, will be
  replaced by a new ``uint32_t mtu`` field of ``struct rte_eth_conf`` in v21.11.
  The new ``mtu`` field will be used to configure the initial device MTU via
  ``rte_eth_dev_configure()`` API.
  Later MTU can be changed by ``rte_eth_dev_set_mtu()`` API as done now.
  The existing ``(struct rte_eth_dev)->data->mtu`` variable will be used to store
  the configured ``mtu`` value,
  and this new ``(struct rte_eth_dev)->data->dev_conf.mtu`` variable will
  be used to store the user configuration request.
  Unlike ``max_rx_pkt_len``, which was valid only when ``JUMBO_FRAME`` enabled,
  ``mtu`` field will be always valid.
  When ``mtu`` config is not provided by the application, default ``RTE_ETHER_MTU``
  value will be used.
  ``(struct rte_eth_dev)->data->mtu`` should be updated after MTU set successfully,
  either by ``rte_eth_dev_configure()`` or ``rte_eth_dev_set_mtu()``.

  An application may need to configure device for a specific Rx packet size, like for
  cases ``DEV_RX_OFFLOAD_SCATTER`` is not supported and device received packet size
  can't be bigger than Rx buffer size.
  To cover these cases an application needs to know the device packet overhead to be
  able to calculate the ``mtu`` corresponding to a Rx buffer size, for this
  ``(struct rte_eth_dev_info).max_rx_pktlen`` will be kept,
  the device packet overhead can be calculated as:
  ``(struct rte_eth_dev_info).max_rx_pktlen - (struct rte_eth_dev_info).max_mtu``

* ethdev: ``rx_descriptor_done`` dev_ops and ``rte_eth_rx_descriptor_done``
  will be removed in 21.11.
  Existing ``rte_eth_rx_descriptor_status`` and ``rte_eth_tx_descriptor_status``
  APIs can be used as replacement.

* ethdev: The port mirroring API can be replaced with a more fine grain flow API.
  The structs ``rte_eth_mirror_conf``, ``rte_eth_vlan_mirror`` and the functions
  ``rte_eth_mirror_rule_set``, ``rte_eth_mirror_rule_reset`` will be marked
  as deprecated in DPDK 20.11, along with the associated macros ``ETH_MIRROR_*``.
  This API will be fully removed in DPDK 21.11.

* ethdev: Attribute ``shared`` of the ``struct rte_flow_action_count``
  is deprecated and will be removed in DPDK 21.11. Shared counters should
  be managed using shared actions API (``rte_flow_shared_action_create`` etc).

* ethdev: The flow API matching pattern structures, ``struct rte_flow_item_*``,
  should start with relevant protocol header.
  Some matching pattern structures implements this by duplicating protocol header
  fields in the struct. To clarify the intention and to be sure protocol header
  is intact, will replace those fields with relevant protocol header struct.
  In v21.02 both individual protocol header fields and the protocol header struct
  will be added as union, target is switch usage to the protocol header by time.
  In v21.11 LTS, protocol header fields will be cleaned and only protocol header
  struct will remain.

* ethdev: Queue specific stats fields will be removed from ``struct rte_eth_stats``.
  Mentioned fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``,
  ``q_errors``.
  Instead queue stats will be received via xstats API. Current method support
  will be limited to maximum 256 queues.
  Also compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` will be removed.

* ethdev: The offload flag ``PKT_RX_EIP_CKSUM_BAD`` will be removed and
  replaced by the new flag ``PKT_RX_OUTER_IP_CKSUM_BAD``. The new name is more
  consistent with existing outer header checksum status flag naming, which
  should help in reducing confusion about its usage.

* i40e: As there are both i40evf and iavf pmd, the functions of them are
  duplicated. And now more and more advanced features are developed on iavf.
  To keep consistent with kernel driver's name
  (https://patchwork.ozlabs.org/patch/970154/), i40evf is no need to maintain.
  Starting from 21.05, the default VF driver of i40e will be iavf, but i40evf
  can still be used if users specify the devarg "driver=i40evf". I40evf will
  be deleted in DPDK 21.11.

* eventdev: The structure ``rte_event_eth_rx_adapter_queue_conf`` will be
  extended to include ``rte_event_eth_rx_adapter_event_vector_config`` elements
  and the function ``rte_event_eth_rx_adapter_queue_event_vector_config`` will
  be removed in DPDK 21.11.

  An application can enable event vectorization by passing the desired vector
  values to the function ``rte_event_eth_rx_adapter_queue_add`` using
  the structure ``rte_event_eth_rx_adapter_queue_add``.

* sched: To allow more traffic classes, flexible mapping of pipe queues to
  traffic classes, and subport level configuration of pipes and queues
  changes will be made to macros, data structures and API functions defined
  in "rte_sched.h". These changes are aligned to improvements suggested in the
  RFC https://mails.dpdk.org/archives/dev/2018-November/120035.html.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* cmdline: ``cmdline`` structure will be made opaque to hide platform-specific
  content. On Linux and FreeBSD, supported prior to DPDK 20.11,
  original structure will be kept until DPDK 21.11.
