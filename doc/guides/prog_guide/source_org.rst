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

**Part 2: Development Environment**

Source Organization
===================

This section describes the organization of sources in the DPDK framework.

Makefiles and Config
--------------------

.. note::

    In the following descriptions,
    RTE_SDK is the environment variable that points to the base directory into which the tarball was extracted.
    See
    :ref:`Useful Variables Provided by the Build System <Useful_Variables_Provided_by_the_Build_System>`
    for descriptions of other variables.

Makefiles that are provided by the DPDK libraries and applications are located in $(RTE_SDK)/mk.

Config templates are located in $(RTE_SDK)/config. The templates describe the options that are enabled for each target.
The config file also contains items that can be enabled and disabled for many of the DPDK libraries,
including debug options.
The user should look at the config file and become familiar with the options.
The config file is also used to create a header file, which will be located in the new build directory.

Libraries
---------

Libraries are located in subdirectories of $(RTE_SDK)/lib.
By convention, we call a library any code that provides an API to an application.
Typically, it generates an archive file (.a), but a kernel module should also go in the same directory.

The lib directory contains::

    lib
    +-- librte_cmdline      # command line interface helper
    +-- librte_distributor  # packet distributor
    +-- librte_eal          # environment abstraction layer
    +-- librte_ether        # generic interface to poll mode driver
    +-- librte_hash         # hash library
    +-- librte_ip_frag      # IP fragmentation library
    +-- librte_ivshmem      # QEMU IVSHMEM library
    +-- librte_kni          # kernel NIC interface
    +-- librte_kvargs       # argument parsing library
    +-- librte_lpm          # longest prefix match library
    +-- librte_malloc       # malloc-like functions
    +-- librte_mbuf         # packet and control mbuf manipulation library
    +-- librte_mempool      # memory pool manager (fixedsized objects)
    +-- librte_meter        # QoS metering library
    +-- librte_net          # various IP-related headers
    +-- librte_pmd_bond     # bonding poll mode driver
    +-- librte_pmd_e1000    # 1GbE poll mode drivers (igb and em)
    +-- librte_pmd_fm10k    # Host interface PMD driver for FM10000 Series
    +-- librte_pmd_ixgbe    # 10GbE poll mode driver
    +-- librte_pmd_i40e     # 40GbE poll mode driver
    +-- librte_pmd_mlx4     # Mellanox ConnectX-3 poll mode driver
    +-- librte_pmd_pcap     # PCAP poll mode driver
    +-- librte_pmd_ring     # ring poll mode driver
    +-- librte_pmd_virtio   # virtio poll mode driver
    +-- librte_pmd_vmxnet3  # VMXNET3 poll mode driver
    +-- librte_pmd_xenvirt  # Xen virtio poll mode driver
    +-- librte_power        # power management library
    +-- librte_ring         # software rings (act as lockless FIFOs)
    +-- librte_sched        # QoS scheduler and dropper library
    +-- librte_timer        # timer library

Applications
------------

Applications are sources that contain a main() function.
They are located in the $(RTE_SDK)/app and $(RTE_SDK)/examples directories.

The app directory contains sample applications that are used to test the DPDK (autotests).
The examples directory contains sample applications that show how libraries can be used.

::

    app
    +-- chkincs            # test prog to check include depends
    +-- test               # autotests, to validate DPDK features
    `-- test-pmd           # test and bench poll mode driver examples

    examples
    +-- cmdline            # Example of using cmdline library
    +-- dpdk_qat           # Example showing integration with Intel QuickAssist
    +-- exception_path     # Sending packets to and from Linux ethernet device (TAP)
    +-- helloworld         # Helloworld basic example
    +-- ip_reassembly      # Example showing IP Reassembly
    +-- ip_fragmentation   # Example showing IPv4 Fragmentation
    +-- ipv4_multicast     # Example showing IPv4 Multicast
    +-- kni                # Kernel NIC Interface example
    +-- l2fwd              # L2 Forwarding example with and without SR-IOV
    +-- l3fwd              # L3 Forwarding example
    +-- l3fwd-power        # L3 Forwarding example with power management
    +-- l3fwd-vf           # L3 Forwarding example with SR-IOV
    +-- link_status_interrupt # Link status change interrupt example
    +-- load_balancer      # Load balancing across multiple cores/sockets
    +-- multi_process      # Example applications with multiple DPDK processes
    +-- qos_meter          # QoS metering example
    +-- qos_sched          # QoS scheduler and dropper example
    +-- timer              # Example of using librte_timer library
    +-- vmdq_dcb           # Intel 82599 Ethernet Controller VMDQ and DCB receiving
    +-- vmdq               # Example of VMDQ receiving for both Intel 10G (82599) and 1G (82576, 82580 and I350) Ethernet Controllers
    `-- vhost              # Example of userspace vhost and switch

.. note::

    The actual examples directory may contain additional sample applications to those shown above.
    Check the latest DPDK source files for details.
