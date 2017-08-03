..  BSD LICENSE
    Copyright(c) 2010-2015 Intel Corporation.
    Copyright(c) 2017 Mellanox Corporation.
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

.. _linux_gsg_linux_drivers:

Linux Drivers
=============

Different PMDs may require different kernel drivers in order to work properly.
Depends on the PMD being used, a corresponding kernel driver should be load
and bind to the network ports.

UIO
---

A small kernel module to set up the device, map device memory to user-space and register interrupts.
In many cases, the standard ``uio_pci_generic`` module included in the Linux kernel
can provide the uio capability. This module can be loaded using the command:

.. code-block:: console

    sudo modprobe uio_pci_generic

.. note::

    ``uio_pci_generic`` module doesn't support the creation of virtual functions.

As an alternative to the ``uio_pci_generic``, the DPDK also includes the igb_uio
module which can be found in the kmod subdirectory referred to above. It can
be loaded as shown below:

.. code-block:: console

    sudo modprobe uio
    sudo insmod kmod/igb_uio.ko

.. note::

    For some devices which lack support for legacy interrupts, e.g. virtual function
    (VF) devices, the ``igb_uio`` module may be needed in place of ``uio_pci_generic``.

.. note::

   If UEFI secure boot is enabled, the Linux kernel may disallow the use of
   UIO on the system. Therefore, devices for use by DPDK should be bound to the
   ``vfio-pci`` kernel module rather than ``igb_uio`` or ``uio_pci_generic``.
   For more details see :ref:`linux_gsg_binding_kernel` below.

Since DPDK release 1.7 onward provides VFIO support, use of UIO is optional
for platforms that support using VFIO.

VFIO
----

A more robust and secure driver in compare to the ``UIO``, relying on IOMMU protection.
To make use of VFIO, the ``vfio-pci`` module must be loaded:

.. code-block:: console

    sudo modprobe vfio-pci

Note that in order to use VFIO, your kernel must support it.
VFIO kernel modules have been included in the Linux kernel since version 3.6.0 and are usually present by default,
however please consult your distributions documentation to make sure that is the case.

Also, to use VFIO, both kernel and BIOS must support and be configured to use IO virtualization (such as Intel® VT-d).

.. note::

    ``vfio-pci`` module doesn't support the creation of virtual functions.

For proper operation of VFIO when running DPDK applications as a non-privileged user, correct permissions should also be set up.
This can be done by using the DPDK setup script (called dpdk-setup.sh and located in the usertools directory).

.. note::

    VFIO can be used without IOMMU. While this is just as unsafe as using UIO, it does make it possible for the user to keep the degree of device access and programming that VFIO has, in situations where IOMMU is not available.

Bifurcated Driver
-----------------

PMDs which use the bifurcated driver co-exists with the device kernel driver.
On such model the NIC is controlled by the kernel, while the data
path is performed by the PMD directly on top of the device.

Such model has the following benefits:

 - It is secure and robust, as the memory management and isolation
   is done by the kernel.
 - It enables the user to use legacy linux tools such as ``ethtool`` or
   ``ifconfig`` while running DPDK application on the same network ports.
 - It enables the DPDK application to filter only part of the traffic,
   While the rest will be directed and handled by the kernel driver.

More about the bifurcated driver can be found in
`Mellanox Bifurcated DPDK PMD
<https://dpdksummit.com/Archive/pdf/2016Userspace/Day02-Session04-RonyEfraim-Userspace2016.pdf>`__.

.. _linux_gsg_binding_kernel:

Binding and Unbinding Network Ports to/from the Kernel Modules
--------------------------------------------------------------

.. note::

    PMDs Which use the bifurcated driver should not be unbind from their kernel drivers. this section is for PMDs which use the UIO or VFIO drivers.

As of release 1.4, DPDK applications no longer automatically unbind all supported network ports from the kernel driver in use.
Instead, in case the PMD being used use the UIO or VFIO drivers, all ports that are to be used by an DPDK application must be bound to the
``uio_pci_generic``, ``igb_uio`` or ``vfio-pci`` module before the application is run.
For such PMDs, any network ports under Linux* control will be ignored and cannot be used by the application.

To bind ports to the ``uio_pci_generic``, ``igb_uio`` or ``vfio-pci`` module for DPDK use,
and then subsequently return ports to Linux* control,
a utility script called dpdk-devbind.py is provided in the usertools subdirectory.
This utility can be used to provide a view of the current state of the network ports on the system,
and to bind and unbind those ports from the different kernel modules, including the uio and vfio modules.
The following are some examples of how the script can be used.
A full description of the script and its parameters can be obtained by calling the script with the ``--help`` or ``--usage`` options.
Note that the uio or vfio kernel modules to be used, should be loaded into the kernel before
running the ``dpdk-devbind.py`` script.

.. warning::

    Due to the way VFIO works, there are certain limitations to which devices can be used with VFIO.
    Mainly it comes down to how IOMMU groups work.
    Any Virtual Function device can be used with VFIO on its own, but physical devices will require either all ports bound to VFIO,
    or some of them bound to VFIO while others not being bound to anything at all.

    If your device is behind a PCI-to-PCI bridge, the bridge will then be part of the IOMMU group in which your device is in.
    Therefore, the bridge driver should also be unbound from the bridge PCI device for VFIO to work with devices behind the bridge.

.. warning::

    While any user can run the dpdk-devbind.py script to view the status of the network ports,
    binding or unbinding network ports requires root privileges.

To see the status of all network ports on the system:

.. code-block:: console

    ./usertools/dpdk-devbind.py --status

    Network devices using DPDK-compatible driver
    ============================================
    0000:82:00.0 '82599EB 10-GbE NIC' drv=uio_pci_generic unused=ixgbe
    0000:82:00.1 '82599EB 10-GbE NIC' drv=uio_pci_generic unused=ixgbe

    Network devices using kernel driver
    ===================================
    0000:04:00.0 'I350 1-GbE NIC' if=em0  drv=igb unused=uio_pci_generic *Active*
    0000:04:00.1 'I350 1-GbE NIC' if=eth1 drv=igb unused=uio_pci_generic
    0000:04:00.2 'I350 1-GbE NIC' if=eth2 drv=igb unused=uio_pci_generic
    0000:04:00.3 'I350 1-GbE NIC' if=eth3 drv=igb unused=uio_pci_generic

    Other network devices
    =====================
    <none>

To bind device ``eth1``,``04:00.1``, to the ``uio_pci_generic`` driver:

.. code-block:: console

    ./usertools/dpdk-devbind.py --bind=uio_pci_generic 04:00.1

or, alternatively,

.. code-block:: console

    ./usertools/dpdk-devbind.py --bind=uio_pci_generic eth1

To restore device ``82:00.0`` to its original kernel binding:

.. code-block:: console

    ./usertools/dpdk-devbind.py --bind=ixgbe 82:00.0
