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

VM Power Management Application
===============================

Introduction
------------

Applications running in Virtual Environments have an abstract view of
the underlying hardware on the Host, in particular applications cannot see
the binding of virtual to physical hardware.
When looking at CPU resourcing, the pinning of Virtual CPUs(vCPUs) to
Host Physical CPUs(pCPUS) is not apparent to an application
and this pinning may change over time.
Furthermore, Operating Systems on virtual machines do not have the ability
to govern their own power policy; the Machine Specific Registers (MSRs)
for enabling P-State transitions are not exposed to Operating Systems
running on Virtual Machines(VMs).

The Virtual Machine Power Management solution shows an example of
how a DPDK application can indicate its processing requirements using VM local
only information(vCPU/lcore) to a Host based Monitor which is responsible
for accepting requests for frequency changes for a vCPU, translating the vCPU
to a pCPU via libvirt and affecting the change in frequency.

The solution is comprised of two high-level components:

#. Example Host Application

   Using a Command Line Interface(CLI) for VM->Host communication channel management
   allows adding channels to the Monitor, setting and querying the vCPU to pCPU pinning,
   inspecting and manually changing the frequency for each CPU.
   The CLI runs on a single lcore while the thread responsible for managing
   VM requests runs on a second lcore.

   VM requests arriving on a channel for frequency changes are passed
   to the librte_power ACPI cpufreq sysfs based library.
   The Host Application relies on both qemu-kvm and libvirt to function.

#. librte_power for Virtual Machines

   Using an alternate implementation for the librte_power API, requests for
   frequency changes are forwarded to the host monitor rather than
   the APCI cpufreq sysfs interface used on the host.

   The l3fwd-power application will use this implementation when deployed on a VM
   (see :doc:`l3_forward_power_man`).

.. _figure_vm_power_mgr_highlevel:

.. figure:: img/vm_power_mgr_highlevel.*

   Highlevel Solution


Overview
--------

VM Power Management employs qemu-kvm to provide communications channels
between the host and VMs in the form of Virtio-Serial which appears as
a paravirtualized serial device on a VM and can be configured to use
various backends on the host. For this example each Virtio-Serial endpoint
on the host is configured as AF_UNIX file socket, supporting poll/select
and epoll for event notification.
In this example each channel endpoint on the host is monitored via
epoll for EPOLLIN events.
Each channel is specified as qemu-kvm arguments or as libvirt XML for each VM,
where each VM can have a number of channels up to a maximum of 64 per VM,
in this example each DPDK lcore on a VM has exclusive access to a channel.

To enable frequency changes from within a VM, a request via the librte_power interface
is forwarded via Virtio-Serial to the host, each request contains the vCPU
and power command(scale up/down/min/max).
The API for host and guest librte_power is consistent across environments,
with the selection of VM or Host Implementation determined at automatically
at runtime based on the environment.

Upon receiving a request, the host translates the vCPU to a pCPU via
the libvirt API before forwarding to the host librte_power.

.. _figure_vm_power_mgr_vm_request_seq:

.. figure:: img/vm_power_mgr_vm_request_seq.*

   VM request to scale frequency


Performance Considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

While Haswell Microarchitecture allows for independent power control for each core,
earlier Microarchtectures do not offer such fine grained control.
When deployed on pre-Haswell platforms greater care must be taken in selecting
which cores are assigned to a VM, for instance a core will not scale down
until its sibling is similarly scaled.

Configuration
-------------

BIOS
~~~~

Enhanced Intel SpeedStep® Technology must be enabled in the platform BIOS
if the power management feature of DPDK is to be used.
Otherwise, the sys file folder /sys/devices/system/cpu/cpu0/cpufreq will not exist,
and the CPU frequency-based power management cannot be used.
Consult the relevant BIOS documentation to determine how these settings
can be accessed.

Host Operating System
~~~~~~~~~~~~~~~~~~~~~

The Host OS must also have the *apci_cpufreq* module installed, in some cases
the *intel_pstate* driver may be the default Power Management environment.
To enable *acpi_cpufreq* and disable *intel_pstate*, add the following
to the grub Linux command line:

.. code-block:: console

  intel_pstate=disable

Upon rebooting, load the *acpi_cpufreq* module:

.. code-block:: console

  modprobe acpi_cpufreq

Hypervisor Channel Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Virtio-Serial channels are configured via libvirt XML:


.. code-block:: xml

  <name>{vm_name}</name>
  <controller type='virtio-serial' index='0'>
    <address type='pci' domain='0x0000' bus='0x00' slot='0x06' function='0x0'/>
  </controller>
  <channel type='unix'>
    <source mode='bind' path='/tmp/powermonitor/{vm_name}.{channel_num}'/>
    <target type='virtio' name='virtio.serial.port.poweragent.{vm_channel_num}'/>
    <address type='virtio-serial' controller='0' bus='0' port='{N}'/>
  </channel>


Where a single controller of type *virtio-serial* is created and up to 32 channels
can be associated with a single controller and multiple controllers can be specified.
The convention is to use the name of the VM in the host path *{vm_name}* and
to increment *{channel_num}* for each channel, likewise the port value *{N}*
must be incremented for each channel.

Each channel on the host will appear in *path*, the directory */tmp/powermonitor/*
must first be created and given qemu permissions

.. code-block:: console

  mkdir /tmp/powermonitor/
  chown qemu:qemu /tmp/powermonitor

Note that files and directories within /tmp are generally removed upon
rebooting the host and the above steps may need to be carried out after each reboot.

The serial device as it appears on a VM is configured with the *target* element attribute *name*
and must be in the form of *virtio.serial.port.poweragent.{vm_channel_num}*,
where *vm_channel_num* is typically the lcore channel to be used in DPDK VM applications.

Each channel on a VM will be present at */dev/virtio-ports/virtio.serial.port.poweragent.{vm_channel_num}*

Compiling and Running the Host Application
------------------------------------------

Compiling
~~~~~~~~~

#. export RTE_SDK=/path/to/rte_sdk
#. cd ${RTE_SDK}/examples/vm_power_manager
#. make

Running
~~~~~~~

The application does not have any specific command line options other than *EAL*:

.. code-block:: console

 ./build/vm_power_mgr [EAL options]

The application requires exactly two cores to run, one core is dedicated to the CLI,
while the other is dedicated to the channel endpoint monitor, for example to run
on cores 0 & 1 on a system with 4 memory channels:

.. code-block:: console

 ./build/vm_power_mgr -l 0-1 -n 4

After successful initialization the user is presented with VM Power Manager CLI:

.. code-block:: console

  vm_power>

Virtual Machines can now be added to the VM Power Manager:

.. code-block:: console

  vm_power> add_vm {vm_name}

When a {vm_name} is specified with the *add_vm* command a lookup is performed
with libvirt to ensure that the VM exists, {vm_name} is used as an unique identifier
to associate channels with a particular VM and for executing operations on a VM within the CLI.
VMs do not have to be running in order to add them.

A number of commands can be issued via the CLI in relation to VMs:

  Remove a Virtual Machine identified by {vm_name} from the VM Power Manager.

  .. code-block:: console

    rm_vm {vm_name}

  Add communication channels for the specified VM, the virtio channels must be enabled
  in the VM configuration(qemu/libvirt) and the associated VM must be active.
  {list} is a comma-separated list of channel numbers to add, using the keyword 'all'
  will attempt to add all channels for the VM:

  .. code-block:: console

    add_channels {vm_name} {list}|all

  Enable or disable the communication channels in {list}(comma-separated)
  for the specified VM, alternatively list can be replaced with keyword 'all'.
  Disabled channels will still receive packets on the host, however the commands
  they specify will be ignored. Set status to 'enabled' to begin processing requests again:

  .. code-block:: console

    set_channel_status {vm_name} {list}|all enabled|disabled

  Print to the CLI the information on the specified VM, the information
  lists the number of vCPUS, the pinning to pCPU(s) as a bit mask, along with
  any communication channels associated with each VM, along with the status of each channel:

  .. code-block:: console

    show_vm {vm_name}

  Set the binding of Virtual CPU on VM with name {vm_name}  to the Physical CPU mask:

  .. code-block:: console

    set_pcpu_mask {vm_name} {vcpu} {pcpu}

  Set the binding of Virtual CPU on VM to the Physical CPU:

  .. code-block:: console

    set_pcpu {vm_name} {vcpu} {pcpu}

Manual control and inspection can also be carried in relation CPU frequency scaling:

  Get the current frequency for each core specified in the mask:

  .. code-block:: console

    show_cpu_freq_mask {mask}

  Set the current frequency for the cores specified in {core_mask} by scaling each up/down/min/max:

  .. code-block:: console

    set_cpu_freq {core_mask} up|down|min|max

  Get the current frequency for the specified core:

  .. code-block:: console

    show_cpu_freq {core_num}

  Set the current frequency for the specified core by scaling up/down/min/max:

  .. code-block:: console

    set_cpu_freq {core_num} up|down|min|max

Compiling and Running the Guest Applications
--------------------------------------------

For compiling and running l3fwd-power, see :doc:`l3_forward_power_man`.

A guest CLI is also provided for validating the setup.

For both l3fwd-power and guest CLI, the channels for the VM must be monitored by the
host application using the *add_channels* command on the host.

Compiling
~~~~~~~~~

#. export RTE_SDK=/path/to/rte_sdk
#. cd ${RTE_SDK}/examples/vm_power_manager/guest_cli
#. make

Running
~~~~~~~

The application does not have any specific command line options other than *EAL*:

.. code-block:: console

 ./build/vm_power_mgr [EAL options]

The application for example purposes uses a channel for each lcore enabled,
for example to run on cores 0,1,2,3 on a system with 4 memory channels:

.. code-block:: console

 ./build/guest_vm_power_mgr -l 0-3 -n 4


After successful initialization the user is presented with VM Power Manager Guest CLI:

.. code-block:: console

  vm_power(guest)>

To change the frequency of a lcore, use the set_cpu_freq command.
Where {core_num} is the lcore and channel to change frequency by scaling up/down/min/max.

.. code-block:: console

  set_cpu_freq {core_num} up|down|min|max
