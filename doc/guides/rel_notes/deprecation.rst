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

* bus: The ``rte_bus`` object will be made opaque in DPDK 22.11.
  The goal is to remove it from the public ABI and make this object extendable.
  As a side effect, registering a bus will be marked as an internal API:
  external users may still register their bus using a new driver header
  (see ``enable_driver_sdk`` meson option).

* drivers: As a follow-up of the work on the ``rte_bus`` object,
  the ``rte_driver`` and ``rte_device`` objects (and as a domino effect,
  their bus-specific counterparts) will be made opaque in DPDK 22.11.
  Registering a driver on a bus will be marked as an internal API:
  external users may still register their drivers using the bus-specific
  driver header (see ``enable_driver_sdk`` meson option).

* bus: The ``dev->device.numa_node`` field is set by each bus driver for
  every device it manages to indicate on which NUMA node this device lies.
  When this information is unknown, the assigned value is not consistent
  across the bus drivers.
  In DPDK 22.11, the default value will be set to -1 by all bus drivers
  when the NUMA information is unavailable.

* kni: The KNI kernel module and library are not recommended for use by new
  applications - other technologies such as virtio-user are recommended instead.
  Following the DPDK technical board
  `decision <https://mails.dpdk.org/archives/dev/2021-January/197077.html>`_
  and `refinement <http://mails.dpdk.org/archives/dev/2022-June/243596.html>`_:

  * Some deprecation warnings will be added in DPDK 22.11.
  * The KNI example application will be removed from DPDK 22.11.
  * The KNI kernel module, library and PMD will be removed from the DPDK 23.11.

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

* ethdev: The function ``rte_eth_set_queue_rate_limit`` takes ``rate`` in Mbps.
  The queue rate is limited to 64 Gbps because declared as ``uint16_t``.
  The ``rate`` parameter will be modified to ``uint32_t`` in DPDK 22.11
  so that it can work for more than 64 Gbps.

* ethdev: Since no single PMD supports ``RTE_ETH_RX_OFFLOAD_HEADER_SPLIT``
  offload and the ``split_hdr_size`` field in structure ``rte_eth_rxmode``
  to enable per-port header split, they will be removed in DPDK 22.11.
  The per-queue Rx packet split offload ``RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT``
  can still be used, and it is configured by ``rte_eth_rxseg_split``.

* ethdev: The flow director API, including ``rte_eth_conf.fdir_conf`` field,
  and the related structures (``rte_fdir_*`` and ``rte_eth_fdir_*``),
  will be removed in DPDK 20.11.

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

* ethdev: Actions ``OF_SET_MPLS_TTL``, ``OF_DEC_MPLS_TTL``, ``OF_SET_NW_TTL``,
  ``OF_COPY_TTL_OUT``, ``OF_COPY_TTL_IN`` are deprecated as not supported by
  any PMD, so they will be removed in DPDK 22.11.

* ethdev: Actions ``OF_DEC_NW_TTL``, ``SET_IPV4_SRC``, ``SET_IPV4_DST``,
  ``SET_IPV6_SRC``, ``SET_IPV6_DST``, ``SET_TP_SRC``, ``SET_TP_DST``,
  ``DEC_TTL``, ``SET_TTL``, ``SET_MAC_SRC``, ``SET_MAC_DST``, ``INC_TCP_SEQ``,
  ``DEC_TCP_SEQ``, ``INC_TCP_ACK``, ``DEC_TCP_ACK``, ``SET_IPV4_DSCP``,
  ``SET_IPV6_DSCP``, ``SET_TAG``, ``SET_META`` are marked as legacy and
  superseded by the generic MODIFY_FIELD action.
  The legacy actions should be deprecated in 22.07, once MODIFY_FIELD
  alternative is implemented.
  The legacy actions should be removed in DPDK 22.11.

* ethdev: The enum ``rte_eth_event_ipsec_subtype`` will be extended to add
  new subtype values ``RTE_ETH_EVENT_IPSEC_SA_PKT_EXPIRY``,
  ``RTE_ETH_EVENT_IPSEC_SA_BYTE_HARD_EXPIRY`` and
  ``RTE_ETH_EVENT_IPSEC_SA_PKT_HARD_EXPIRY`` in DPDK 22.11.

* bbdev: ``RTE_BBDEV_OP_TYPE_COUNT`` terminating the ``rte_bbdev_op_type``
  enum will be deprecated and instead use fixed array size when required
  to allow for future enum extension.
  Will extend API to support new operation type ``RTE_BBDEV_OP_FFT`` as per
  this `RFC <https://patches.dpdk.org/project/dpdk/list/?series=22111>`__.
  New members will be added in ``rte_bbdev_driver_info`` to expose
  PMD queue topology inspired by
  this `RFC <https://patches.dpdk.org/project/dpdk/list/?series=22076>`__.
  New member will be added in ``rte_bbdev_driver_info`` to expose
  the device status as per
  this `RFC <https://patches.dpdk.org/project/dpdk/list/?series=23367>`__.
  This should be updated in DPDK 22.11.

* cryptodev: Hide structures ``rte_cryptodev_sym_session`` and
  ``rte_cryptodev_asym_session`` to remove unnecessary indirection between
  session and the private data of session. An opaque pointer can be exposed
  directly to application which can be attached to the ``rte_crypto_op``.

* cryptodev: The function ``rte_cryptodev_cb_fn`` will be updated
  to have another parameter ``qp_id`` to return the queue pair ID
  which got error interrupt to the application,
  so that application can reset that particular queue pair.

* security: Hide structure ``rte_security_session`` and expose an opaque
  pointer for the private data to the application which can be attached
  to the packet while enqueuing.

* security: MACsec support is planned to be added in DPDK 22.11,
  which would result in updates to structures ``rte_security_macsec_xform``,
  ``rte_security_macsec_stats`` and security capability structure
  ``rte_security_capability`` to accommodate MACsec capabilities.

* eventdev: The function ``rte_event_crypto_adapter_queue_pair_add`` will
  accept configuration of type ``rte_event_crypto_adapter_queue_conf`` instead
  of ``rte_event``, similar to ``rte_event_eth_rx_adapter_queue_add`` signature.
  Event will be one of the configuration fields,
  together with additional vector parameters.

* eventdev: The structure ``rte_event_timer_adapter_stats`` will be
  extended by adding a new field ``evtim_drop_count``.
  This counter will represent the number of times an event_timer expiry event
  is dropped by the timer adapter.
  This field will be used to add periodic mode support
  to the software timer adapter in DPDK 22.11.

* eventdev: The function pointer declaration ``eventdev_stop_flush_t``
  will be renamed to ``rte_eventdev_stop_flush_t`` in DPDK 22.11.

* eventdev: The element ``*u64s`` in the structure ``rte_event_vector``
  is deprecated and will be replaced with ``u64s`` in DPDK 22.11.

* eventdev: The structure ``rte_event_vector`` will be modified to include
  ``elem_offset:12`` bits taken from ``rsvd:15``. The ``elem_offset`` defines
  the offset into the vector array from which valid elements are present.
  The difference between ``rte_event_vector::nb_elem`` and
  ``rte_event_vector::elem_offset`` gives the number of valid elements left
  to process from the ``rte_event_vector::elem_offset``.

* eventdev: New fields to represent event queue weight and affinity
  will be added to ``rte_event_queue_conf`` structure in DPDK 22.11.

* metrics: The function ``rte_metrics_init`` will have a non-void return
  in order to notify errors instead of calling ``rte_exit``.

* telemetry: The allowed characters in names for dictionary values
  will be limited to alphanumeric characters
  and a small subset of additional printable characters.
  This will ensure that all dictionary parameter names can be output
  without escaping in JSON - or in any future output format used.
  Names for the telemetry commands will be similarly limited.
  The parameters for telemetry commands are unaffected by this change.

* net/octeontx_ep: The driver ``octeontx_ep`` was to support OCTEON TX
  line of products.
  It will be renamed to ``octeon_ep`` in DPDK 22.11 to apply for
  all OCTEON EP products: OCTEON TX and future OCTEON chipsets.

* raw/dpaa2_cmdif: The ``dpaa2_cmdif`` rawdev driver will be deprecated
  in DPDK 22.11, as it is no longer in use, no active user known.

* raw/ifgpa: The function ``rte_pmd_ifpga_get_pci_bus`` will be removed
  in DPDK 22.11.

* raw/ioat: The ``ioat`` rawdev driver has been deprecated, since it's
  functionality is provided through the new ``dmadev`` infrastructure.
  To continue to use hardware previously supported by the ``ioat`` rawdev driver,
  applications should be updated to use the ``dmadev`` library instead,
  with the underlying HW-functionality being provided by the ``ioat`` or
  ``idxd`` dma drivers
