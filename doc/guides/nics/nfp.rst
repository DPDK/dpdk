..  BSD LICENSE
    Copyright(c) 2015 Netronome Systems, Inc. All rights reserved.
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

NFP poll mode driver library
============================

Netronome's sixth generation of flow processors pack 216 programmable
cores and over 100 hardware accelerators that uniquely combine packet,
flow, security and content processing in a single device that scales
up to 400 Gbps.

This document explains how to use DPDK with the Netronome Poll Mode
Driver (PMD) supporting Netronome's Network Flow Processor 6xxx
(NFP-6xxx).

Currently the driver supports virtual functions (VFs) only.

Dependencies
------------

Before using the Netronome's DPDK PMD some NFP-6xxx configuration,
which is not related to DPDK, is required. The system requires
installation of **Netronome's BSP (Board Support Package)** which includes
Linux drivers, programs and libraries.

If you have a NFP-6xxx device you should already have the code and
documentation for doing this configuration. Contact
**support@netronome.com** to obtain the latest available firmware.

The NFP Linux kernel drivers (including the required PF driver for the
NFP) are available on Github at
**https://github.com/Netronome/nfp-drv-kmods** along with build
instructions.

DPDK runs in userspace and PMDs uses the Linux kernel UIO interface to
allow access to physical devices from userspace. The NFP PMD requires
the **igb_uio** UIO driver, available with DPDK, to perform correct
initialization.

Building the software
---------------------

Netronome's PMD code is provided in the **drivers/net/nfp** directory.
Although NFP PMD has Netronome´s BSP dependencies, it is possible to
compile it along with other DPDK PMDs even if no BSP was installed before.
Of course, a DPDK app will require such a BSP installed for using the
NFP PMD.

Default PMD configuration is at **common_linuxapp configuration** file:

- **CONFIG_RTE_LIBRTE_NFP_PMD=y**

Once DPDK is built all the DPDK apps and examples include support for
the NFP PMD.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.


System configuration
--------------------

#. **Enable SR-IOV on the NFP-6xxx device:** The current NFP PMD works with
   Virtual Functions (VFs) on a NFP device. Make sure that one of the Physical
   Function (PF) drivers from the above Github repository is installed and
   loaded.

   Virtual Functions need to be enabled before they can be used with the PMD.
   Before enabling the VFs it is useful to obtain information about the
   current NFP PCI device detected by the system:

   .. code-block:: console

      lspci -d19ee:

   Now, for example, configure two virtual functions on a NFP-6xxx device
   whose PCI system identity is "0000:03:00.0":

   .. code-block:: console

      echo 2 > /sys/bus/pci/devices/0000:03:00.0/sriov_numvfs

   The result of this command may be shown using lspci again:

   .. code-block:: console

      lspci -d19ee: -k

   Two new PCI devices should appear in the output of the above command. The
   -k option shows the device driver, if any, that devices are bound to.
   Depending on the modules loaded at this point the new PCI devices may be
   bound to nfp_netvf driver.
