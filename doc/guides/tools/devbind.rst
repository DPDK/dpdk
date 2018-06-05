..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2016 Canonical Limited. All rights reserved.

dpdk-devbind Application
========================

The ``dpdk-devbind`` tool is a Data Plane Development Kit (DPDK) utility
that helps binding and unbinding devices from specific drivers.
As well as checking their status in that regard.


Running the Application
-----------------------

The tool has a number of command line options:

.. code-block:: console

   dpdk-devbind [options] DEVICE1 DEVICE2 ....

OPTIONS
-------

* ``--help, --usage``

        Display usage information and quit

* ``-s, --status``

        Print the current status of all known network interfaces.
        For each device, it displays the PCI domain, bus, slot and function,
        along with a text description of the device. Depending upon whether the
        device is being used by a kernel driver, the ``igb_uio`` driver, or no
        driver, other relevant information will be displayed:
        - the Linux interface name e.g. ``if=eth0``
        - the driver being used e.g. ``drv=igb_uio``
        - any suitable drivers not currently using that device e.g. ``unused=igb_uio``
        NOTE: if this flag is passed along with a bind/unbind option, the
        status display will always occur after the other operations have taken
        place.

* ``-b driver, --bind=driver``

        Select the driver to use or "none" to unbind the device

* ``-u, --unbind``

        Unbind a device (Equivalent to ``-b none``)

* ``--force``

        By default, devices which are used by Linux - as indicated by having
        routes in the routing table - cannot be modified. Using the ``--force``
        flag overrides this behavior, allowing active links to be forcibly
        unbound.
        WARNING: This can lead to loss of network connection and should be used
        with caution.


.. warning::

    Due to the way VFIO works, there are certain limitations to which devices can be used with VFIO.
    Mainly it comes down to how IOMMU groups work.
    Any Virtual Function device can be used with VFIO on its own, but physical devices will require either all ports bound to VFIO,
    or some of them bound to VFIO while others not being bound to anything at all.

    If your device is behind a PCI-to-PCI bridge, the bridge will then be part of the IOMMU group in which your device is in.
    Therefore, the bridge driver should also be unbound from the bridge PCI device for VFIO to work with devices behind the bridge.

.. warning::

    While any user can run the ``dpdk-devbind.py`` script to view the status of the network ports,
    binding or unbinding network ports requires root privileges.


Examples
--------

To display current device status::

   dpdk-devbind --status

To bind eth1 from the current driver and move to use igb_uio::

   dpdk-devbind --bind=igb_uio eth1

To unbind 0000:01:00.0 from using any driver::

   dpdk-devbind -u 0000:01:00.0

To bind 0000:02:00.0 and 0000:02:00.1 to the ixgbe kernel driver::

   dpdk-devbind -b ixgbe 02:00.0 02:00.1

To check status of all network ports, assign one to the igb_uio driver and check status again::

   # Check the status of the available devices.
   dpdk-devbind --status
   Network devices using DPDK-compatible driver
   ============================================
   <none>

   Network devices using kernel driver
   ===================================
   0000:0a:00.0 '82599ES 10-Gigabit' if=eth2 drv=ixgbe unused=


   # Bind the device to igb_uio.
   sudo dpdk-devbind -b igb_uio 0000:0a:00.0


   # Recheck the status of the devices.
   dpdk-devbind --status
   Network devices using DPDK-compatible driver
   ============================================
   0000:0a:00.0 '82599ES 10-Gigabit' drv=igb_uio unused=
