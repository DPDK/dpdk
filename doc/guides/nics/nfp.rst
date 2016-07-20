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
Because Netronome´s BSP dependencies the driver is disabled by default
in DPDK build using **common_linuxapp configuration** file. Enabling the
driver or if you use another configuration file and want to have NFP
support, this variable is needed:

- **CONFIG_RTE_LIBRTE_NFP_PMD=y**

Once DPDK is built all the DPDK apps and examples include support for
the NFP PMD.


System configuration
--------------------

Using the NFP PMD is not different to using other PMDs. Usual steps are:

#. **Configure hugepages:** All major Linux distributions have the hugepages
   functionality enabled by default. By default this allows the system uses for
   working with transparent hugepages. But in this case some hugepages need to
   be created/reserved for use with the DPDK through the hugetlbfs file system.
   First the virtual file system need to be mounted:

   .. code-block:: console

      mount -t hugetlbfs none /mnt/hugetlbfs

   The command uses the common mount point for this file system and it needs to
   be created if necessary.

   Configuring hugepages is performed via sysfs:

   .. code-block:: console

      /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   This sysfs file is used to specify the number of hugepages to reserve.
   For example:

   .. code-block:: console

      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   This will reserve 2GB of memory using 1024 2MB hugepages. The file may be
   read to see if the operation was performed correctly:

   .. code-block:: console

      cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

   The number of unused hugepages may also be inspected.

   Before executing the DPDK app it should match the value of nr_hugepages.

   .. code-block:: console

      cat /sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages

   The hugepages reservation should be performed at system initialization and
   it is usual to use a kernel parameter for configuration. If the reservation
   is attempted on a busy system it will likely fail. Reserving memory for
   hugepages may be done adding the following to the grub kernel command line:

   .. code-block:: console

      default_hugepagesz=1M hugepagesz=2M hugepages=1024

   This will reserve 2GBytes of memory using 2Mbytes huge pages.

   Finally, for a NUMA system the allocation needs to be made on the correct
   NUMA node. In a DPDK app there is a master core which will (usually) perform
   memory allocation. It is important that some of the hugepages are reserved
   on the NUMA memory node where the network device is attached. This is because
   of a restriction in DPDK by which TX and RX descriptors rings must be created
   on the master code.

   Per-node allocation of hugepages may be inspected and controlled using sysfs.
   For example:

   .. code-block:: console

      cat /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages

   For a NUMA system there will be a specific hugepage directory per node
   allowing control of hugepage reservation. A common problem may occur when
   hugepages reservation is performed after the system has been working for
   some time. Configuration using the global sysfs hugepage interface will
   succeed but the per-node allocations may be unsatisfactory.

   The number of hugepages that need to be reserved depends on how the app uses
   TX and RX descriptors, and packets mbufs.

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

#. **To install the uio kernel module (manually):** All major Linux
   distributions have support for this kernel module so it is straightforward
   to install it:

   .. code-block:: console

      modprobe uio

   The module should now be listed by the lsmod command.

#. **To install the igb_uio kernel module (manually):** This module is part
   of DPDK sources and configured by default (CONFIG_RTE_EAL_IGB_UIO=y).

   .. code-block:: console

      modprobe igb_uio.ko

   The module should now be listed by the lsmod command.

   Depending on which NFP modules are loaded, it could be necessary to
   detach NFP devices from the nfp_netvf module. If this is the case the
   device needs to be unbound, for example:

   .. code-block:: console

      echo 0000:03:08.0 > /sys/bus/pci/devices/0000:03:08.0/driver/unbind

      lspci -d19ee: -k

   The output of lspci should now show that 0000:03:08.0 is not bound to
   any driver.

   The next step is to add the NFP PCI ID to the IGB UIO driver:

   .. code-block:: console

      echo 19ee 6003 > /sys/bus/pci/drivers/igb_uio/new_id

   And then to bind the device to the igb_uio driver:

   .. code-block:: console

      echo 0000:03:08.0 > /sys/bus/pci/drivers/igb_uio/bind

      lspci -d19ee: -k

   lspci should show that device bound to igb_uio driver.

#. **Using scripts to install and bind modules:** DPDK provides scripts which are
   useful for installing the UIO modules and for binding the right device to those
   modules avoiding doing so manually:

   * **dpdk-setup.sh**
   * **dpdk-devbind.py**

   Configuration may be performed by running dpdk-setup.sh which invokes
   dpdk-devbind.py as needed. Executing dpdk-setup.sh will display a menu of
   configuration options.
