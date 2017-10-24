..  BSD LICENSE
    Copyright(c) 2015 IGEL Co.,Ltd. All rights reserved.
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
    * Neither the name of IGEL Co.,Ltd. nor the names of its
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

Port Hotplug Framework
======================

The Port Hotplug Framework provides DPDK applications with the ability to
attach and detach ports at runtime. Because the framework depends on PMD
implementation, the ports that PMDs cannot handle are out of scope of this
framework. Furthermore, after detaching a port from a DPDK application, the
framework doesn't provide a way for removing the devices from the system.
For the ports backed by a physical NIC, the kernel will need to support PCI
Hotplug feature.

Overview
--------

The basic requirements of the Port Hotplug Framework are:

*       DPDK applications that use the Port Hotplug Framework must manage their
        own ports.

        The Port Hotplug Framework is implemented to allow DPDK applications to
        manage ports. For example, when DPDK applications call the port attach
        function, the attached port number is returned. DPDK applications can
        also detach the port by port number.

*       Kernel support is needed for attaching or detaching physical device
        ports.

        To attach new physical device ports, the device will be recognized by
        userspace driver I/O framework in kernel at first. Then DPDK
        applications can call the Port Hotplug functions to attach the ports.
        For detaching, steps are vice versa.

*       Before detaching, they must be stopped and closed.

        DPDK applications must call "rte_eth_dev_stop()" and
        "rte_eth_dev_close()" APIs before detaching ports. These functions will
        start finalization sequence of the PMDs.

*       The framework doesn't affect legacy DPDK applications behavior.

        If the Port Hotplug functions aren't called, all legacy DPDK apps can
        still work without modifications.

Port Hotplug API overview
-------------------------

*       Attaching a port

        "rte_eth_dev_attach()" API attaches a port to DPDK application, and
        returns the attached port number. Before calling the API, the device
        should be recognized by an userspace driver I/O framework. The API
        receives a pci address like "0000:01:00.0" or a virtual device name
        like "net_pcap0,iface=eth0". In the case of virtual device name, the
        format is the same as the general "--vdev" option of DPDK.

*       Detaching a port

        "rte_eth_dev_detach()" API detaches a port from DPDK application, and
        returns a pci address of the detached device or a virtual device name
        of the device.

Reference
---------

        "testpmd" supports the Port Hotplug Framework.

Limitations
-----------

*       The Port Hotplug APIs are not thread safe.

*       The framework can only be enabled with Linux. BSD is not supported.

*       Not all PMDs support detaching feature.
        The underlying bus must support hot-unplug. If not supported,
        the function ``rte_eth_dev_detach()`` will return negative ENOTSUP.
