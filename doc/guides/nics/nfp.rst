..  BSD LICENSE
    Copyright(c) 2015-2017 Netronome Systems, Inc. All rights reserved.
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
up to 400-Gb/s.

This document explains how to use DPDK with the Netronome Poll Mode
Driver (PMD) supporting Netronome's Network Flow Processor 6xxx
(NFP-6xxx) and Netronome's Flow Processor 4xxx (NFP-4xxx).

NFP is a SRIOV capable device and the PMD driver supports the physical
function (PF) and the virtual functions (VFs).

Dependencies
------------

Before using the Netronome's DPDK PMD some NFP configuration,
which is not related to DPDK, is required. The system requires
installation of **Netronome's BSP (Board Support Package)** along
with a specific NFP firmware application. Netronome's NSP ABI
version should be 0.20 or higher.

If you have a NFP device you should already have the code and
documentation for this configuration. Contact
**support@netronome.com** to obtain the latest available firmware.

The NFP Linux netdev kernel driver for VFs has been a part of the
vanilla kernel since kernel version 4.5, and support for the PF
since kernel version 4.11. Support for older kernels can be obtained
on Github at
**https://github.com/Netronome/nfp-drv-kmods** along with the build
instructions.

NFP PMD needs to be used along with UIO ``igb_uio`` or VFIO (``vfio-pci``)
Linux kernel driver.

Building the software
---------------------

Netronome's PMD code is provided in the **drivers/net/nfp** directory.
Although NFP PMD has Netronome´s BSP dependencies, it is possible to
compile it along with other DPDK PMDs even if no BSP was installed previously.
Of course, a DPDK app will require such a BSP installed for using the
NFP PMD, along with a specific NFP firmware application.

Default PMD configuration is at the **common_linuxapp configuration** file:

- **CONFIG_RTE_LIBRTE_NFP_PMD=y**

Once the DPDK is built all the DPDK apps and examples include support for
the NFP PMD.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Using the PF
------------

NFP PMD supports using the NFP PF as another DPDK port, but it does not
have any functionality for controlling VFs. In fact, it is not possible to use
the PMD with the VFs if the PF is being used by DPDK, that is, with the NFP PF
bound to ``igb_uio`` or ``vfio-pci`` kernel drivers. Future DPDK versions will
have a PMD able to work with the PF and VFs at the same time and with the PF
implementing VF management along with other PF-only functionalities/offloads.

The PMD PF has extra work to do which will delay the DPDK app initialization
like uploading the firmware and configure the Link state properly when starting or
stopping a PF port. Since DPDK 18.05 the firmware upload happens when
a PF is initialized, which was not always true with older DPDK versions.

Depending on the Netronome product installed in the system, firmware files
should be available under ``/lib/firmware/netronome``. DPDK PMD supporting the
PF looks for a firmware file in this order:

	1) First try to find a firmware image specific for this device using the
	   NFP serial number:

		serial-00-15-4d-12-20-65-10-ff.nffw

	2) Then try the PCI name:

		pci-0000:04:00.0.nffw

	3) Finally try the card type and media:

		nic_AMDA0099-0001_2x25.nffw

Netronome's software packages install firmware files under ``/lib/firmware/netronome``
to support all the Netronome's SmartNICs and different firmware applications.
This is usually done using file names based on SmartNIC type and media and with a
directory per firmware application. Options 1 and 2 for firmware filenames allow
more than one SmartNIC, same type of SmartNIC or different ones, and to upload a
different firmware to each SmartNIC.


PF multiport support
--------------------

Some NFP cards support several physical ports with just one single PCI device.
The DPDK core is designed with a 1:1 relationship between PCI devices and DPDK
ports, so NFP PMD PF support requires handling the multiport case specifically.
During NFP PF initialization, the PMD will extract the information about the
number of PF ports from the firmware and will create as many DPDK ports as
needed.

Because the unusual relationship between a single PCI device and several DPDK
ports, there are some limitations when using more than one PF DPDK port: there
is no support for RX interrupts and it is not possible either to use those PF
ports with the device hotplug functionality.


System configuration
--------------------

#. **Enable SR-IOV on the NFP device:** The current NFP PMD supports the PF and
   the VFs on a NFP device. However, it is not possible to work with both at the
   same time because the VFs require the PF being bound to the NFP PF Linux
   netdev driver.  Make sure you are working with a kernel with NFP PF support or
   get the drivers from the above Github repository and follow the instructions
   for building and installing it.

   VFs need to be enabled before they can be used with the PMD.
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
