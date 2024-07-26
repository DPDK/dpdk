..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

ABI and API Deprecation
=======================

See the guidelines document for details of the :doc:`ABI policy
<../contributing/abi_policy>`.

With DPDK 23.11, there will be a new major ABI version: 24.
This means that during the development of 23.11,
new items may be added to structs or enums,
even if those additions involve an ABI compatibility breakage.

Other API and ABI deprecation notices are to be posted below.

Deprecation Notices
-------------------

* build: The ``enable_kmods`` option is deprecated and will be removed in a future release.
  Setting/clearing the option has no impact on the build.
  Instead, kernel modules will be always built for OS's where out-of-tree kernel modules
  are required for DPDK operation.
  Currently, this means that modules will only be built for FreeBSD.
  No modules are shipped with DPDK for either Linux or Windows.

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* telemetry: The functions ``rte_tel_data_add_array_u64`` and ``rte_tel_data_add_dict_u64``,
  used by telemetry callbacks for adding unsigned integer values to be returned to the user,
  are renamed to ``rte_tel_data_add_array_uint`` and ``rte_tel_data_add_dict_uint`` respectively.
  As such, the old function names are deprecated and will be removed in a future release.

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

* net: A new IPv6 address structure will be introduced in DPDK 24.11.
  It will replace all ad-hoc ``uint8_t[16]`` arrays in all public APIs and structures.
  The following libraries and symbols are expected to be affected:

  ethdev
    - ``struct rte_flow_item_icmp6_nd_ns``
    - ``struct rte_flow_item_icmp6_nd_na``
    - ``struct rte_flow_action_set_ipv6``
    - ``struct rte_flow_tunnel``
  fib
    - ``rte_fib6_add()``
    - ``rte_fib6_delete()``
    - ``rte_fib6_lookup_bulk()``
  gro
    - ``struct tcp6_flow_key``
  hash
    - ``struct rte_ipv6_tuple``
  ipsec
    - ``struct rte_ipsec_sadv6_key``
  lpm
    - ``rte_lpm6_add()``
    - ``rte_lpm6_is_rule_present()``
    - ``rte_lpm6_delete()``
    - ``rte_lpm6_delete_bulk_func()``
    - ``rte_lpm6_lookup()``
    - ``rte_lpm6_lookup_bulk_func()``
  net
    - ``struct rte_ipv6_hdr``
  node
    - ``rte_node_ip6_route_add()``
  pipeline
    - ``struct rte_table_action_ipv6_header``
  rib
    - ``rte_rib6_lookup()``
    - ``rte_rib6_lookup_exact()``
    - ``rte_rib6_get_nxt()``
    - ``rte_rib6_insert()``
    - ``rte_rib6_remove()``
    - ``rte_rib6_get_ip()``
  table
    - ``struct rte_table_lpm_ipv6_key``

* net, ethdev: The flow item ``RTE_FLOW_ITEM_TYPE_VXLAN_GPE``
  is replaced with ``RTE_FLOW_ITEM_TYPE_VXLAN``.
  The struct ``rte_flow_item_vxlan_gpe`` and its mask ``rte_flow_item_vxlan_gpe_mask``
  are replaced with ``rte_flow_item_vxlan`` and ``rte_flow_item_vxlan_mask``.
  The flow item ``RTE_FLOW_ITEM_TYPE_VXLAN_GPE``,
  the structs ``rte_flow_item_vxlan_gpe``, ``rte_flow_item_vxlan_gpe_mask``,
  and the header struct ``rte_vxlan_gpe_hdr`` with the macro ``RTE_ETHER_VXLAN_GPE_HLEN``
  will be removed in DPDK 25.11.

* ethdev: The flow API matching pattern structures, ``struct rte_flow_item_*``,
  should start with relevant protocol header structure from lib/net/.
  The individual protocol header fields and the protocol header struct
  may be kept together in a union as a first migration step.
  In future (target is DPDK 23.11), the protocol header fields will be cleaned
  and only protocol header struct will remain.

  These items are not compliant (not including struct from lib/net/):

  - ``rte_flow_item_ah``
  - ``rte_flow_item_e_tag``
  - ``rte_flow_item_geneve``
  - ``rte_flow_item_geneve_opt``
  - ``rte_flow_item_gre``
  - ``rte_flow_item_icmp6``
  - ``rte_flow_item_icmp6_nd_na``
  - ``rte_flow_item_icmp6_nd_ns``
  - ``rte_flow_item_icmp6_nd_opt``
  - ``rte_flow_item_icmp6_nd_opt_sla_eth``
  - ``rte_flow_item_icmp6_nd_opt_tla_eth``
  - ``rte_flow_item_igmp``
  - ``rte_flow_item_ipv6_ext``
  - ``rte_flow_item_l2tpv3oip``
  - ``rte_flow_item_mpls``
  - ``rte_flow_item_nsh``
  - ``rte_flow_item_nvgre``
  - ``rte_flow_item_pfcp``
  - ``rte_flow_item_pppoe``
  - ``rte_flow_item_pppoe_proto_id``

* ethdev: Queue specific stats fields will be removed from ``struct rte_eth_stats``.
  Mentioned fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``,
  ``q_errors``.
  Instead queue stats will be received via xstats API. Current method support
  will be limited to maximum 256 queues.
  Also compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` will be removed.

* ethdev: Flow actions ``PF`` and ``VF`` have been deprecated since DPDK 21.11
  and are yet to be removed. That still has not happened because there are net
  drivers which support combined use of either action ``PF`` or action ``VF``
  with action ``QUEUE``, namely, i40e, ixgbe and txgbe (L2 tunnel rule).
  It is unclear whether it is acceptable to just drop support for
  such a complex use case, so maintainers of the said drivers
  should take a closer look at this and provide assistance.

* ethdev: Actions ``OF_DEC_NW_TTL``, ``SET_IPV4_SRC``, ``SET_IPV4_DST``,
  ``SET_IPV6_SRC``, ``SET_IPV6_DST``, ``SET_TP_SRC``, ``SET_TP_DST``,
  ``DEC_TTL``, ``SET_TTL``, ``SET_MAC_SRC``, ``SET_MAC_DST``, ``INC_TCP_SEQ``,
  ``DEC_TCP_SEQ``, ``INC_TCP_ACK``, ``DEC_TCP_ACK``, ``SET_IPV4_DSCP``,
  ``SET_IPV6_DSCP``, ``SET_TAG``, ``SET_META`` are marked as legacy and
  superseded by the generic ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``.
  The legacy actions should be removed
  once ``MODIFY_FIELD`` alternative is implemented in drivers.

* fib: A new flag field will be introduced in ``rte_fib_conf`` structure
  in DPDK 24.11. This field will be used to pass extra configuration settings.

* cryptodev: The function ``rte_cryptodev_cb_fn`` will be updated
  to have another parameter ``qp_id`` to return the queue pair ID
  which got error interrupt to the application,
  so that application can reset that particular queue pair.

* cryptodev: The structure ``rte_cryptodev_qp_conf`` will be updated
  to have a new parameter to set priority of that particular queue pair.

* cryptodev: The enum ``rte_crypto_asym_xform_type`` and struct ``rte_crypto_asym_op``
  will be extended to include new values to support EDDSA.
  This will break ABI compatibility with existing applications that use these data types.

* cryptodev: The ``rte_crypto_rsa_xform`` struct member to hold private key
  in either exponent or quintuple format is changed from union to struct data type.
  This change is to support ASN.1 syntax (RFC 3447 Appendix A.1.2).
  This change will not break existing applications.

* cryptodev: Some changes may happen to manage RSA padding for virtio-crypto.

* cryptodev: The Intel IPsec Multi-Buffer version will be bumped
  to a minimum version of v1.4.
  This will effect the KASUMI, SNOW3G, ZUC, AESNI GCM, AESNI MB and CHACHAPOLY
  SW PMDs.

* ipsec: The IPsec library is updated to support sequence number provided
  by application. To allow the same, two new API functions are being introduced:
  ``rte_ipsec_pkt_crypto_sqn_assign`` and ``rte_ipsec_pkt_crypto_xprepare``.
  It separates the sequence number update functionality
  from the existing ``rte_ipsec_pkt_crypto_prepare`` function.
  Corresponding configure structure changes are being made for the new API.
  Additionally a new flag ``RTE_IPSEC_SAFLAG_SQN_ASSIGN_DISABLE``
  is introduced to disable sequence number assignment in IPsec library.

* eventdev: The single-event (non-burst) enqueue and dequeue operations,
  used by static inline burst enqueue and dequeue functions in ``rte_eventdev.h``,
  will be removed in DPDK 23.11.
  This simplification includes changing the layout and potentially also
  the size of the public ``rte_event_fp_ops`` struct, breaking the ABI.
  Since these functions are not called directly by the application,
  the API remains unaffected.

* pipeline: The pipeline library legacy API (functions rte_pipeline_*)
  will be deprecated and subsequently removed in DPDK 24.11 release.
  Before this, the new pipeline library API (functions rte_swx_pipeline_*)
  will gradually transition from experimental to stable status.

* table: The table library legacy API (functions rte_table_*)
  will be deprecated and subsequently removed in DPDK 24.11 release.
  Before this, the new table library API (functions rte_swx_table_*)
  will gradually transition from experimental to stable status.

* port: The port library legacy API (functions rte_port_*)
  will be deprecated and subsequently removed in DPDK 24.11 release.
  Before this, the new port library API (functions rte_swx_port_*)
  will gradually transition from experimental to stable status.

* graph: The graph library data structures will be modified
  to support node specific errors.
  The structures ``rte_node``, ``rte_node_register``
  and ``rte_graph_cluster_node_stats`` will be extended
  to include node error counters and error description.
