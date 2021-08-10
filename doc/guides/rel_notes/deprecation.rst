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

* eal: The lcore state ``FINISHED`` will be removed from
  the ``enum rte_lcore_state_t``.
  The lcore state ``WAIT`` is enough to represent the same state.

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

* mbuf: The mbuf offload flags ``PKT_*`` will be renamed as ``RTE_MBUF_F_*``.
  A compatibility layer will be kept until DPDK 22.11, except for the flags
  that are already deprecated (``PKT_RX_L4_CKSUM_BAD``, ``PKT_RX_IP_CKSUM_BAD``,
  ``PKT_RX_EIP_CKSUM_BAD``, ``PKT_TX_QINQ_PKT``) which will be removed.

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
  ``rte_eth_fdir_flex_conf -> rte_fdir_conf -> rte_eth_conf (in the middle)``.
  Need to identify this kind of usages and fix in 20.11, otherwise this blocks
  us extending existing enum/define.
  One solution can be using a fixed size array instead of ``.*MAX.*`` value.

* ethdev: Will add ``RTE_ETH_`` prefix to all ethdev macros/enums in v21.11.
  Macros will be added for backward compatibility.
  Backward compatibility macros will be removed on v22.11.
  A few old backward compatibility macros from 2013 that does not have
  proper prefix will be removed on v21.11.

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

* ethdev: Announce moving from dedicated modify function for each field,
  to using the general ``rte_flow_modify_field`` action.

* ethdev: The struct ``rte_flow_action_modify_data`` will be modified
  to support modifying fields larger than 64 bits.
  In addition, documentation will be updated to clarify byte order.

* ethdev: Attribute ``shared`` of the ``struct rte_flow_action_count``
  is deprecated and will be removed in DPDK 21.11. Shared counters should
  be managed using shared actions API (``rte_flow_shared_action_create`` etc).

* ethdev: Definition of the flow API action ``RTE_FLOW_ACTION_TYPE_PORT_ID``
  is ambiguous and needs clarification.
  Structure ``rte_flow_action_port_id`` will be extended to specify
  traffic direction to the represented entity or ethdev port itself
  in DPDK 21.11.

* ethdev: Flow API documentation is unclear if ethdev port used to create
  a flow rule adds any implicit match criteria in the case of transfer rules.
  The semantics will be clarified in DPDK 21.11 and it will require fixes in
  drivers and applications which interpret it in a different way.

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

* net: ``s_addr`` and ``d_addr`` fields of ``rte_ether_hdr`` structure
  will be renamed in DPDK 21.11 to avoid conflict with Windows Sockets headers.

* net: The structure ``rte_ipv4_hdr`` will have two unions.
  The first union is for existing ``version_ihl`` byte
  and new bitfield for version and IHL.
  The second union is for existing ``fragment_offset``
  and new bitfield for fragment flags and offset.

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

* compressdev: ``min`` and ``max`` fields of ``rte_param_log2_range`` structure
  will be renamed in DPDK 21.11 to avoid conflict with Windows Sockets headers.

* cryptodev: ``min`` and ``max`` fields of ``rte_crypto_param_range`` structure
  will be renamed in DPDK 21.11 to avoid conflict with Windows Sockets headers.

* cryptodev: The field ``dataunit_len`` of the ``struct rte_crypto_cipher_xform``
  has a limited size ``uint16_t``.
  It will be moved and extended as ``uint32_t`` in DPDK 21.11.

* cryptodev: The structure ``rte_crypto_sym_vec`` would be updated to add
  ``dest_sgl`` to support out of place processing.
  This field will be null for inplace processing.
  This change is targeted for DPDK 21.11.

* cryptodev: The structure ``rte_crypto_vec`` would be updated to add
  ``tot_len`` to support total buffer length.
  This is required for security cases like IPsec and PDCP encryption offload
  to know how much additional memory space is available in buffer other than
  data length so that driver/HW can write expanded size data after encryption.
  This change is targeted for DPDK 21.11.

* cryptodev: Hide structures ``rte_cryptodev_sym_session`` and
  ``rte_cryptodev_asym_session`` to remove unnecessary indirection between
  session and the private data of session. An opaque pointer can be exposed
  directly to application which can be attached to the ``rte_crypto_op``.

* security: Hide structure ``rte_security_session`` and expose an opaque
  pointer for the private data to the application which can be attached
  to the packet while enqueuing.

* security: The IPsec configuration structure
  ``struct rte_security_ipsec_xform`` will be updated with new members to allow
  SA lifetime configuration. A new structure would be introduced to replace the
  current member, ``esn_soft_limit``.

* security: The structure ``rte_security_ipsec_xform`` will be extended with
  multiple fields: source and destination port of UDP encapsulation,
  IPsec payload MSS (Maximum Segment Size), and ESN (Extended Sequence Number).

* security: The IPsec SA config options ``struct rte_security_ipsec_sa_options``
  will be updated with new fields to support new features like IPsec inner
  checksum, tunnel header verification, TSO in case of protocol offload.

* ipsec: The structure ``rte_ipsec_sa_prm`` will be extended with a new field
  ``hdr_l3_len`` to configure tunnel L3 header length.

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
  for ``response_info``. Both should be decoupled for better clarity.
  New space for ``response_info`` can be made by changing
  ``rte_event_crypto_metadata`` type to structure from union.
  This change is targeted for DPDK 21.11.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* cmdline: ``cmdline`` structure will be made opaque to hide platform-specific
  content. On Linux and FreeBSD, supported prior to DPDK 20.11,
  original structure will be kept until DPDK 21.11.

* security: The functions ``rte_security_set_pkt_metadata`` and
  ``rte_security_get_userdata`` will be made inline functions and additional
  flags will be added in structure ``rte_security_ctx`` in DPDK 21.11.

* cryptodev: The structure ``rte_crypto_op`` would be updated to reduce
  reserved bytes to 2 (from 3), and use 1 byte to indicate warnings and other
  information from the crypto/security operation. This field will be used to
  communicate events such as soft expiry with IPsec in lookaside mode.
