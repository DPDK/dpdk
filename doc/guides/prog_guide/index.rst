..  BSD LICENSE
    Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Programmer's Guide
==================

.. toctree::
    :maxdepth: 3
    :numbered:

    intro
    overview
    env_abstraction_layer
    service_cores
    ring_lib
    mempool_lib
    mbuf_lib
    poll_mode_drv
    rte_flow
    traffic_management
    cryptodev_lib
    link_bonding_poll_mode_drv_lib
    timer_lib
    hash_lib
    efd_lib
    lpm_lib
    lpm6_lib
    packet_distrib_lib
    reorder_lib
    ip_fragment_reassembly_lib
    generic_receive_offload_lib
    pdump_lib
    multi_proc_support
    kernel_nic_interface
    thread_safety_dpdk_functions
    eventdev
    qos_framework
    power_man
    packet_classif_access_ctrl
    packet_framework
    vhost_lib
    metrics_lib
    port_hotplug_framework
    source_org
    dev_kit_build_system
    dev_kit_root_make_help
    extend_dpdk
    build_app
    ext_app_lib_make_help
    perf_opt_guidelines
    writing_efficient_code
    profile_app
    glossary


**Figures**

:numref:`figure_architecture-overview` :ref:`figure_architecture-overview`

:numref:`figure_linuxapp_launch` :ref:`figure_linuxapp_launch`

:numref:`figure_malloc_heap` :ref:`figure_malloc_heap`

:numref:`figure_ring1` :ref:`figure_ring1`

:numref:`figure_ring-enqueue1` :ref:`figure_ring-enqueue1`

:numref:`figure_ring-enqueue2` :ref:`figure_ring-enqueue2`

:numref:`figure_ring-enqueue3` :ref:`figure_ring-enqueue3`

:numref:`figure_ring-dequeue1` :ref:`figure_ring-dequeue1`

:numref:`figure_ring-dequeue2` :ref:`figure_ring-dequeue2`

:numref:`figure_ring-dequeue3` :ref:`figure_ring-dequeue3`

:numref:`figure_ring-mp-enqueue1` :ref:`figure_ring-mp-enqueue1`

:numref:`figure_ring-mp-enqueue2` :ref:`figure_ring-mp-enqueue2`

:numref:`figure_ring-mp-enqueue3` :ref:`figure_ring-mp-enqueue3`

:numref:`figure_ring-mp-enqueue4` :ref:`figure_ring-mp-enqueue4`

:numref:`figure_ring-mp-enqueue5` :ref:`figure_ring-mp-enqueue5`

:numref:`figure_ring-modulo1` :ref:`figure_ring-modulo1`

:numref:`figure_ring-modulo2` :ref:`figure_ring-modulo2`

:numref:`figure_memory-management` :ref:`figure_memory-management`

:numref:`figure_memory-management2` :ref:`figure_memory-management2`

:numref:`figure_mempool` :ref:`figure_mempool`

:numref:`figure_mbuf1` :ref:`figure_mbuf1`

:numref:`figure_mbuf2` :ref:`figure_mbuf2`

:numref:`figure_multi_process_memory` :ref:`figure_multi_process_memory`

:numref:`figure_kernel_nic_intf` :ref:`figure_kernel_nic_intf`

:numref:`figure_pkt_flow_kni` :ref:`figure_pkt_flow_kni`


:numref:`figure_pkt_proc_pipeline_qos` :ref:`figure_pkt_proc_pipeline_qos`

:numref:`figure_hier_sched_blk` :ref:`figure_hier_sched_blk`

:numref:`figure_sched_hier_per_port` :ref:`figure_sched_hier_per_port`

:numref:`figure_data_struct_per_port` :ref:`figure_data_struct_per_port`

:numref:`figure_prefetch_pipeline` :ref:`figure_prefetch_pipeline`

:numref:`figure_pipe_prefetch_sm` :ref:`figure_pipe_prefetch_sm`

:numref:`figure_blk_diag_dropper` :ref:`figure_blk_diag_dropper`

:numref:`figure_flow_tru_droppper` :ref:`figure_flow_tru_droppper`

:numref:`figure_ex_data_flow_tru_dropper` :ref:`figure_ex_data_flow_tru_dropper`

:numref:`figure_pkt_drop_probability` :ref:`figure_pkt_drop_probability`

:numref:`figure_drop_probability_graph` :ref:`figure_drop_probability_graph`

:numref:`figure_figure32` :ref:`figure_figure32`

:numref:`figure_figure33` :ref:`figure_figure33`

:numref:`figure_figure34` :ref:`figure_figure34`

:numref:`figure_figure35` :ref:`figure_figure35`

:numref:`figure_figure37` :ref:`figure_figure37`

:numref:`figure_figure38` :ref:`figure_figure38`

:numref:`figure_figure39` :ref:`figure_figure39`

:numref:`figure_efd1` :ref:`figure_efd1`

:numref:`figure_efd2` :ref:`figure_efd2`

:numref:`figure_efd3` :ref:`figure_efd3`

:numref:`figure_efd4` :ref:`figure_efd4`

:numref:`figure_efd5` :ref:`figure_efd5`

:numref:`figure_efd6` :ref:`figure_efd6`

:numref:`figure_efd7` :ref:`figure_efd7`

:numref:`figure_efd8` :ref:`figure_efd8`

:numref:`figure_efd9` :ref:`figure_efd9`

:numref:`figure_efd10` :ref:`figure_efd10`

:numref:`figure_efd11` :ref:`figure_efd11`


**Tables**

:numref:`table_qos_1` :ref:`table_qos_1`

:numref:`table_qos_2` :ref:`table_qos_2`

:numref:`table_qos_3` :ref:`table_qos_3`

:numref:`table_qos_4` :ref:`table_qos_4`

:numref:`table_qos_5` :ref:`table_qos_5`

:numref:`table_qos_6` :ref:`table_qos_6`

:numref:`table_qos_7` :ref:`table_qos_7`

:numref:`table_qos_8` :ref:`table_qos_8`

:numref:`table_qos_9` :ref:`table_qos_9`

:numref:`table_qos_10` :ref:`table_qos_10`

:numref:`table_qos_11` :ref:`table_qos_11`

:numref:`table_qos_12` :ref:`table_qos_12`

:numref:`table_qos_13` :ref:`table_qos_13`

:numref:`table_qos_14` :ref:`table_qos_14`

:numref:`table_qos_15` :ref:`table_qos_15`

:numref:`table_qos_16` :ref:`table_qos_16`

:numref:`table_qos_17` :ref:`table_qos_17`

:numref:`table_qos_18` :ref:`table_qos_18`

:numref:`table_qos_19` :ref:`table_qos_19`

:numref:`table_qos_20` :ref:`table_qos_20`

:numref:`table_qos_21` :ref:`table_qos_21`

:numref:`table_qos_22` :ref:`table_qos_22`

:numref:`table_qos_23` :ref:`table_qos_23`

:numref:`table_qos_24` :ref:`table_qos_24`

:numref:`table_qos_25` :ref:`table_qos_25`

:numref:`table_qos_26` :ref:`table_qos_26`

:numref:`table_qos_27` :ref:`table_qos_27`

:numref:`table_qos_28` :ref:`table_qos_28`

:numref:`table_qos_29` :ref:`table_qos_29`

:numref:`table_qos_30` :ref:`table_qos_30`

:numref:`table_qos_31` :ref:`table_qos_31`

:numref:`table_qos_32` :ref:`table_qos_32`

:numref:`table_qos_33` :ref:`table_qos_33`

:numref:`table_qos_34` :ref:`table_qos_34`

:numref:`table_hash_lib_1` :ref:`table_hash_lib_1`

:numref:`table_hash_lib_2` :ref:`table_hash_lib_2`
