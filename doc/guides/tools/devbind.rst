..  BSD LICENSE
    Copyright(c) 2016 Canonical Limited. All rights reserved.

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
