..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

|today|


**Contents**

.. toctree::
    :maxdepth: 3
    :numbered:

    intro
    overview
    env_abstraction_layer
    malloc_lib
    ring_lib
    mempool_lib
    mbuf_lib
    poll_mode_drv
    ivshmem_lib
    link_bonding_poll_mode_drv_lib
    timer_lib
    hash_lib
    lpm_lib
    lpm6_lib
    packet_distrib_lib
    reorder_lib
    ip_fragment_reassembly_lib
    multi_proc_support
    kernel_nic_interface
    thread_safety_intel_dpdk_functions
    qos_framework
    power_man
    packet_classif_access_ctrl
    packet_framework
    vhost_lib
    port_hotplug_framework
    source_org
    dev_kit_build_system
    dev_kit_root_make_help
    extend_intel_dpdk
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

:numref:`figure_vhost_net_arch2` :ref:`figure_vhost_net_arch2`

:numref:`figure_kni_traffic_flow` :ref:`figure_kni_traffic_flow`


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


**Tables**

:ref:`Table 1. Packet Processing Pipeline Implementing QoS <pg_table_1>`

:ref:`Table 2. Infrastructure Blocks Used by the Packet Processing Pipeline <pg_table_2>`

:ref:`Table 3. Port Scheduling Hierarchy <pg_table_3>`

:ref:`Table 4. Scheduler Internal Data Structures per Port <pg_table_4>`

:ref:`Table 5. Ethernet Frame Overhead Fields <pg_table_5>`

:ref:`Table 6. Token Bucket Generic Operations <pg_table_6>`

:ref:`Table 7. Token Bucket Generic Parameters <pg_table_7>`

:ref:`Table 8. Token Bucket Persistent Data Structure <pg_table_8>`

:ref:`Table 9. Token Bucket Operations <pg_table_9>`

:ref:`Table 10. Subport/Pipe Traffic Class Upper Limit Enforcement Persistent Data Structure <pg_table_10>`

:ref:`Table 11. Subport/Pipe Traffic Class Upper Limit Enforcement Operations <pg_table_11>`

:ref:`Table 12. Weighted Round Robin (WRR) <pg_table_12>`

:ref:`Table 13. Subport Traffic Class Oversubscription <pg_table_13>`

:ref:`Table 14. Watermark Propagation from Subport Level to Member Pipes at the Beginning of Each Traffic Class Upper Limit Enforcement Period <pg_table_14>`

:ref:`Table 15. Watermark Calculation <pg_table_15>`

:ref:`Table 16. RED Configuration Parameters <pg_table_16>`

:ref:`Table 17. Relative Performance of Alternative Approaches <pg_table_17>`

:ref:`Table 18. RED Configuration Corresponding to RED Configuration File <pg_table_18>`

:ref:`Table 19. Port types <pg_table_19>`

:ref:`Table 20. Port abstract interface <pg_table_20>`

:ref:`Table 21. Table types <pg_table_21>`

:ref:`Table 29. Table Abstract Interface <pg_table_29_1>`

:ref:`Table 22. Configuration parameters common for all hash table types <pg_table_22>`

:ref:`Table 23. Configuration parameters specific to extendable bucket hash table <pg_table_23>`

:ref:`Table 24. Configuration parameters specific to pre-computed key signature hash table <pg_table_24>`

:ref:`Table 25. The main large data structures (arrays) used for configurable key size hash tables <pg_table_25>`

:ref:`Table 26. Field description for bucket array entry (configurable key size hash tables) <pg_table_26>`

:ref:`Table 27. Description of the bucket search pipeline stages (configurable key size hash tables) <pg_table_27>`

:ref:`Table 28. Lookup tables for match, match_many, match_pos <pg_table_28>`

:ref:`Table 29. Collapsed lookup tables for match, match_many and match_pos <pg_table_29>`

:ref:`Table 30. The main large data structures (arrays) used for 8-byte and 16-byte key size hash tables <pg_table_30>`

:ref:`Table 31. Field description for bucket array entry (8-byte and 16-byte key hash tables) <pg_table_31>`

:ref:`Table 32. Description of the bucket search pipeline stages (8-byte and 16-byte key hash tables) <pg_table_32>`

:ref:`Table 33. Next hop actions (reserved) <pg_table_33>`

:ref:`Table 34. User action examples <pg_table_34>`
