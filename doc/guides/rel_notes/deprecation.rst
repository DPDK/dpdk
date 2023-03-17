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

* telemetry: The functions ``rte_tel_data_add_array_u64`` and ``rte_tel_data_add_dict_u64``,
  used by telemetry callbacks for adding unsigned integer values to be returned to the user,
  are renamed to ``rte_tel_data_add_array_uint`` and ``rte_tel_data_add_dict_uint`` respectively.
  As such, the old function names are deprecated and will be removed in a future release.

* eal: RTE_FUNC_PTR_OR_* macros have been marked deprecated and will be removed
  in the future. Applications can use ``devtools/cocci/func_or_ret.cocci``
  to update their code.

* eal: The functions ``rte_thread_setname`` and ``rte_ctrl_thread_create``
  are planned to be deprecated starting with the 23.07 release, subject to
  the replacement API rte_thread_set_name and rte_thread_create_control being
  marked as stable, and planned to be removed by the 23.11 release.

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

* kni: The KNI kernel module and library are not recommended for use by new
  applications - other technologies such as virtio-user are recommended instead.
  Following the DPDK technical board
  `decision <https://mails.dpdk.org/archives/dev/2021-January/197077.html>`_
  and `refinement <https://mails.dpdk.org/archives/dev/2022-June/243596.html>`_,
  the KNI kernel module, library and PMD will be removed from the DPDK 23.11 release.

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

* net/bnx2x: Starting from DPDK 23.07, the Marvell QLogic bnx2x driver will be removed.
  This decision has been made to alleviate the burden of maintaining a discontinued product.

* net/liquidio: Remove LiquidIO ethdev driver.
  The LiquidIO product line has been substituted
  with CN9K/CN10K OCTEON product line smart NICs located in ``drivers/net/octeon_ep/``.
  DPDK 20.08 has categorized the LiquidIO driver as UNMAINTAINED
  because of the absence of updates in the driver.
  Due to the above reasons, the driver will be unavailable from DPDK 23.07.

* cryptodev: The function ``rte_cryptodev_cb_fn`` will be updated
  to have another parameter ``qp_id`` to return the queue pair ID
  which got error interrupt to the application,
  so that application can reset that particular queue pair.

* cryptodev: The arrays of algorithm strings ``rte_crypto_cipher_algorithm_strings``,
  ``rte_crypto_auth_algorithm_strings``, ``rte_crypto_aead_algorithm_strings`` and
  ``rte_crypto_asym_xform_strings`` are deprecated and will be removed in DPDK 23.11.
  Application can use the new APIs ``rte_cryptodev_get_cipher_algo_string``,
  ``rte_cryptodev_get_auth_algo_string``, ``rte_cryptodev_get_aead_algo_string`` and
  ``rte_cryptodev_asym_get_xform_string`` respectively.

* flow_classify: The flow_classify library and example have no maintainer.
  The library is experimental and, as such, it could be removed from DPDK.
  Its removal has been postponed to let potential users report interest
  in maintaining it.
  In the absence of such interest, this library will be removed in DPDK 23.11.

* pipeline: The pipeline library legacy API (functions rte_pipeline_*)
  will be deprecated in DPDK 23.07 release and removed in DPDK 23.11 release.
  The new pipeline library API (functions rte_swx_pipeline_*)
  will gradually transition from experimental to stable status
  starting with DPDK 23.07 release.

* table: The table library legacy API (functions rte_table_*)
  will be deprecated in DPDK 23.07 release and removed in DPDK 23.11 release.
  The new table library API (functions rte_swx_table_*)
  will gradually transition from experimental to stable status
  starting with DPDK 23.07 release.

* port: The port library legacy API (functions rte_port_*)
  will be deprecated in DPDK 23.07 release and removed in DPDK 23.11 release.
  The new port library API (functions rte_swx_port_*)
  will gradually transition from experimental to stable status
  starting with DPDK 23.07 release.
