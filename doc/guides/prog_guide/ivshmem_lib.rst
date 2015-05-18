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

IVSHMEM Library
===============

The DPDK IVSHMEM library facilitates fast zero-copy data sharing among virtual machines
(host-to-guest or guest-to-guest) by means of QEMU's IVSHMEM mechanism.

The library works by providing a command line for QEMU to map several hugepages into a single IVSHMEM device.
For the guest to know what is inside any given IVSHMEM device
(and to distinguish between DPDK and non-DPDK IVSHMEM devices),
a metadata file is also mapped into the IVSHMEM segment.
No work needs to be done by the guest application to map IVSHMEM devices into memory;
they are automatically recognized by the DPDK Environment Abstraction Layer (EAL).

A typical DPDK IVSHMEM use case looks like the following.


.. figure:: img/ivshmem.*

   Typical Ivshmem use case


The same could work with several virtual machines, providing host-to-VM or VM-to-VM communication.
The maximum number of metadata files is 32 (by default) and each metadata file can contain different (or even the same) hugepages.
The only constraint is that each VM has to have access to the memory it is sharing with other entities (be it host or another VM).
For example, if the user wants to share the same memzone across two VMs, each VM must have that memzone in its metadata file.

IVHSHMEM Library API Overview
-----------------------------

The following is a simple guide to using the IVSHMEM Library API:

*   Call rte_ivshmem_metadata_create() to create a new metadata file.
    The metadata name is used to distinguish between multiple metadata files.

*   Populate each metadata file with DPDK data structures.
    This can be done using the following API calls:

    *   rte_ivhshmem_metadata_add_memzone() to add rte_memzone to metadata file

    *   rte_ivshmem_metadata_add_ring() to add rte_ring to metadata file

    *   rte_ivshmem_metadata_add_mempool() to add rte_mempool to metadata file

*   Finally, call rte_ivshmem_metadata_cmdline_generate() to generate the command line for QEMU.
    Multiple metadata files (and thus multiple command lines) can be supplied to a single VM.

.. note::

    Only data structures fully residing in DPDK hugepage memory work correctly.
    Supported data structures created by malloc(), mmap()
    or otherwise using non-DPDK memory cause undefined behavior and even a segmentation fault.

IVSHMEM Environment Configuration
---------------------------------

The steps needed to successfully run IVSHMEM applications are the following:

*   Compile a special version of QEMU from sources.

    The source code can be found on the QEMU website (currently, version 1.4.x is supported, but version 1.5.x is known to work also),
    however, the source code will need to be patched to support using regular files as the IVSHMEM memory backend.
    The patch is not included in the DPDK package,
    but is available on the `Intel®DPDK-vswitch project webpage <https://01.org/packet-processing/intel%C2%AE-ovdk>`_
    (either separately or in a DPDK vSwitch package).

*   Enable IVSHMEM library in the DPDK build configuration.

    In the default configuration, IVSHMEM library is not compiled. To compile the IVSHMEM library,
    one has to either use one of the provided IVSHMEM targets
    (for example, x86_64-ivshmem-linuxapp-gcc),
    or set CONFIG_RTE_LIBRTE_IVSHMEM to "y" in the build configuration.

*   Set up hugepage memory on the virtual machine.

    The guest applications run as regular DPDK (primary) processes and thus need their own hugepage memory set up inside the VM.
    The process is identical to the one described in the *DPDK Getting Started Guide*.

Best Practices for Writing IVSHMEM Applications
-----------------------------------------------

When considering the use of IVSHMEM for sharing memory, security implications need to be carefully evaluated.
IVSHMEM is not suitable for untrusted guests, as IVSHMEM is essentially a window into the host process memory.
This also has implications for the multiple VM scenarios.
While the IVSHMEM library tries to share as little memory as possible,
it is quite probable that data designated for one VM might also be present in an IVSMHMEM device designated for another VM.
Consequently, any shared memory corruption will affect both host and all VMs sharing that particular memory.

IVSHMEM applications essentially behave like multi-process applications,
so it is important to implement access serialization to data and thread safety.
DPDK ring structures are already thread-safe, however,
any custom data structures that the user might need would have to be thread-safe also.

Similar to regular DPDK multi-process applications,
it is not recommended to use function pointers as functions might have different memory addresses in different processes.

It is best to avoid freeing the rte_mbuf structure on a different machine from where it was allocated,
that is, if the mbuf was allocated on the host, the host should free it.
Consequently, any packet transmission and reception should also happen on the same machine (whether virtual or physical).
Failing to do so may lead to data corruption in the mempool cache.

Despite the IVSHMEM mechanism being zero-copy and having good performance,
it is still desirable to do processing in batches and follow other procedures described in
:ref:`Performance Optimization <Performance_Optimization>`.

Best Practices for Running IVSHMEM Applications
-----------------------------------------------

For performance reasons,
it is best to pin host processes and QEMU processes to different cores so that they do not interfere with each other.
If NUMA support is enabled, it is also desirable to keep host process' hugepage memory and QEMU process on the same NUMA node.

For the best performance across all NUMA nodes, each QEMU core should be pinned to host CPU core on the appropriate NUMA node.
QEMU's virtual NUMA nodes should also be set up to correspond to physical NUMA nodes.
More on how to set up DPDK and QEMU NUMA support can be found in *DPDK Getting Started Guide* and
`QEMU documentation <http://qemu.weilnetz.de/qemu-doc.html>`_ respectively.
A script called cpu_layout.py is provided with the DPDK package (in the tools directory)
that can be used to identify which CPU cores correspond to which NUMA node.

The QEMU IVSHMEM command line creation should be considered the last step before starting the virtual machine.
Currently, there is no hot plug support for QEMU IVSHMEM devices,
so one cannot add additional memory to an IVSHMEM device once it has been created.
Therefore, the correct sequence to run an IVSHMEM application is to run host application first,
obtain the command lines for each IVSHMEM device and then run all QEMU instances with guest applications afterwards.

It is important to note that once QEMU is started, it holds on to the hugepages it uses for IVSHMEM devices.
As a result, if the user wishes to shut down or restart the IVSHMEM host application,
it is not enough to simply shut the application down.
The virtual machine must also be shut down (if not, it will hold onto outdated host data).
