..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

Sample Applications User Guide
==============================

|today|

**Contents**

.. toctree::
    :maxdepth: 2
    :numbered:

    intro
    cmd_line
    exception_path
    hello_world
    skeleton
    rxtx_callbacks
    ip_frag
    ipv4_multicast
    ip_reassembly
    kernel_nic_interface
    l2_forward_job_stats
    l2_forward_real_virtual
    l3_forward
    l3_forward_power_man
    l3_forward_access_ctrl
    l3_forward_virtual
    link_status_intr
    load_balancer
    multi_process
    qos_metering
    qos_scheduler
    intel_quickassist
    quota_watermark
    timer
    packet_ordering
    vmdq_dcb_forwarding
    vhost
    netmap_compatibility
    internet_proto_ip_pipeline
    test_pipeline
    dist_app
    vm_power_management

**Figures**

:ref:`Figure 1.Packet Flow <figure_1>`

:ref:`Figure 2.Kernel NIC Application Packet Flow <figure_2>`

:ref:`Figure 3.Performance Benchmark Setup (Basic Environment) <figure_3>`

:ref:`Figure 4.Performance Benchmark Setup (Virtualized Environment) <figure_4>`

:ref:`Figure 5.Load Balancer Application Architecture <figure_5>`

:ref:`Figure 5.Example Rules File <figure_5_1>`

:ref:`Figure 6.Example Data Flow in a Symmetric Multi-process Application <figure_6>`

:ref:`Figure 7.Example Data Flow in a Client-Server Symmetric Multi-process Application <figure_7>`

:ref:`Figure 8.Master-slave Process Workflow <figure_8>`

:ref:`Figure 9.Slave Process Recovery Process Flow <figure_9>`

:ref:`Figure 10.QoS Scheduler Application Architecture <figure_10>`

:ref:`Figure 11.Intel®QuickAssist Technology Application Block Diagram <figure_11>`

:ref:`Figure 12.Pipeline Overview <figure_12>`

:ref:`Figure 13.Ring-based Processing Pipeline Performance Setup <figure_13>`

:ref:`Figure 14.Threads and Pipelines <figure_14>`

:ref:`Figure 15.Packet Flow Through the VMDQ and DCB Sample Application <figure_15>`

:ref:`Figure 16.QEMU Virtio-net (prior to vhost-net) <figure_16>`

:ref:`Figure 17.Virtio with Linux* Kernel Vhost <figure_17>`

:ref:`Figure 18.Vhost-net Architectural Overview <figure_18>`

:ref:`Figure 19.Packet Flow Through the vhost-net Sample Application <figure_19>`

:ref:`Figure 20.Packet Flow on TX in DPDK-testpmd <figure_20>`

:ref:`Figure 21.Test Pipeline Application <figure_21>`

:ref:`Figure 22.Performance Benchmarking Setup (Basic Environment) <figure_22>`

:ref:`Figure 23.Distributor Sample Application Layout <figure_23>`

:ref:`Figure 24.High level Solution <figure_24>`

:ref:`Figure 25.VM request to scale frequency <figure_25>`

**Tables**

:ref:`Table 1.Output Traffic Marking <table_1>`

:ref:`Table 2.Entity Types <table_2>`

:ref:`Table 3.Table Types <table_3>`
