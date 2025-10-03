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

* kvargs: The function ``rte_kvargs_process`` will get a new parameter
  for returning key match count. It will ease handling of no-match case.

* eal: The ``-c <coremask>`` commandline parameter is deprecated
  and will be removed in a future release.
  Use the ``-l <corelist>`` or ``--lcores=<corelist>`` parameters instead
  to specify the cores to be used when running a DPDK application.

* eal: The ``-s <service-coremask>`` commandline parameter is deprecated
  and will be removed in a future release.
  Use the ``-S <service-corelist>`` parameter instead
  to specify the cores to be used for background services in DPDK.

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

* bus/vmbus: Starting DPDK 25.11, all the vmbus API defined in
  ``drivers/bus/vmbus/rte_bus_vmbus.h`` will become internal to DPDK.
  Those API functions are used internally by DPDK core and netvsc PMD.

* net/intel: Drivers that have an SSE vector path alongside other vector paths,
  namely i40e, iavf and ice, will have their SSE vector paths removed in DPDK 25.11.
  Modern x86 systems all support AVX2, if not AVX-512,
  so the SSE path is no longer widely used.
  This change will not result in any feature loss,
  as the fallback scalar paths which have feature parity with SSE
  will be used in the cases where the SSE paths would have been used.

* net/mlx5: ``repr_matching_en`` device argument is deprecated
  and will be removed in DPDK 25.11 release.
  With disabled representor matching, behavior of Rx datapath in mlx5 PMD
  is incompatible with current DPDK representor model.
  Packets from any E-Switch port can arrive on any representor,
  depending only on created flow rules.
  Such working model should be exposed directly in DPDK ethdev API,
  without relying on flow API.
  Currently there is no alternative API
  providing the same functionality as with ``repr_matching_en`` set to 0.
