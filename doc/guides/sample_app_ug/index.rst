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

Sample Applications User Guide
==============================

June 2014

INFORMATION IN THIS DOCUMENT IS PROVIDED IN CONNECTION WITH INTEL PRODUCTS.
NO LICENSE, EXPRESS OR IMPLIED, BY ESTOPPEL OR OTHERWISE, TO ANY INTELLECTUAL PROPERTY RIGHTS IS GRANTED BY THIS DOCUMENT.
EXCEPT AS PROVIDED IN INTEL'S TERMS AND CONDITIONS OF SALE FOR SUCH PRODUCTS,
INTEL ASSUMES NO LIABILITY WHATSOEVER AND INTEL DISCLAIMS ANY EXPRESS OR IMPLIED WARRANTY,
RELATING TO SALE AND/OR USE OF INTEL PRODUCTS INCLUDING LIABILITY OR WARRANTIES RELATING TO FITNESS FOR A PARTICULAR PURPOSE,
MERCHANTABILITY, OR INFRINGEMENT OF ANY PATENT, COPYRIGHT OR OTHER INTELLECTUAL PROPERTY RIGHT.

A "Mission Critical Application" is any application in which failure of the Intel Product could result, directly or indirectly, in personal injury or death.
SHOULD YOU PURCHASE OR USE INTEL'S PRODUCTS FOR ANY SUCH MISSION CRITICAL APPLICATION, YOU SHALL INDEMNIFY AND HOLD INTEL AND ITS SUBSIDIARIES,
SUBCONTRACTORS AND AFFILIATES, AND THE DIRECTORS, OFFICERS, AND EMPLOYEES OF EACH, HARMLESS AGAINST ALL CLAIMS COSTS, DAMAGES,
AND EXPENSES AND REASONABLE ATTORNEYS' FEES ARISING OUT OF, DIRECTLY OR INDIRECTLY, ANY CLAIM OF PRODUCT LIABILITY, PERSONAL INJURY,
OR DEATH ARISING IN ANY WAY OUT OF SUCH MISSION CRITICAL APPLICATION,
WHETHER OR NOT INTEL OR ITS SUBCONTRACTOR WAS NEGLIGENT IN THE DESIGN,
MANUFACTURE, OR WARNING OF THE INTEL PRODUCT OR ANY OF ITS PARTS.

Intel may make changes to specifications and product descriptions at any time, without notice.
Designers must not rely on the absence or characteristics of any features or instructions marked "reserved" or "undefined".
Intel reserves these for future definition and shall have no responsibility whatsoever for conflicts or incompatibilities arising from future changes to them.
The information here is subject to change without notice.
Do not finalize a design with this information.

The products described in this document may contain design defects or errors known as errata which may cause the product to deviate from published specifications.
Current characterized errata are available on request.

Contact your local Intel sales office or your distributor to obtain the latest specifications and before placing your product order.

Copies of documents which have an order number and are referenced in this document, or other Intel literature, may be obtained by calling 1-800-548-4725, or go to:
`http://www.intel.com/design/literature.htm <http://www.intel.com/design/literature.htm>`_.

Intel and the Intel logo are trademarks or registered trademarks of Intel Corporation or its subsidiaries in the United States and other countries.

\*Other names and brands may be claimed as the property of others.

Copyright © 2012 - 2014, Intel Corporation. All rights reserved.

**Contents**

.. toctree::
    :maxdepth: 2
    :numbered:

    intro
    cmd_line
    exception_path
    hello_world
    ip_frag
    ipv4_multicast
    ip_reassembly
    kernel_nic_interface
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
    vmdq_dcb_forwarding
    vhost
    netmap_compatibility
    internet_proto_ip_pipeline
    test_pipeline
    dist_app

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

**Tables**

:ref:`Table 1.Output Traffic Marking <table_1>`

:ref:`Table 2.Entity Types <table_2>`

:ref:`Table 3.Table Types <table_3>`
