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

Description of Release
======================

These release notes cover the new features,
fixed bugs and known issues for Data Plane Development Kit (DPDK) release version 2.0.0.

For instructions on compiling and running the release, see the *DPDK Getting Started Guide*.

Using DPDK Upgrade Patches
--------------------------

For minor updates to the main DPDK releases, the software may be made available both as a new full package and as a patch file to be applied to the previously released package.
In the latter case, the following commands should be used to apply the patch on top of the already-installed package for the previous release:

.. code-block:: console

    # cd $RTE_SDK
    # patch –p1 < /path/to/patch/file

Once the patch has been applied cleanly, the DPDK can be recompiled and used as before (described in the *DPDK Getting Started Guide*).

.. note::

    If the patch does not apply cleanly, perhaps because of modifications made locally to the software,
    it is recommended to use the full release package for the minor update, instead of using the patch.

Documentation Roadmap
---------------------

The following is a list of DPDK documents in the suggested reading order:

*   **Release Notes**
    (this document): Provides release-specific information, including supported features, limitations, fixed issues, known issues and so on.
    Also, provides the answers to frequently asked questions in FAQ format.

*   **Getting Started Guide**
    : Describes how to install and configure the DPDK software; designed to get users up and running quickly with the software.

*   **FreeBSD* Getting Started Guide**
    : A document describing the use of the DPDK with FreeBSD* has been added in DPDK Release 1.6.0.
    Refer to this guide for installation and configuration instructions to get started using the DPDK with FreeBSD*.

*   **Programmer's Guide**
    : Describes:

    *   The software architecture and how to use it (through examples), specifically in a Linux* application (linuxapp) environment

    *   The content of the DPDK, the build system (including the commands that can be used in the root DPDK Makefile to build the development kit and an application)
        and guidelines for porting an application

    *   Optimizations used in the software and those that should be considered for new development

    A glossary of terms is also provided.

*   **API Reference**
    : Provides detailed information about DPDK functions, data structures and other programming constructs.

*   **Sample Applications User Guide**
    : Describes a set of sample applications. Each chapter describes a sample application that showcases specific functionality and provides instructions on how to compile,
    run and use the sample application.

    The following sample applications are included:

    *   Command Line

    *   Exception Path (into Linux* for packets using the Linux TUN/TAP driver)

    *   Hello World

    *   Integration with Intel® QuickAssist Technology

    *   Link Status Interrupt (Ethernet* Link Status Detection)

    *   IP Reassembly

    *   IP Pipeline

    *   IP Fragmentation

    *   IPv4 Multicast

    *   L2 Forwarding (supports virtualized and non-virtualized environments)

    *   L2 Forwarding IVSHMEM

    *   L2 Forwarding Jobstats

    *   L3 Forwarding

    *   L3 Forwarding with Access Control

    *   L3 Forwarding with Power Management

    *   L3 Forwarding in a Virtualized Environment

    *   Link Bonding

    *   Link Status Interrupt

    *   Load Balancing

    *   Multi-process

    *   QoS Scheduler + Dropper

    *   QoS Metering

    *   Quota & Watermarks

    *   Timer

    *   VMDQ and DCB L2 Forwarding

    *   VMDQ L2 Forwarding

    *   Userspace vhost

    *   Userspace vhost switch

    *   Netmap

    *   Kernel NIC Interface (KNI)

    *   VM Power Management

    *   Distributor

    *   RX-TX Callbacks

    *   Skeleton

    In addition, there are some other applications that are built when the libraries are created.
    The source for these applications is in the DPDK/app directory and are called:

    *   test

    *   testpmd

    Once the libraries are created, they can be found in the build/app directory.

    *   The test application provides a variety of specific tests for the various functions in the DPDK.

    *   The testpmd application provides a number of different packet throughput tests and examples of features such as
        how to use the Flow Director found in the Intel® 82599 10 Gigabit Ethernet Controller.

    The testpmd application is documented in the *DPDK Testpmd Application Note*.
    The test application is not currently documented.
    However, you should be able to run and use test application with the command line help that is provided in the application.
