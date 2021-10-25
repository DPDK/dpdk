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

* eal: Making ``struct rte_intr_handle`` internal to avoid any ABI breakages
  in future.

* rte_atomicNN_xxx: These APIs do not take memory order parameter. This does
  not allow for writing optimized code for all the CPU architectures supported
  in DPDK. DPDK has adopted the atomic operations from
  https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html. These
  operations must be used for patches that need to be merged in 20.08 onwards.
  This change will not introduce any performance degradation.

* rte_smp_*mb: These APIs provide full barrier functionality. However, many
  use cases do not require full barriers. To support such use cases, DPDK has
  adopted atomic operations from
  https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html. These
  operations and a new wrapper ``rte_atomic_thread_fence`` instead of
  ``__atomic_thread_fence`` must be used for patches that need to be merged in
  20.08 onwards. This change will not introduce any performance degradation.

* mempool: Helper macro ``MEMPOOL_HEADER_SIZE()`` is deprecated and will
  be removed in DPDK 22.11. The replacement macro
  ``RTE_MEMPOOL_HEADER_SIZE()`` is internal only.

* mempool: Macro to register mempool driver ``MEMPOOL_REGISTER_OPS()`` is
  deprecated and will be removed in DPDK 22.11. Use replacement macro
  ``RTE_MEMPOOL_REGISTER_OPS()``.

* mempool: The mempool API macros ``MEMPOOL_PG_*`` are deprecated and
  will be removed in DPDK 22.11.

* pci: To reduce unnecessary ABIs exposed by DPDK bus driver, "rte_bus_pci.h"
  will be made internal in 21.11 and macros/data structures/functions defined
  in the header will not be considered as ABI anymore. This change is inspired
  by the RFC https://patchwork.dpdk.org/project/dpdk/list/?series=17176.

* lib: will fix extending some enum/define breaking the ABI. There are multiple
  samples in DPDK that enum/define terminated with a ``.*MAX.*`` value which is
  used by iterators, and arrays holding these values are sized with this
  ``.*MAX.*`` value. So extending this enum/define increases the ``.*MAX.*``
  value which increases the size of the array and depending on how/where the
  array is used this may break the ABI.
  ``RTE_ETH_FLOW_MAX`` is one sample of the mentioned case, adding a new flow
  type will break the ABI because of ``flex_mask[RTE_ETH_FLOW_MAX]`` array
  usage in following public struct hierarchy:
  ``rte_eth_fdir_flex_conf -> rte_eth_fdir_conf -> rte_eth_conf (in the middle)``.
  Need to identify this kind of usages and fix in 20.11, otherwise this blocks
  us extending existing enum/define.
  One solution can be using a fixed size array instead of ``.*MAX.*`` value.

* ethdev: The flow director API, including ``rte_eth_conf.fdir_conf`` field,
  and the related structures (``rte_fdir_*`` and ``rte_eth_fdir_*``),
  will be removed in DPDK 20.11.

* ethdev: New offload flags ``RTE_ETH_RX_OFFLOAD_FLOW_MARK`` will be added in 19.11.
  This will allow application to enable or disable PMDs from updating
  ``rte_mbuf::hash::fdir``.
  This scheme will allow PMDs to avoid writes to ``rte_mbuf`` fields on Rx and
  thereby improve Rx performance if application wishes do so.
  In 19.11 PMDs will still update the field even when the offload is not
  enabled.

* ethdev: Announce moving from dedicated modify function for each field,
  to using the general ``rte_flow_modify_field`` action.

* ethdev: The flow API matching pattern structures, ``struct rte_flow_item_*``,
  should start with relevant protocol header.
  Some matching pattern structures implements this by duplicating protocol header
  fields in the struct. To clarify the intention and to be sure protocol header
  is intact, will replace those fields with relevant protocol header struct.
  In v21.02 both individual protocol header fields and the protocol header struct
  will be added as union, target is switch usage to the protocol header by time.
  In v21.11 LTS, protocol header fields will be cleaned and only protocol header
  struct will remain.

* ethdev: Add new Rx queue offload flag ``RTE_ETH_RX_OFFLOAD_SHARED_RXQ`` and
  ``shared_group`` field to ``struct rte_eth_rxconf``.

* ethdev: Queue specific stats fields will be removed from ``struct rte_eth_stats``.
  Mentioned fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``,
  ``q_errors``.
  Instead queue stats will be received via xstats API. Current method support
  will be limited to maximum 256 queues.
  Also compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` will be removed.

* ethdev: Items and actions ``PF``, ``VF``, ``PHY_PORT``, ``PORT_ID`` are
  deprecated as hard-to-use / ambiguous and will be removed in DPDK 22.11.

* ethdev: The use of attributes ``ingress`` / ``egress`` in "transfer" flows
  is deprecated as ambiguous with respect to the embedded switch. The use of
  these attributes will become invalid starting from DPDK 22.11.

* vhost: ``rte_vdpa_register_device``, ``rte_vdpa_unregister_device``,
  ``rte_vhost_host_notifier_ctrl`` and ``rte_vdpa_relay_vring_used`` vDPA
  driver interface will be marked as internal in DPDK v21.11.

* vhost: rename ``struct vhost_device_ops`` to ``struct rte_vhost_device_ops``
  in DPDK v21.11.

* vhost: The experimental tags of ``rte_vhost_driver_get_protocol_features``,
  ``rte_vhost_driver_get_queue_num``, ``rte_vhost_crypto_create``,
  ``rte_vhost_crypto_free``, ``rte_vhost_crypto_fetch_requests``,
  ``rte_vhost_crypto_finalize_requests``, ``rte_vhost_crypto_set_zero_copy``,
  ``rte_vhost_va_from_guest_pa``, ``rte_vhost_extern_callback_register``,
  and ``rte_vhost_driver_set_protocol_features`` functions will be removed
  and the API functions will be made stable in DPDK 21.11.

* cryptodev: Hide structures ``rte_cryptodev_sym_session`` and
  ``rte_cryptodev_asym_session`` to remove unnecessary indirection between
  session and the private data of session. An opaque pointer can be exposed
  directly to application which can be attached to the ``rte_crypto_op``.

* security: Hide structure ``rte_security_session`` and expose an opaque
  pointer for the private data to the application which can be attached
  to the packet while enqueuing.

* eventdev: The file ``rte_eventdev_pmd.h`` will be renamed to ``eventdev_driver.h``
  to make the driver interface as internal and the structures ``rte_eventdev_data``,
  ``rte_eventdev`` and ``rte_eventdevs`` will be moved to a new file named
  ``rte_eventdev_core.h`` in DPDK 21.11.
  The ``rte_`` prefix for internal structures and functions will be removed across the
  library.
  The experimental eventdev trace APIs and ``rte_event_vector_pool_create``,
  ``rte_event_eth_rx_adapter_vector_limits_get`` will be promoted to stable.
  An 8-byte reserved field will be added to the structure ``rte_event_timer`` to
  support future extensions.

* eventdev: The structure ``rte_event_eth_rx_adapter_queue_conf`` will be
  extended to include ``rte_event_eth_rx_adapter_event_vector_config`` elements
  and the function ``rte_event_eth_rx_adapter_queue_event_vector_config`` will
  be removed in DPDK 21.11.

  An application can enable event vectorization by passing the desired vector
  values to the function ``rte_event_eth_rx_adapter_queue_add`` using
  the structure ``rte_event_eth_rx_adapter_queue_add``.

* eventdev: Reserved bytes of ``rte_event_crypto_request`` is a space holder
  for ``response_info``. Both should be decoupled for better clarity in
  DPDK 21.11.
  New space for ``response_info`` can be made by changing
  ``rte_event_crypto_metadata`` type to structure from union.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* raw/ioat: The ``ioat`` rawdev driver has been deprecated, since it's
  functionality is provided through the new ``dmadev`` infrastructure.
  To continue to use hardware previously supported by the ``ioat`` rawdev driver,
  applications should be updated to use the ``dmadev`` library instead,
  with the underlying HW-functionality being provided by the ``ioat`` or
  ``idxd`` dma drivers
